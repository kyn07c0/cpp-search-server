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

int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
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
    void SetStopWords(const string& text)
    {
        for(const string& word : SplitIntoWords(text))
        {
            stop_words.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
    {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1 / (double)words.size();

        for(const string& word : words)
        {
            word_to_document_freqs[word][document_id] += inv_word_count;
        }

        documents.emplace(document_id,
                          DocumentData {ComputeAverageRating(ratings), status});
    }

    int GetDocumentCount() const
    {
        return documents.size();
    }

    template<typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter filter) const
    {
        const Query query = ParseQuery(raw_query);
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_) const
    {
        return FindTopDocuments(raw_query,
                                [&status_](int document_id, DocumentStatus status, int rating)
                                {
                                    return status == status_;
                                });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
    {
        const Query query = ParseQuery(raw_query);
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

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words;
    map<string, map<int, double>> word_to_document_freqs;
    map<int, DocumentData> documents;

    bool IsStopWord(const string& word) const
    {
        return stop_words.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const
    {
        vector<string> words;
        for(const string& word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
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
        for (const string& word : SplitIntoWords(text))
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
            matched_documents.push_back({ document_id,
                                          relevance,
                                          documents.at(document_id).rating });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document)
{
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}