#include "search_server.h"

using namespace std::literals;

SearchServer::SearchServer()
{
}

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(std::string_view(stop_words_text))
{
}

SearchServer::SearchServer(std::string_view stop_words_text) : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings)
{
    ValidateNewDocument(document_id, document);

    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for(std::string_view word : words)
    {
        auto it = words_.insert(static_cast<std::string>(word));
        word_to_document_freqs_[*it.first][document_id] += inv_word_count;
        document_word_freqs_[document_id][*it.first] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

void SearchServer::SetStopWords(std::string_view stop_words_text)
{
    for(std::string_view word : SplitIntoWords(stop_words_text))
    {
        stop_words_.insert(static_cast<std::string>(word));
    }
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
    {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
    {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
    {
        return document_status == status;
    });
}

int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    ValidateDocumentIndex(document_id);

    return document_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id)
{
    ValidateDocumentIndex(document_id);

    for(auto& [word, freqs] : document_word_freqs_.at(document_id))
    {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id)
{
    ValidateDocumentIndex(document_id);

    auto word_freqs = document_word_freqs_.at(document_id);
    std::vector<std::string_view> words;
    words.reserve(word_freqs.size());

    transform(std::execution::par, word_freqs.begin(), word_freqs.end(), words.begin(), [](const auto item) {
        return item.first;
    });

    for_each(std::execution::par, words.begin(), words.end(), [this, document_id](const auto& word) {
        word_to_document_freqs_.at(word).erase(document_id);
    });

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_word_freqs_.erase(document_id);
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const
{
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const
{
    Query query = ParseQuery(raw_query);

    std::sort(query.minus_words.begin(), query.minus_words.end());
    auto last_minus = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last_minus, query.minus_words.end());

    std::sort(query.plus_words.begin(), query.plus_words.end());
    auto last_plus = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(last_plus, query.plus_words.end());

    std::vector<std::string_view> matched_words;

    for(std::string_view word : query.plus_words)
    {
        if(document_word_freqs_.at(document_id).count(word) > 0)
        {
            const auto it = word_to_document_freqs_.find(word);
            if (it->first == word)
                matched_words.push_back(it->first);
        }
    }

    for(std::string_view word : query.minus_words)
    {
        if(document_word_freqs_.at(document_id).count(word) > 0)
        {
            matched_words.clear();
            break;
        }
    }

    return {matched_words, documents_.at(document_id).status};
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const
{
    ValidateDocumentIndex(document_id);

    Query query = ParseQuery(std::execution::par, raw_query);

    std::sort(query.minus_words.begin(), query.minus_words.end());
    auto last_minus = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last_minus, query.minus_words.end());

    const auto pred = [this, document_id](const std::string_view word) {
                return word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id);
            };

    if(any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), pred))
    {
        return { {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    auto matched_copy = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [this, document_id](std::string_view word) {
        return (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id));
    });

    std::sort(matched_words.begin(), matched_copy);
    auto last_matched = std::unique(matched_words.begin(), matched_copy);
    matched_words.erase(last_matched, matched_words.end());

    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(std::string_view word) const
{
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text)
{
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }

    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    if (ratings.empty())
    {
        return 0;
    }
    
    int rating_sum = 0;
    for (const int rating : ratings)
    {
        rating_sum += rating;
    }

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view word) const
{
    if(word.empty())
    {
        throw std::invalid_argument("Query word is empty"s);
    }

    ValidateWordQuery(word);

    bool is_minus = false;

    if(word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }

    if(word.empty() || word[0] == '-' || !IsValidWord(word))
    {
        throw std::invalid_argument("Query word "s + std::string(word) + " is invalid"s);
    }

    return { word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const
{
    return ParseQuery(std::execution::seq, text);
}

SearchServer::Query SearchServer::ParseQuery(const std::execution::sequenced_policy&, std::string_view text) const
{
    std::vector<std::string_view> words = SplitIntoWords(text);

    Query result;
    result.minus_words.reserve(words.size());
    result.plus_words.reserve(words.size());

    for(std::string_view word : SplitIntoWords(text))
    {
        const auto query_word = ParseQueryWord(word);

        if(!query_word.is_stop)
        {
            if(query_word.is_minus)
            {
                result.minus_words.push_back(query_word.data);
            }
            else
            {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    return result;
}

SearchServer::Query SearchServer::ParseQuery(const std::execution::parallel_policy&, std::string_view text) const
{
    std::vector<std::string_view> words = SplitIntoWords(text);

    Query result;
    result.minus_words.reserve(words.size());
    result.plus_words.reserve(words.size());

    std::vector<QueryWord> query_words(words.size());
    std::transform(std::execution::par, words.begin(), words.end(), query_words.begin(), [this](std::string_view word){
        return ParseQueryWord(word);
    });

    for(QueryWord& query_word : query_words)
    {
        if(!query_word.is_stop)
        {
            if(query_word.is_minus)
            {
                result.minus_words.push_back(query_word.data);
            }
            else
            {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(std::string_view word)
{
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

bool SearchServer::IsValidSearchMinusWord(std::string_view word)
{
    std::string_view prefix_word = word.substr(0, 2);

    if(prefix_word == "-" || prefix_word == "--")
    {
        return false;
    }

    return true;
}

void SearchServer::ValidateStopWord(std::string_view stop_word)
{
    if(!IsValidWord(stop_word))
    {
        throw std::invalid_argument("Стоп-слово содержит недопустимый символ.");
    }
}

void SearchServer::ValidateNewDocument(const int document_id, std::string_view document) const
{
    if(document_id < 0)
    {
        throw std::invalid_argument("Попытка добавить документ с отрицательным id.");
    }
    else if(documents_.find(document_id) != documents_.end())
    {
        throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа.");
    }
    else if(!IsValidWord(document))
    {
        throw std::invalid_argument("Наличие недопустимых символов в тексте добавляемого документа.");
    }
}

void SearchServer::ValidateWordQuery(std::string_view word) const
{
    if(!IsValidWord(word))
    {
        throw std::invalid_argument("Наличие недопустимых символов в слове поискового запроса.");
    }
    else if(!IsValidSearchMinusWord(word))
    {
        throw std::invalid_argument("Некорректно указано исключающее слово поискового запроса.");
    }
}

void SearchServer::ValidateDocumentIndex(const size_t index) const
{
    if(index >= documents_.size())
    {
        throw std::out_of_range("Индекс документа выходит за пределы допустимого диапазона.");
    }
}
