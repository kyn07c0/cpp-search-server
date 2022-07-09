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

// -------- Начало модульных тестов поисковой системы ----------

// Инструменты для проверки работы функций поисковой системы

template <typename T>
ostream& operator<<(ostream& out, const vector<T>& items)
{
    out << "[";
    int count = 0;
    int item_size = items.size();
    for(const auto& item : items)
    {
        out << item;
        if(count < items.size() - 1)
        {
            out << ", ";
        }
        ++count;
    }
    out << "]";
    return out;
}

template <typename T>
ostream& operator<<(ostream& out, const set<T>& items)
{
    out << "{";
    int count = 0;
    int item_size = items.size();
    for(const auto& item : items)
    {
        out << item;
        if(count < item_size - 1)
        {
            out << ", ";
        }
        ++count;
    }
    out << "}";
    return out;
}

template <typename T, typename U>
ostream& operator<<(ostream& out, const map<T, U>& items)
{
    out << "{";
    int count = 0;
    int item_size = items.size();
    for(const auto& [key, value] : items)
    {
        out << key << ": " << value;
        if(count < item_size - 1)
        {
            out << ", ";
        }
        ++count;
    }
    out << "}";
    return out;
}

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func,
                unsigned line, const string& hint)
{
    if (!value)
    {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint)
{
    if (t != u)
    {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;

        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Тест проверяет, что поисковая система нахдит документ по искомому слову
void TestAddDocumentContent()
{
    const int doc_id = 41;
    const string content = "This book has coding and programming interview questions "
                           "that will give you an idea of the nature of responses required "
                           "to leave an impact in your IT interview."s;
    const vector<int> ratings = {4, 4, 5};

    {
        // Убеждаемся, что после создании экземпляра поискового сервера в нем отсутствуют документы
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);

        // Проверяем, что после добавления одного документа их общее кол-во прирасло на один документ
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);

        // Проверяем, что при попытке добавить документ с существующим id он добавлен не будет
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
    }
}

// Получение общего числа документов
void TestCountAllDocuments()
{
    SearchServer server;
    server.AddDocument(0, "белый кот имел выразтельные глаза и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот лизал пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "домашний кот и его выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
    ASSERT_EQUAL(server.GetDocumentCount(), 4);

    server.AddDocument(4, "cat in the city", DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(5, "cat  in  the city", DocumentStatus::BANNED, {1, 2, 3});
    ASSERT_EQUAL(server.GetDocumentCount(), 6);
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(1, "cat in the city", DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(2, "cat out of town", DocumentStatus::ACTUAL, {3, 2, 1});
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city", DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(2, "cat out of town", DocumentStatus::ACTUAL, {3, 2, 1});
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
    // Поиск слова, в двух одинаковах документах
    {
        SearchServer server;
        server.AddDocument(1, "cat in the city", DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(2, "cat  in  the city", DocumentStatus::BANNED, {1, 2, 3});
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1);
    }
}

// Тест проверяет, что если документ содержит минус-слово, он исключается из результата поиска
void TestExcludingDocumentWithNegativeWordFromQueryResult()
{
    const string content = "Big cat in the Saint-Petersburg city"s;
    const vector<int> ratings = {4, 4, 5};

    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs_1 = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs_1.size(), 1);

        const auto found_docs_2 = server.FindTopDocuments("cat -city"s);
        ASSERT(found_docs_2.empty());

        const auto found_docs_3 = server.FindTopDocuments("cat -City -Big"s);
        ASSERT(found_docs_3.empty());
    }
}

// Тест проверяет работу матчинга документов
void TestMatchDocument()
{
    const string content = "cat in the city"s;
    const string raw_query = "car";
    const string raw_query_with_minus_words = "book -nature";
    const vector<int> ratings = {2, 4, 6};
    const string stop_words = "book"s;

    // Проверка выполнения поискового запроса
    {
        SearchServer server;
        server.AddDocument(1, "белый кот имел выразтельные глаза и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "пушистый кот лизал пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(3, "домашний кот и его выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

        vector<string> matching_words = {};
        DocumentStatus document_status;

        tuple<vector<string>, DocumentStatus> result_1 = server.MatchDocument("кот енот", 1);
        tie(matching_words, document_status) = result_1;
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "кот"), 1);
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "енот"), 0);

        tuple<vector<string>, DocumentStatus> result_2 = server.MatchDocument("глаза енот", 2);
        tie(matching_words, document_status) = result_2;
        ASSERT(matching_words.empty());

        tuple<vector<string>, DocumentStatus> result_3 = server.MatchDocument("кот глаза", 3);
        tie(matching_words, document_status) = result_3;
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "глаза"), 1);
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "кот"), 1);

        tuple<vector<string>, DocumentStatus> result_4 = server.MatchDocument("кот -глаза", 3);
        tie(matching_words, document_status) = result_4;
        ASSERT(matching_words.empty());
    }
}

