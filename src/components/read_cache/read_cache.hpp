#ifndef __HILL__READ_CACHE__READ_CACHE__
#define __HILL__READ_CACHE__READ_CACHE__

#include "memory_manager/memory_manager.hpp"
#include "kv_pair/kv_pair.hpp"
#include "remote_memory/remote_memory.hpp"
#include "config/config.hpp"

#include <unordered_map>

namespace Hill {
    namespace ReadCache {
        using namespace ::Hill::Memory;

        namespace Constants {
#ifdef __HILL_DEBUG__
            constexpr size_t uCACHE_SIZE = 100UL;
#else
            constexpr size_t uCACHE_SIZE = 100000UL;
#endif
        }
        
        struct CacheItem {
            // these two lengths are for fast remote access
            uint64_t key_length : 16;
            uint64_t value_length : 16;
            uint64_t fingerprint : 32;
            const KVPair::HillString *key_ptr;
            PolymorphicPointer value_ptr;
        };

        /*
         * ReadCache is designed for sharing hot data among nodes, so its structure is plain array
         * and is constructued on a given memory region for RDMA access
         *
         * Though HillString tracks string size, but it's kept in PM, we copy this size in this 
         * cache so that we do not need to access PM multiple times. More importantly, remote nodes
         * are able to fetch keys and values with one single RDMA read instead of first reading 
         * lengths then contents
         */
        struct Cache {
            // flexible array
            CacheItem items[1];
            static constexpr std::hash<std::string> hasher{};
            
            Cache() = default;
            ~Cache() = default;
            Cache(const Cache &) = delete;
            Cache(Cache &&) = delete;
            auto operator=(const Cache &) = delete;
            auto operator=(Cache &&) = delete;

            static auto make_cache(const byte_ptr_t &region) -> Cache & {
                auto tmp = reinterpret_cast<Cache *>(region);

                for (size_t i = 0; i < Constants::uCACHE_SIZE; i++) {
                    memset(&tmp->items[i], 0, sizeof(CacheItem));
                }
                return *tmp;
            };

            auto get(const std::string &in) const noexcept -> PolymorphicPointer {
                auto full = hasher(in);
                auto fingerprint = full & 0x00000000ffffffff;
                auto hash = full % Constants::uCACHE_SIZE;
                auto &item = items[hash];
                if (item.fingerprint == fingerprint) {
                    if (in.compare(0, in.size(), item.key_ptr->raw_chars(), 0, item.key_length) == 0) {
                        return item.value_ptr;
                    }
                }

                return nullptr;
            }

            auto fill(size_t pos, uint64_t full, const KVPair::HillString *key, const PolymorphicPointer &value,
                      const uint16_t value_size) noexcept -> void {
                auto &item = items[pos];
                item.fingerprint = full;
                item.key_ptr = key;
                item.key_length = key->size();
                item.value_ptr = value;
                item.value_length = value_size;
            }
        };
    }
}
#endif
