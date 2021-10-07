#ifndef __HILL__INDEXING__INDEXING__
#define __HILL__INDEXING__INDEXING__
#include "memory_manager/memory_manager.hpp"
#include "remote_memory/remote_memory.hpp"
#include "wal/wal.hpp"
#include "kv_pair/kv_pair.hpp"
#include "misc/misc.hpp"

#include <vector>
#include <atomic>
#include <cstring>

namespace Hill {
    namespace Indexing {
        using namespace Memory::TypeAliases;
        using namespace KVPair::TypeAliases;
        namespace Constants {
            static constexpr int iDGREE = 64;
            static constexpr int iNUM_HIGHKEY = iDGREE - 1;
        }

        namespace Enums {
            enum class OpStatus {
                Ok,
                Failed,
                Retry,
                NoMemory,
                NeedSplit,
            };

            enum class NodeType : byte_t {
                Leaf,
                Inner,
                Unknown,
            };
        }

        struct VersionLock {
            std::atomic<uint64_t> l;
            mutable uint64_t tmp;
            VersionLock() : l(0), tmp(0) {};
            ~VersionLock() = default;
            VersionLock(const VersionLock &) = delete;
            VersionLock(VersionLock &&) = delete;
            auto operator=(const VersionLock &) -> VersionLock = delete;
            auto operator=(VersionLock &&) -> VersionLock = delete;

            inline auto lock() noexcept -> void {
                auto expected = 0UL;
                do {
                    tmp = l.load();                    
                    expected = tmp & (~0x1UL);
                } while (l.compare_exchange_strong(expected, tmp | (0x1UL)));
            }
            
            inline auto try_lock() noexcept -> bool {
                tmp = l.load();
                auto expected = tmp & (~0x1UL);
                auto desired = tmp | (0x1UL);
                return l.compare_exchange_strong(expected, desired);
            }

            inline auto unlock() noexcept -> void {
                l += 1;
            }

            inline auto is_locked() const noexcept -> bool {
                tmp = l.load();
                return tmp & (0x1UL);
            }

            inline auto version() const noexcept -> uint64_t {
                return l >> 1;
            }

            inline auto whole_value() const noexcept -> uint64_t {
                return l.load();
            }

            inline auto reset() noexcept -> void {
                l.store(0);
            }
        };

        struct InnerNode;
        struct LeafNode {
            hill_key_t *highkey;            
            hill_key_t *keys[Constants::iDGREE];
            Memory::PolymorphicPointer values[Constants::iDGREE];
            LeafNode *right_link;
            // for convenient access
            VersionLock version_lock;

            LeafNode() = delete;
            // All nodes are on PM, not in heap or stack
            ~LeafNode() = delete;
            LeafNode(const LeafNode &) = delete;
            LeafNode(LeafNode &&) = delete;
            auto operator=(const LeafNode &) = delete;
            auto operator=(LeafNode &&) = delete;

            static auto make_leaf(const byte_ptr_t &ptr) -> struct LeafNode * {
                auto tmp = reinterpret_cast<LeafNode *>(ptr);
                for (int i = 0; i < Constants::iDGREE; i++) {
                    tmp->version_lock.reset();
                    tmp->keys[i] = nullptr;
                    tmp->values[i] = nullptr;
                    tmp->right_link = nullptr;
                    tmp->highkey = nullptr;
                }
                return tmp;
            }

            inline auto is_full() const noexcept -> bool {
                return keys[Constants::iDGREE - 1] != nullptr;
            }

            auto insert(int tid, WAL::Logger *log, Memory::Allocator *alloc, Memory::RemoteMemoryAgent *agent,
                        const char *k, size_t k_sz, const char *v, size_t v_sz) -> Enums::OpStatus;
        };

        
        struct PolymorphicNodePointer {
            Enums::NodeType type;
            void *value;
            PolymorphicNodePointer() : type(Enums::NodeType::Unknown), value(nullptr) {};
            PolymorphicNodePointer(std::nullptr_t nu) : type(Enums::NodeType::Unknown), value(nu) {};
            PolymorphicNodePointer(const PolymorphicNodePointer &) = default;
            PolymorphicNodePointer(LeafNode *l) : type(Enums::NodeType::Leaf), value(l) {};
            PolymorphicNodePointer(InnerNode *l) : type(Enums::NodeType::Inner), value(l) {};
            ~PolymorphicNodePointer() = default;
            auto operator=(const PolymorphicNodePointer &) -> PolymorphicNodePointer & = default;
            auto operator=(PolymorphicNodePointer &&) -> PolymorphicNodePointer & = default;
            auto operator=(std::nullptr_t nu) -> PolymorphicNodePointer & {
                value = nu;
                return *this;
            }
            auto operator=(LeafNode *v) -> PolymorphicNodePointer & {
                value = reinterpret_cast<void *>(v);
                return *this;
            }
            
            auto operator=(InnerNode *v) -> PolymorphicNodePointer & {
                value = reinterpret_cast<void *>(v);
                return *this;
            }

            
            inline auto is_leaf() const noexcept -> bool {
                return type == Enums::NodeType::Leaf;
            }

            inline auto is_inner() const noexcept -> bool {
                return type == Enums::NodeType::Inner;
            }

            inline auto is_null() const noexcept -> bool {
                return value == nullptr;
            }

            template<typename T>
            inline auto get_as() const noexcept -> typename std::enable_if<std::is_same_v<T, LeafNode *>, LeafNode *>::type {
                return reinterpret_cast<T>(value);
            }