// Тест проверяет корректность вычисления релевантности
void TestRelevanceCalculation()
{
    {
        SearchServer server;
        server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

        const auto found_docs = server.FindTopDocuments("кот пёс"s);

        double idf_cat = log(3.0/2.0);
        double idf_dog = log(3.0/1.0);

        double tf_cat_doc_1 = 1.0 / 5.0;
        double tf_dog_doc_1 = 0.0 / 5.0;
        double relevance_doc_1 = (idf_cat * tf_cat_doc_1) + (idf_dog * tf_dog_doc_1); // 0.081093021621632885

        double tf_cat_doc_2 = 1.0 / 4.0;
        double tf_dog_doc_2 = 0.0 / 4.0;
        double relevance_doc_2 = (idf_cat * tf_cat_doc_2) + (idf_dog * tf_dog_doc_2); // 0.1013662770270411

        double tf_cat_doc_3 = 0.0 / 4.0;
        double tf_dog_doc_3 = 1.0 / 4.0;
        double relevance_doc_3 = (idf_cat * tf_cat_doc_3) + (idf_dog * tf_dog_doc_3); // 0.27465307216702745

        ASSERT_EQUAL(found_docs[0].relevance, relevance_doc_3);
        ASSERT_EQUAL(found_docs[1].relevance, relevance_doc_2);
        ASSERT_EQUAL(found_docs[2].relevance, relevance_doc_1);
    }
}

