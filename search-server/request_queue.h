#pragma once

#include "search_server.h"
#include "document.h"
#include <deque>

class RequestQueue
{
    public:
        explicit RequestQueue(const SearchServer& search_server);

        template <typename DocumentPredicate>
        std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate)
        {
            std::vector<Document> documents = search_server_.FindTopDocuments(raw_query, document_predicate);
            AddResult(documents.size());

            return documents;
        }

        std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
        std::vector<Document> AddFindRequest(const std::string& raw_query);
        int GetNoResultRequests() const;

    private:
        void AddResult(int result_num);

        struct QueryResult
        {
            int query_number;
            int result_count;
        };

        std::deque<QueryResult> requests_;
        const static int min_in_day_ = 1440;
        const SearchServer& search_server_;
        int query_num;
};