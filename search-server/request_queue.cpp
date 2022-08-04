#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_(search_server), query_num(1)
{
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status)
{
    std::vector<Document> documents = search_server_.FindTopDocuments(raw_query, status);
    AddResult(documents.size());

    return documents;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query)
{
    std::vector<Document> documents = search_server_.FindTopDocuments(raw_query);
    AddResult(documents.size());

    return documents;
}

int RequestQueue::GetNoResultRequests() const
{
    int cnt = count_if(requests_.begin(), requests_.end(), [](QueryResult qr){
        return qr.result_count == 0;
    });

    return cnt;
}

void RequestQueue::AddResult(int result_num)
{
    query_num++;
    requests_.push_back({query_num, result_num});

    if(requests_.size() > min_in_day_)
    {
        requests_.pop_front();
    }
}