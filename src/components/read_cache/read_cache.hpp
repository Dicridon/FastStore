#ifndef __HILL__READ_CACHE__READ_CACHE__
#define __HILL__READ_CACHE__READ_CACHE__

#include "memory_manager/memory_manager.hpp"
#include "kv_pair/kv_pair.hpp"
#include "remote_memory/remote_memory.hpp"
#include "config/config.hpp"

#include <unordered_map>
#include <list>
#include <chrono>
using namespace std::literals;

namespace Hill {
    namespace ReadCache {
        using namespace ::Hill::Memory;

        namespace Constants {
#ifdef __HILL_DEBUG__
            constexpr size_t uCACHE_SIZE = 100UL;
#else
            constexpr size_t uCACHE_SIZE = 1000000UL;
#endif
        }

        struct CacheItem {
            std::string key;
            PolymorphicPointer value_ptr;
            size_t value_size;
            std::chrono::time_point<std::chrono::steady_clock> expire;

            CacheItem() = default;
            CacheItem(const std::string &k, const PolymorphicPointer &ptr, size_t sz) : key(k), value_ptr(ptr), value_size(sz) {
                expire = std::chrono::steady_clock::now() + 2s;
            };
            ~CacheItem() = default;
            CacheItem(const CacheItem &) = default;
            CacheItem(CacheItem &&)= default;
            auto operator=(const CacheItem &) -> CacheItem & = default;
            auto operator=(CacheItem &&) -> CacheItem & = default;


            static auto make_cache_item(const std::string &key, const PolymorphicPointer &ptr, size_t sz)
                -> std::unique_ptr<CacheItem>
            {
                return std::make_unique<CacheItem>(key, ptr, sz);
            }
        };

        class Cache {
        public:
            Cache(size_t cache_cap) : load(0), capacity(cache_cap), hit(0), accessed(0) {};
            Cache() = default;
            ~Cache() = default;
            Cache(const Cache &) = default;
            Cache(Cache &&)= default;
            auto operator=(const Cache &) -> Cache & = default;
            auto operator=(Cache &&) -> Cache & = default;

            auto get(const std::string &key) -> const CacheItem *;
            auto insert(const std::string &key, const PolymorphicPointer &value, size_t sz) -> void;

            inline auto hit_ratio() const noexcept -> double {
                return double(hit) / accessed;
            }

            auto dump() const noexcept -> void {
                std::cout << "Cache is\n";
                for (const auto &i : list) {
                    std::cout << i->key << " ";
                }
                std::cout << "\n";
            }

        private:
            std::unordered_map<std::string, std::list<std::unique_ptr<CacheItem>>::iterator> map;
            std::list<std::unique_ptr<CacheItem>> list;

            size_t load;
            const size_t capacity;

            uint64_t hit;
            uint64_t accessed;
        };
    }
}
#endif
