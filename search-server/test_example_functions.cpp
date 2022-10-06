#include "test_example_functions.h"

using namespace std::literals;

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& items)
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
std::ostream& operator<<(std::ostream& out, const std::set<T>& items)
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
std::ostream& operator<<(std::ostream& out, const std::map<T, U>& items)
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

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
                unsigned line, const std::string& hint)
{
    if (!value)
    {
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty())
        {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint)
{
    if (t != u)
    {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;

        std::cout << t << " != "s << u << "."s;
        if (!hint.empty())
        {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

void TestAddDocumentContent()
{
    const int doc_id = 41;
    const std::string content = "This book has coding and programming interview questions "
                                "that will give you an idea of the nature of responses required "
                                "to leave an impact in your IT interview."s;
    const std::vector<int> ratings = {4, 4, 5};

    try
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
    catch(std::invalid_argument const& ex)
    {
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

    server.AddDocument(4, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(5, "cat  in  the city"s, DocumentStatus::BANNED, {1, 2, 3});
    ASSERT_EQUAL(server.GetDocumentCount(), 6);
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(2, "cat out of town"s, DocumentStatus::ACTUAL, {3, 2, 1});
        std::vector<Document> found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(2, "cat out of town"s, DocumentStatus::ACTUAL, {3, 2, 1});
        std::vector<Document> found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.empty());
    }

    // Поиск слова в двух одинаковах по содержанию документах, но с разным статусом
    {
        SearchServer server;
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(2, "cat  in  the city"s, DocumentStatus::BANNED, {1, 2, 3});
        std::vector<Document> found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1);
    }
}

// Тест проверяет, что если документ содержит минус-слово, он исключается из результата поиска
void TestExcludingDocumentWithNegativeWordFromQueryResult()
{
    const std::string content = "Big cat in the Saint-Petersburg city"s;
    const std::vector<int> ratings = {4, 4, 5};

    {
        SearchServer server;
        server.AddDocument(1, content, DocumentStatus::ACTUAL, ratings);

        std::vector<Document> found_docs_1 = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs_1.size(), 1);

        std::vector<Document> found_docs_2 = server.FindTopDocuments("cat -city"s);
        ASSERT(found_docs_2.empty());

        std::vector<Document> found_docs_3 = server.FindTopDocuments("cat -City -Big"s);
        ASSERT(found_docs_3.empty());
    }
}

// Тест проверяет работу матчинга документов
void TestMatchDocument()
{
    const std::string content = "cat in the city"s;
    const std::string raw_query = "car";
    const std::string raw_query_with_minus_words = "book -nature";
    const std::vector<int> ratings = {2, 4, 6};
    const std::string stop_words = "book"s;

    // Проверка выполнения поискового запроса
    {
        SearchServer server;
        server.AddDocument(1, "белый кот имел выразтельные глаза и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(2, "пушистый кот лизал пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(3, "домашний кот и его выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

        std::vector<std::string_view> matching_words;
        DocumentStatus document_status;

        tie(matching_words, document_status) = server.MatchDocument("кот енот"s, 1);
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "кот"s), 1);
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "енот"s), 0);

        tie(matching_words, document_status) = server.MatchDocument("глаза енот"s, 2);
        ASSERT(matching_words.empty());

        tie(matching_words, document_status) = server.MatchDocument("кот глаза"s, 3);
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "глаза"s), 1);
        ASSERT_EQUAL(count(matching_words.begin(), matching_words.end(), "кот"s), 1);

        tie(matching_words, document_status) = server.MatchDocument("кот -глаза"s, 3);
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

        std::vector<Document> found_docs = server.FindTopDocuments("кот пёс"s);

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

        std::vector<Document> found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);

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
    SearchServer server;

    server.AddDocument(1, "Есть преступление хуже, чем сжигать книги. Например - не читать их."s, DocumentStatus::ACTUAL, {0, 10, 0, 2, 3});
    int rating_doc_1 = (0 + 10 + 0 + 2 + 3) / 5;
    std::vector<Document> found_actual_docs = server.FindTopDocuments("книги читать"s);
    ASSERT_EQUAL(found_actual_docs[0].rating, rating_doc_1);

    server.AddDocument(2, "По вечерам я люблю читать книги."s, DocumentStatus::IRRELEVANT, {1, 12, 5});
    int rating_doc_2 = (1 + 12 + 5) / 3;
    std::vector<Document> found_irrelevant_docs = server.FindTopDocuments("книги читать"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(found_irrelevant_docs[0].rating, rating_doc_2);


    server.AddDocument(3, "На уроке литературы дети должны читать книги."s, DocumentStatus::BANNED, {6, 2, 1});
    int rating_doc_3 = (6 + 2 + 1) / 3;
    std::vector<Document> found_banned_docs = server.FindTopDocuments("книги читать"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_banned_docs[0].rating, rating_doc_3);
}

// Тест проверяет работу фильтра документов по статусу
void TestStatusFilter()
{
    const std::string content = "cat in the city"s;

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

    std::vector<Document> found_actual_docs = server.FindTopDocuments("city"s, DocumentStatus::ACTUAL);
    std::vector<Document> found_banned_docs = server.FindTopDocuments("city"s, DocumentStatus::BANNED);
    std::vector<Document> found_irrelevant_docs = server.FindTopDocuments("city"s, DocumentStatus::IRRELEVANT);
    std::vector<Document> found_remove_docs = server.FindTopDocuments("city"s, DocumentStatus::REMOVED);

    ASSERT_EQUAL(found_actual_docs.size(), 4);
    ASSERT_EQUAL(found_banned_docs.size(), 2);
    ASSERT_EQUAL(found_irrelevant_docs.size(), 2);
    ASSERT_EQUAL(found_remove_docs.size(), 1);
}

// Тест проверяет работу фильтра документов с использованием предиката
void TestPredicateFilter()
{
    const std::string content = "cat in the city"s;

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

    std::vector<Document> result_1 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return rating == 1; });
    ASSERT(result_1.size() == 1);

    std::vector<Document> result_2 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return rating < 3; });
    ASSERT(result_2.size() == 3);

    std::vector<Document> result_3 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED && rating < 4; });
    ASSERT(result_3.size() == 0);

    std::vector<Document> result_4 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return doc_id == 4; });
    ASSERT(result_4.size() == 1);

    std::vector<Document> result_5 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return rating > 2 && rating < 5; });
    ASSERT(result_5.size() == 3);

    std::vector<Document> result_6 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return status == DocumentStatus::IRRELEVANT && rating == 4; });
    ASSERT(result_6.size() == 2);

    std::vector<Document> result_7 = server.FindTopDocuments("city"s, [](int doc_id, DocumentStatus status, int rating) { return doc_id == 4; });
    ASSERT(result_7.size() == 1);
}


template <typename T>
void RunTestImpl(T func, const std::string& func_name)
{
    func();
    std::cerr << func_name << " OK" << std::endl;
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

    std::cout << "Search server testing finished"s << std::endl;
}
