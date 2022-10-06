#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <execution>
#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double COMPARISON_ERROR = 1e-6;
const int NUMBER_PARTS_PARALLEL_MAP = 7;

class SearchServer
{
    public:
        explicit SearchServer();

        template <typename StringContainer>
        explicit SearchServer(const StringContainer& stop_words);

        explicit SearchServer(const std::string& stop_words_text);
        explicit SearchServer(std::string_view stop_words_text);

        void SetStopWords(std::string_view stop_words_text);

        void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

        template <typename DocumentPredicate>
        std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
        template <typename DocumentPredicate>
        std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;
        template <typename DocumentPredicate>
        std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;

        std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
        std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const;
        std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const;

        std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
        std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;
        std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const;

        int GetDocumentCount() const;

        std::set<int>::const_iterator begin() const;
        std::set<int>::const_iterator end() const;

        const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

        void RemoveDocument(int document_id);
        void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
        void RemoveDocument(const std::execution::parallel_policy&, int document_id);

        std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
        std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;
        std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;

    private:
        struct DocumentData
        {
            int rating;
            DocumentStatus status;
        };

        std::set<std::string, std::less<>> words_;
        std::set<std::string, std::less<>> stop_words_;
        std::map<std::string_view , std::map<int, double>> word_to_document_freqs_;
        std::map<int, std::map<std::string_view, double>> document_word_freqs_;
        std::map<int, DocumentData> documents_;
        std::set<int> document_ids_;

        bool IsStopWord(std::string_view word) const;
        std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text);
        static int ComputeAverageRating(const std::vector<int>& ratings);

        struct QueryWord
        {
            std::string_view data;
            bool is_minus;
            bool is_stop;
        };

        QueryWord ParseQueryWord(std::string_view text) const;

        struct Query
        {
            std::vector<std::string_view> plus_words;
            std::vector<std::string_view> minus_words;
        };

        Query ParseQuery(std::string_view text) const;
        Query ParseQuery(const std::execution::sequenced_policy&, std::string_view text) const;
        Query ParseQuery(const std::execution::parallel_policy&, std::string_view text) const;

        double ComputeWordInverseDocumentFreq(std::string_view word) const;

        template <typename DocumentPredicate>
        std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
        template <typename DocumentPredicate>
        std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
        template <typename DocumentPredicate>
        std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;

        static bool IsValidWord(std::string_view word);

        static bool IsValidSearchMinusWord(std::string_view word);
        void ValidateStopWord(std::string_view stop_word);
        void ValidateNewDocument(const int document_id, std::string_view document) const;
        void ValidateWordQuery(std::string_view word) const;
        void ValidateDocumentIndex(const size_t index) const;
};


template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    for(std::string_view word : stop_words)
    {
        ValidateStopWord(word);
        stop_words_.insert(static_cast<std::string>(word));
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const
{
    Query query = ParseQuery(raw_query);

    std::sort(query.minus_words.begin(), query.minus_words.end());
    auto last_minus = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last_minus, query.minus_words.end());

    std::sort(query.plus_words.begin(), query.plus_words.end());
    auto last_plus = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(last_plus, query.plus_words.end());

    auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs)
    {
        if (std::abs(lhs.relevance - rhs.relevance) < COMPARISON_ERROR)
        {
            return lhs.rating > rhs.rating;
        }
        else
        {
            return lhs.relevance > rhs.relevance;
        }
    });

    if(matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const
{
    Query query = ParseQuery(std::execution::par, raw_query);

    std::sort(query.minus_words.begin(), query.minus_words.end());
    auto last_minus = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last_minus, query.minus_words.end());

    std::sort(query.plus_words.begin(), query.plus_words.end());
    auto last_plus = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(last_plus, query.plus_words.end());

    auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs)
    {
        if (std::abs(lhs.relevance - rhs.relevance) < COMPARISON_ERROR)
        {
            return lhs.rating > rhs.rating;
        }
        else
        {
            return lhs.relevance > rhs.relevance;
        }
    });

    if(matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const
{
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;

    for(std::string_view word : query.plus_words)
    {
        if(word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }

        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

        for(const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto& document_data = documents_.at(document_id);
            if(document_predicate(document_id, document_data.status, document_data.rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for(std::string_view word : query.minus_words)
    {
        if(word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }

        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for(const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> cm_document_to_relevance(NUMBER_PARTS_PARALLEL_MAP);

    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [this, &document_predicate, &cm_document_to_relevance](std::string_view word){
        if(word_to_document_freqs_.count(word) == 0)
        {
            return;
        }

        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

        for(const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto& document_data = documents_.at(document_id);
            if(document_predicate(document_id, document_data.status, document_data.rating))
            {
                cm_document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    });

    std::map<int, double> document_to_relevance = cm_document_to_relevance.BuildOrdinaryMap();

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance](std::string_view word){
        if(word_to_document_freqs_.count(word) == 0)
        {
            return;
        }

        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    });

    std::vector<Document> matched_documents;
    for(const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }

    return matched_documents;
}
