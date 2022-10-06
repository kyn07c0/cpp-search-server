#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view text)
{
    std::vector<std::string_view> words;
    const size_t pos_end = text.npos;
    size_t space = text.find(' ');

    while(space != pos_end)
    {
        words.push_back(text.substr(0, space));
        text.remove_prefix(space + 1);
        space = text.find(' ');
    }
    words.push_back(text.substr(0, space));

    return words;
}
