#include "search_server.h"

SearchServer::SearchServer()
{
}

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings)
{
    ValidateNewDocument(document_id, document);

    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}

void SearchServer::SetStopWords(const std::string& stop_words_text)
{
    for(const std::string& word : SplitIntoWords(stop_words_text))
    {
        stop_words_.insert(word);
    }
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const
{
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating)
    {
        return document_status == status;
    });
}

int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}

int SearchServer::GetDocumentId(int index) const
{
    ValidateDocumentIndex(index);

    return document_ids_.at(index);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const
{
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;

    for(const std::string& word : query.plus_words)
    {
        if(word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }

        if(word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.push_back(word);
        }
    }

    for(const std::string& word : query.minus_words)
    {
        if(word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if(word_to_document_freqs_.at(word).count(document_id) != 0)
        {
            matched_words.clear();
            break;
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string& word) const
{
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const
{
    std::vector<std::string> words;
    for(const std::string& word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s + word + " is invalid"s);
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const
{
    std::string word = text;
    bool is_minus = false;

    if(word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const
{
    Query result;
    for (const std::string& word : SplitIntoWords(text))
    {
        ValidateQueryWord(word);

        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                result.minus_words.insert(query_word.data);
            }
            else
            {
                result.plus_words.insert(query_word.data);
            }
        }
    }

    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const std::string& word)
{
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

bool SearchServer::IsValidSearchMinusWord(const std::string& word)
{
    std::string prefix_word = word.substr(0, 2);

    if(prefix_word == "-" || prefix_word == "--")
    {
        return false;
    }

    return true;
}

void SearchServer::ValidateStopWord(const std::string stop_word)
{
    if(!IsValidWord(stop_word))
    {
        throw std::invalid_argument("Стоп-слово содержит недопустимый символ."s);
    }
}

void SearchServer::ValidateNewDocument(const int document_id, const std::string& document) const
{
    if(document_id < 0)
    {
        throw std::invalid_argument("Попытка добавить документ с отрицательным id."s);
    }

    if(documents_.find(document_id) != documents_.end())
    {
        throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа."s);
    }

    if(!IsValidWord(document))
    {
        throw std::invalid_argument("Наличие недопустимых символов в тексте добавляемого документа."s);
    }
}

void SearchServer::ValidateQueryWord(const std::string word) const
{
    if(word.empty())
    {
        throw std::invalid_argument("Слово запроса пустое"s);
    }

    if(!IsValidWord(word))
    {
        throw std::invalid_argument("Наличие недопустимых символов в слове поискового запроса."s);
    }

    if(!IsValidSearchMinusWord(word))
    {
        throw std::invalid_argument("Некорректно указано исключающее слово поискового запроса."s);
    }
}

void SearchServer::ValidateDocumentIndex(const int index) const
{
    if(index >= documents_.size())
    {
        throw std::out_of_range("Индекс документа выходит за пределы допустимого диапазона."s);
    }
}
