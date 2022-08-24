#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server)
{
    std::set<int> document_id_for_del;

    for(const int document_id : search_server)
    {
        const std::map<std::string, double> word_freq = search_server.GetWordFrequencies(document_id);

        for(const int next_document_id : search_server)
        {
            if(next_document_id > document_id)
            {
                std::set<std::string> words_current_doc;
                for(const auto& [word, freq] : search_server.GetWordFrequencies(document_id))
                {
                    words_current_doc.insert(word);
                }

                std::set<std::string> words_next_doc;
                for(const auto& [word, freq] : search_server.GetWordFrequencies(next_document_id))
                {
                    words_next_doc.insert(word);
                }

                if(words_current_doc == words_next_doc)
                {
                    document_id_for_del.insert(next_document_id);
                }
            }
        }
    }

    for(int document_id : document_id_for_del)
    {
        std::cout << "Found duplicate document id " << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}
