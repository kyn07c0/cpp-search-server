#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double COMPARISON_ERROR = 1e-6;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

vector<string> SplitIntoWords(const string& text)
{
    vector<string> words;
    string word;
    for(const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if(!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

struct Document
{
    int id;
    double relevance;
    int rating;
    
    Document() : id(0), relevance(0.0), rating(0)
    {
    }
    
    Document(int id_, double relevance_, int rating_) : id(id_), relevance(relevance_), rating(rating_)
    {
    }
};

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer
{
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words_)
    {
        for(const string& word : stop_words_)
        {
            ValidateStopWord(word);
            stop_words.insert(word);
        }
    }

    explicit SearchServer(const string& stop_words_text)
    {
        for(const string& word : SplitIntoWords(stop_words_text))
        {
            ValidateStopWord(word);
            stop_words.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
    {
        // Проверка нового документа
        ValidateNewDocument(document_id, document);

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1 / (double)words.size();

        for(const string& word : words)
        {
            word_to_document_freqs[word][document_id] += inv_word_count;
        }

        documents.emplace(document_id, DocumentData {ComputeAverageRating(ratings), status});
    }

    int GetDocumentCount() const
    {
        return documents.size();
    }

    template<typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter filter) const
    {
        // Проверка поискового запроса
        ValidateQuery(raw_query);

        Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, filter);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs)
             {
                 if(abs(lhs.relevance - rhs.relevance) < COMPARISON_ERROR)
                 {
                     return lhs.rating > rhs.rating;
                 }
                 else
                 {
                     return lhs.relevance > rhs.relevance;
                 }
             });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const
    {
        return FindTopDocuments(raw_query,
                                [](int document_id, DocumentStatus status, int rating)
                                {
                                    return status == DocumentStatus::ACTUAL;
                                });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const
    {
        return FindTopDocuments(raw_query,
                                [&status](int document_id, DocumentStatus status_, int rating)
                                {
                                    return status_ == status;
                                });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
    {
        // Проверка поискового запроса
        ValidateQuery(raw_query);

        Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for(const string& word : query.plus_words)
        {
            if (word_to_document_freqs.count(word) == 0)
            {
                continue;
            }
            if (word_to_document_freqs.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }

        for(const string& word : query.minus_words)
        {
            if(word_to_document_freqs.count(word) == 0)
            {
                continue;
            }
            if(word_to_document_freqs.at(word).count(document_id))
            {
                matched_words.clear();
                break;
            }
        }

        return {matched_words, documents.at(document_id).status};
    }

    int GetDocumentId(int index) const
    {
        // Проверка корректности индекса
        ValidateDocumentIndex(index);

        auto it = documents.begin();
        advance(it, index);
        return it->first;
    }

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words;
    map<string, map<int, double>> word_to_document_freqs;
    map<int, DocumentData> documents;

    [[nodiscard]] bool IsStopWord(const string& word) const
    {
        return stop_words.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const
    {
        vector<string> words;
        for(const string& word : SplitIntoWords(text))
        {
            if(!IsStopWord(word))
            {
                words.push_back(word);
            }
        }

        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings)
    {
        if(ratings.empty())
        {
            return 0;
        }

        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const
    {
        bool is_minus = false;

        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }

        return { text, is_minus, IsStopWord(text) };
    }

    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const
    {
        Query query;
        for(const string& word : SplitIntoWords(text))
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }

        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const
    {
        return log(GetDocumentCount() / (double)word_to_document_freqs.at(word).size());
    }

    template<typename Filter>
    vector<Document> FindAllDocuments(const Query& query, Filter& filter) const
    {
        map<int, double> document_to_relevance;
        for(const string& word : query.plus_words)
        {
            if(word_to_document_freqs.count(word) == 0)
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for(const auto [document_id, term_freq] : word_to_document_freqs.at(word))
            {
                if(filter(document_id, documents.at(document_id).status, documents.at(document_id).rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for(const string& word : query.minus_words)
        {
            if(word_to_document_freqs.count(word) == 0)
            {
                continue;
            }

            for(const auto [document_id, _] : word_to_document_freqs.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for(const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back({ document_id, relevance, documents.at(document_id).rating });
        }
        return matched_documents;
    }


    static bool IsValidWord(const string& word)
    {
        return none_of(word.begin(), word.end(), [](char c) { return c >= '\0' && c < ' '; });
    }

    static bool IsValidSearchMinusWord(const string& word)
    {
        if(word == "-" || (word[0] == '-' && word[1] == '-'))
        {
            return false;
        }

        return true;
    }

    void ValidateStopWord(const string stop_word)
    {
        if(!IsValidWord(stop_word))
        {
            throw invalid_argument("Стоп-слово содержит недопустимый символ.");
        }
    }

    void ValidateNewDocument(const int document_id, const string& document) const
    {
        if(document_id < 0)
        {
            throw invalid_argument("Попытка добавить документ с отрицательным id.");
        }

        if(documents.find(document_id) != documents.end())
        {
            throw invalid_argument("Попытка добавить документ c id ранее добавленного документа.");
        }

        if(!IsValidWord(document))
        {
            throw invalid_argument("Наличие недопустимых символов в тексте добавляемого документа.");
        }
    }

    void ValidateQuery(const string raw_query) const
    {
        for(const string& word_query : SplitIntoWords(raw_query))
        {
            if(!IsValidWord(raw_query))
            {
                throw invalid_argument("Наличие недопустимых символов в слове поискового запроса.");
            }

            if(!IsValidSearchMinusWord(word_query))
            {
                throw invalid_argument("Некорректно указано исключающее слово поискового запроса.");
            }
        }
    }

    void ValidateDocumentIndex(const int index) const
    {
        if(index > documents.size() - 1)
        {
            throw out_of_range("Индекс документа выходит за пределы допустимого диапазона.");
        }
    }
};

// ------------ Пример использования ----------------

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}