            template<typename T>
            inline auto get_as() const noexcept -> typename std::enable_if<std::is_same_v<T, InnerNode *>, InnerNode *>::type {
                return reinterpret_cast<T>(value);
            }

            inline auto get_highkey() const noexcept -> hill_key_t * {
                // InnerNode and LeafNode's memory layouts are similar
                return get_as<LeafNode *>()->highkey;
            }
        };
        

        /*
         * The layout of a node is as follows
         * | k1 | k2 | k3 |
         * | c1 | c2 | c3 | c4 |
         * Each child stored keys <= highkey
         *
         * Note we do not keep a parent pointer because a vector is used for backtracing
         * We do not use smart pointers either because we need atomic update to pointers
         */
        struct InnerNode {
            hill_key_t *highkey;            
            hill_key_t *keys[Constants::iNUM_HIGHKEY];
            PolymorphicNodePointer children[Constants::iDGREE];
            InnerNode *right_link;
            VersionLock version_lock;

            InnerNode() = default;
            // All nodes are on PM, not in heap or stack
            ~InnerNode() = default;
            InnerNode(const InnerNode &) = delete;
            InnerNode(InnerNode &&) = delete;
            auto operator=(const InnerNode &) = delete;
            auto operator=(InnerNode &&) = delete;

            static auto make_inner() -> InnerNode * {
                auto tmp = new InnerNode;
                for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                    tmp->version_lock.reset();
                    tmp->keys[i] = nullptr;
                    tmp->children[i] = nullptr;
                    tmp->right_link = nullptr;
                    tmp->highkey = nullptr;
                }
                tmp->children[Constants::iDGREE - 1] = nullptr;
                return tmp;
            }

            inline auto is_full() const noexcept -> bool {
                return keys[Constants::iNUM_HIGHKEY - 1] != nullptr;
            }

            // this child should be on the right of split_key
            auto insert(hill_key_t *split_key, PolymorphicNodePointer child) -> Enums::OpStatus;
        };

        
        class OLFIT {
        public:
            OLFIT(Memory::Allocator *alloc_, std::unique_ptr<WAL::Logger> &&logger_)
                : root(nullptr), alloc(alloc_), logger(std::move(logger_)), agent(nullptr) {
                // NodeSplit is also for new root node creation
                auto ptr = logger->make_log(0, WAL::Enums::Ops::NodeSplit);
                // crashing here is ok, because no memory allocation is done;
                alloc->allocate(0, sizeof(LeafNode), ptr);
                /* 
                 * crash here is ok, allocation is done. Crash in the allocation function
                 * is fine because on recovery, the allocator scans memory regions to restore
                 * partially allocated memory blocks
                 */
                LeafNode::make_leaf(ptr);
                logger->commit(0);
            }
            // external interfaces use const char * as input
            auto insert(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz) noexcept -> Enums::OpStatus;
            auto search(const char *k, size_t k_sz) const noexcept -> Memory::PolymorphicPointer;
            inline auto enable_agent(Memory::RemoteMemoryAgent *agent_) -> void {
                agent = agent_;
            }
            
        private:
            PolymorphicNodePointer root;
            Memory::Allocator *alloc;
            std::unique_ptr<WAL::Logger> logger;
            Memory::RemoteMemoryAgent *agent;
            auto traverse_node(const char *k, size_t k_sz) const noexcept -> std::pair<LeafNode *, std::vector<InnerNode *>> {
                if (root.is_leaf()) {
                    return {root.get_as<LeafNode *>(), {}};
                }
                
                PolymorphicNodePointer current = root;
                PolymorphicNodePointer next = nullptr;
                InnerNode *inner;
                auto version = 0UL;
                std::vector<InnerNode *> ancestors;
                while (!current.is_leaf()) {
                    inner = current.get_as<InnerNode *>();
                    version = inner->version_lock.version();
                    next = find_next(inner, k, k_sz, ancestors);
                    if (inner->version_lock.version() == version) {
                        current = next;
                    }
                }
                return {current.get_as<LeafNode *>(), std::move(ancestors)};
            }

            // follow the original paper of OLFIT
            auto find_next(InnerNode *current, const char *k, size_t k_sz, std::vector<InnerNode *>& ans) const noexcept
                -> PolymorphicNodePointer {
                auto chars = current->highkey->raw_chars();
                auto size = current->highkey->size();
                auto result = strncmp(chars, k, std::min(k_sz, size));
                switch(result) {
                case 0:
                    ans.push_back(current);
                    return current->children[Constants::iDGREE - 1];
                case 1:
                    for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                        if (strncmp(chars, k, std::min(k_sz, size)) <= 0) {
                            ans.push_back(current);
                            return current->children[i];
                        }
                    }
                case -1:
                    return current->right_link;
                default:
                    return nullptr;
                }
            }


            // split an old node and return a new node with keys migrated
            auto split_leaf(int tid, LeafNode *l, const char *k, size_t k_sz, const char *v, size_t v_sz) -> LeafNode *;
            // split_inner is seperated from split leaf because they have different memory policies
            auto split_inner(int tid, InnerNode *l, hill_key_t *splitkey, PolymorphicNodePointer child) -> InnerNode *;
            // push up split keys to ancestors
            auto push_up(int tid, LeafNode *node, LeafNode *new_leaf, std::vector<InnerNode *> &ans) -> Enums::OpStatus;
            
        };
    }
}
#endif
