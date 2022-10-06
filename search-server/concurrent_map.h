#pragma once

#include <mutex>

template <typename Key, typename Value>
class ConcurrentMap
{
    private:
        struct SubMap
        {
            std::mutex mutex;
            std::map<Key, Value> map;
        };

        std::vector<SubMap> sub_maps_;

    public:
        static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

        explicit ConcurrentMap(int sub_maps_num) : sub_maps_(sub_maps_num)
        {
        }

        struct Access
        {
            std::lock_guard<std::mutex> guard;
            Value& ref_to_value;

            Access(const Key& key, SubMap& sub_map) : guard(sub_map.mutex), ref_to_value(sub_map.map[key])
            {
            }
        };

        Access operator[](const Key& key)
        {
            auto& sub_map = sub_maps_[static_cast<uint64_t>(key) % sub_maps_.size()];
            return {key, sub_map};
        }

        std::map<Key, Value> BuildOrdinaryMap()
        {
            std::map<Key, Value> result;

            for(auto& [mutex, sub_map] : sub_maps_)
            {
                std::lock_guard lg(mutex);
                result.insert(sub_map.begin(), sub_map.end());
            }

            return result;
        }
};
