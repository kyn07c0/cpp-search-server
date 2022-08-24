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
#include "string_processing.h"
#include "document.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double COMPARISON_ERROR = 1e-6;

class SearchServer
{
    public:
        explicit SearchServer();

        template <typename StringContainer>
        explicit SearchServer(const StringContainer& stop_words);

        explicit SearchServer(const std::string& stop_words_text);

        void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
        void SetStopWords(const std::string& stop_words_text);

        template <typename DocumentPredicate>
        std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;
        std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
        std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

        int GetDocumentCount() const;

        std::set<int>::const_iterator begin() const;
        std::set<int>::const_iterator end() const;

        const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

        void RemoveDocument(int document_id);

        std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    private:
        struct DocumentData
        {
            int rating;
            DocumentStatus status;
        };

        std::set<std::string> stop_words_;
        std::map<int, std::map<std::string, double>> document_word_freqs_;
        std::map<int, DocumentData> documents_;
        std::set<int> document_ids_;

        bool IsStopWord(const std::string& word) const;
        std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
        static int ComputeAverageRating(const std::vector<int>& ratings);

        struct QueryWord
        {
            std::string data;
            bool is_minus;
            bool is_stop;
        };

        QueryWord ParseQueryWord(const std::string& text) const;

        struct Query
        {
            std::set<std::string> plus_words;
            std::set<std::string> minus_words;
        };

        Query ParseQuery(const std::string& text) const;
        double ComputeWordInverseDocumentFreq(const std::string& word) const;

        template <typename DocumentPredicate>
        std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

        static bool IsValidWord(const std::string& word);
        static bool IsValidSearchMinusWord(const std::string& word);
        void ValidateStopWord(const std::string stop_word);
        void ValidateNewDocument(const int document_id, const std::string& document) const;
        void ValidateWordQuery(const std::string word) const;
        void ValidateDocumentIndex(const int index) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
{
    for(const std::string& word : stop_words)
    {
        ValidateStopWord(word);
        stop_words_.insert(word);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, document_predicate);

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
    std::map<int, double> document_to_relevance;
    for(const std::string& word : query.plus_words)
    {
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

        for(const auto [document_id, word_freq] : document_word_freqs_)
        {
            const auto& document_data = documents_.at(document_id);
            if(document_predicate(document_id, document_data.status, document_data.rating) && word_freq.count(word) > 0)
            {
                 document_to_relevance[document_id] += word_freq.at(word) * inverse_document_freq;
            }
        }
    }

    for(const std::string& word : query.minus_words)
    {
        for(const auto [document_id, word_freq] : document_word_freqs_)
        {
            if(word_freq.count(word) > 0)
            {
                document_to_relevance.erase(document_id);
            }
        }
    }

    std::vector<Document> matched_documents;
    for(const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }

    return matched_documents;
}
