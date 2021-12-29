#include "read_cache.hpp"

namespace Hill {
    namespace ReadCache {
        auto Cache::get(const std::string &key) -> const CacheItem * {
            auto time = std::chrono::steady_clock::now();
            auto ret = map.find(key);
            if (ret == map.end()) {
                ++accessed;
                return nullptr;
            }

            if (time > (*ret->second)->expire) {
                auto iter = ret->second;
                list.erase(iter);
                map.erase(key);
                --load;
                ++accessed;
                return nullptr;
            }

            list.splice(list.begin(), list, ret->second);
            ret->second = list.begin();
            ++accessed;
            ++hit;
            return (*ret->second).get();
        }

        auto Cache::insert(const std::string &key, const PolymorphicPointer &value, size_t sz) -> void {
            if (load == capacity) {
                auto &item = list.back();
                map.erase(item->key);
                list.pop_back();
                --load;
            }

            auto item = CacheItem::make_cache_item(key, value, sz);
            list.push_front(std::move(item));
            map.insert({key, list.begin()});
            ++load;
        }

        auto Cache::expire(const std::string &key) -> void {
            auto ret = map.find(key);
            if (ret == map.end()) {
                return;                
            }

            (*ret->second)->expire += Constants::tLEASE;
        }
    }
}
