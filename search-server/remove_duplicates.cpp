#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server)
{
    std::set<int> document_id_for_del;
    std::set<std::set<std::string>> unique_word_sets;

    for(const int document_id : search_server)
    {
        std::set<std::string> word_set;
        for(auto [word, freq] : search_server.GetWordFrequencies(document_id))
        {
            word_set.insert(word);
        }

        if(unique_word_sets.count(word_set) != 0)
        {
            document_id_for_del.insert(document_id);
        }
        else
        {
            unique_word_sets.insert(word_set);
        }
    }

    for(int document_id : document_id_for_del)
    {
        std::cout << "Found duplicate document id " << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}