// Тест проверяет, как найденные документы сортируются по релевантности
void TestRelevanceSorting()
{
    // Поиск по ключевым словам
    {
        SearchServer server;
        server.AddDocument(1, "белый кот имел выразтельные глаза и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "пушистый кот лизал пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(3, "домашний кот и его выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        server.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
        
        ASSERT_EQUAL(found_docs.size(), 3);

        ASSERT(found_docs[0].relevance > found_docs[1].relevance);
        ASSERT(found_docs[1].relevance > found_docs[2].relevance);

        ASSERT_EQUAL(found_docs[0].id, 2);
        ASSERT_EQUAL(found_docs[1].id, 3);
        ASSERT_EQUAL(found_docs[2].id, 1);
    }
}

// Тест проверяет корректность вычисления рейтинга документов
void TestRatingDocuments()
{
    const string content = "Есть преступление хуже, чем сжигать книги. Например - не читать их."s;

    {
        SearchServer server;
        server.AddDocument(1, "Есть преступление хуже, чем сжигать книги. Например - не читать их."s, DocumentStatus::ACTUAL, {0, 10, 0, 2, 3});
        server.AddDocument(2, "По вечерам я люблю читать книги."s, DocumentStatus::IRRELEVANT, {1, 12, 5});
        server.AddDocument(3, "На уроке литературы дети должны читать книги."s, DocumentStatus::BANNED, {6, 2, 1});

        int rating_doc_1 = (0 + 10 + 0 + 2 + 3) / 5;
        int rating_doc_2 = (1 + 12 + 5) / 3;
        int rating_doc_3 = (6 + 2 + 1) / 3;

        const auto found_actual_docs = server.FindTopDocuments("книги читать"s);
        ASSERT_EQUAL(found_actual_docs[0].rating, rating_doc_1);

        const auto found_irrelevant_docs = server.FindTopDocuments("книги читать"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_irrelevant_docs[0].rating, rating_doc_2);

        const auto found_banned_docs = server.FindTopDocuments("книги читать"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_banned_docs[0].rating, rating_doc_3);
    }
}

// Тест проверяет работу фильтра документов по статусу
void TestStatusFilter()
{
    const string content = "cat in the city"s;

    SearchServer server;
    server.AddDocument(1, content, DocumentStatus::ACTUAL, {8, 3, 9});
    server.AddDocument(2, content, DocumentStatus::ACTUAL, {3, 2, 2});
    server.AddDocument(3, content, DocumentStatus::ACTUAL, {4, 23, 1});
    server.AddDocument(4, content, DocumentStatus::ACTUAL, {5, 1, 3});
    server.AddDocument(5, content, DocumentStatus::BANNED, {4, 3, 9});
    server.AddDocument(6, content, DocumentStatus::BANNED, {1, 12, 2});
    server.AddDocument(7, content, DocumentStatus::IRRELEVANT, {6, 4, 2});
    server.AddDocument(8, content, DocumentStatus::IRRELEVANT, {5, 7, 1});
    server.AddDocument(9, content, DocumentStatus::REMOVED, {6, 5, 5});

    const auto found_actual_docs = server.FindTopDocuments("city"s, DocumentStatus::ACTUAL);
    const auto found_banned_docs = server.FindTopDocuments("city"s, DocumentStatus::BANNED);
    const auto found_irrelevant_docs = server.FindTopDocuments("city"s, DocumentStatus::IRRELEVANT);
    const auto found_remove_docs = server.FindTopDocuments("city"s, DocumentStatus::REMOVED);

    ASSERT_EQUAL(found_actual_docs.size(), 4);
    ASSERT_EQUAL(found_banned_docs.size(), 2);
    ASSERT_EQUAL(found_irrelevant_docs.size(), 2);
    ASSERT_EQUAL(found_remove_docs.size(), 1);
}

// Тест проверяет работу фильтра документов с использованием предиката
void TestPredicateFilter()
{
    const string content = "cat in the city"s;

    SearchServer server;
    server.AddDocument(1, content, DocumentStatus::ACTUAL, {8, 4, 9});              // rating = 7
    server.AddDocument(2, content, DocumentStatus::ACTUAL, {3, 2, 2});              // rating = 2
    server.AddDocument(3, content, DocumentStatus::ACTUAL, {4, 2, 0});              // rating = 2
    server.AddDocument(4, content, DocumentStatus::ACTUAL, {5, 1, 3});              // rating = 3
    server.AddDocument(5, content, DocumentStatus::BANNED, {4, 3, 8});              // rating = 5
    server.AddDocument(6, content, DocumentStatus::IRRELEVANT, {1, 12, 2});         // rating = 5
    server.AddDocument(7, content, DocumentStatus::IRRELEVANT, {6, 4, 2});          // rating = 4
    server.AddDocument(8, content, DocumentStatus::IRRELEVANT, {5, 6, 1, 5, 3});    // rating = 4
    server.AddDocument(9, content, DocumentStatus::REMOVED, {1, 1, 1});             // rating = 1

    ASSERT(server.FindTopDocuments("city"s, []([[maybe_unused]] int doc_id, [[maybe_unused]] DocumentStatus status, int rating) {
        return rating == 1; }).size() == 1);
    ASSERT(server.FindTopDocuments("city"s, []([[maybe_unused]] int doc_id, [[maybe_unused]] DocumentStatus status, int rating) {
        return rating < 3; }).size() == 3);
    ASSERT(server.FindTopDocuments("city"s, []([[maybe_unused]] int doc_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::BANNED && rating < 4; }).size() == 0);
    ASSERT(server.FindTopDocuments("city"s, [](int doc_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) {
        return doc_id == 4; }).size() == 1);
    ASSERT(server.FindTopDocuments("city"s, []([[maybe_unused]] int doc_id, [[maybe_unused]] DocumentStatus status, int rating) {
        return rating > 2 && rating < 5; }).size() == 3);
    ASSERT(server.FindTopDocuments("city"s, []([[maybe_unused]] int doc_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::IRRELEVANT && rating == 4; }).size() == 2);
    ASSERT(server.FindTopDocuments("city"s, [](int doc_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) {
        return doc_id == 4; }).size() == 1);
}

template <typename T>
void RunTestImpl(T func, const string& func_name)
{
    func();
    cerr << func_name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer()
{
    RUN_TEST(TestAddDocumentContent);
    RUN_TEST(TestCountAllDocuments);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludingDocumentWithNegativeWordFromQueryResult);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestRelevanceCalculation);
    RUN_TEST(TestRelevanceSorting);
    RUN_TEST(TestRatingDocuments);
    RUN_TEST(TestStatusFilter);
    RUN_TEST(TestPredicateFilter);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main()
{
    TestSearchServer();
    cout << "Search server testing finished"s << endl;

    return 0;
}
