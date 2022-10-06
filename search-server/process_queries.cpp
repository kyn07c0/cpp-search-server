#include "process_queries.h"
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> documents_lists(queries.size());

    transform(std::execution::par, queries.begin(), queries.end(), documents_lists.begin(), [&search_server](const std::string& query){
        return search_server.FindTopDocuments(query);
    });

    return documents_lists;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<Document> result;

    for(auto& docs : ProcessQueries(search_server, queries))
    {
        result.insert(result.end(), docs.begin(), docs.end());
    }

    return result;
}
