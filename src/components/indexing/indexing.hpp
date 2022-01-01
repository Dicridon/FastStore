#ifndef __HILL__INDEXING__INDEXING__
#define __HILL__INDEXING__INDEXING__
#include "memory_manager/memory_manager.hpp"
#include "remote_memory/remote_memory.hpp"
#include "wal/wal.hpp"
#include "kv_pair/kv_pair.hpp"
#include "misc/misc.hpp"
#include "coloring/coloring.hpp"
#include "debug_logger/debug_logger.hpp"
#include "city/city.hpp"

#include <vector>
#include <atomic>
#include <cstring>

namespace Hill {
    namespace Indexing {
        using namespace Memory::TypeAliases;
        using namespace KVPair::TypeAliases;
        using namespace WAL::TypeAliases;
        namespace Constants {
#ifdef __HILL_DEBUG__
            static constexpr int iDEGREE = 3;
            static constexpr int iNUM_HIGHKEY = iDEGREE - 1;
#else
            static constexpr int iDEGREE = 16;
            static constexpr int iNUM_HIGHKEY = iDEGREE - 1;
#endif
        }

        namespace Enums {
            enum class OpStatus {
                Ok,
                Failed,
                Retry,
                NoMemory,
                NeedSplit,
                RepeatInsert,
                Unkown,
            };

            enum class NodeType : byte_t {
                Leaf,
                Inner,
                Unknown,
            };
        }

        struct InnerNode;
        struct LeafNode {
            InnerNode *parent;
            uint64_t fingerprints[Constants::iNUM_HIGHKEY];
            hill_key_t *keys[Constants::iNUM_HIGHKEY];
            Memory::PolymorphicPointer values[Constants::iNUM_HIGHKEY];
            size_t value_sizes[Constants::iNUM_HIGHKEY];

            LeafNode() = delete;
            // All nodes are on PM, not in heap or stack
            ~LeafNode() = delete;
            LeafNode(const LeafNode &) = delete;
            LeafNode(LeafNode &&) = delete;
            auto operator=(const LeafNode &) = delete;
            auto operator=(LeafNode &&) = delete;

            static auto make_leaf(const byte_ptr_t &ptr) -> struct LeafNode * {
                auto tmp = reinterpret_cast<LeafNode *>(ptr);
                for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                    tmp->keys[i] = nullptr;
                    tmp->values[i] = nullptr;
                    tmp->value_sizes[i] = 0;
                    tmp->fingerprints[i] = 0;
                }
                tmp->parent = nullptr;
                return tmp;
            }

            inline auto is_full() const noexcept -> bool {
                return keys[Constants::iNUM_HIGHKEY - 1] != nullptr;
            }

            auto insert(int tid, WAL::Logger *log, Memory::Allocator *alloc, Memory::RemoteMemoryAgent *agent,
                        const char *k, size_t k_sz, const char *v, size_t v_sz)
                -> std::pair<Enums::OpStatus, Memory::PolymorphicPointer>;
            auto dump() const noexcept -> void;
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
                type = Enums::NodeType::Unknown;
                value = nu;
                return *this;
            }
            auto operator=(LeafNode *v) -> PolymorphicNodePointer & {
                type = Enums::NodeType::Leaf;
                value = reinterpret_cast<void *>(v);
                return *this;
            }

            auto operator=(InnerNode *v) -> PolymorphicNodePointer & {
                type = Enums::NodeType::Inner;
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

            inline auto get_parent() const noexcept -> InnerNode * {
                return get_as<LeafNode *>()->parent;
            }

            inline auto set_parent(InnerNode *p) -> void {
                get_as<LeafNode *>()->parent = p;
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
            InnerNode *parent;
            hill_key_t *keys[Constants::iNUM_HIGHKEY];
            PolymorphicNodePointer children[Constants::iDEGREE];

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
                    tmp->keys[i] = nullptr;
                    tmp->children[i] = nullptr;
                }
                tmp->parent = nullptr;
                tmp->children[Constants::iDEGREE - 1] = nullptr;
                return tmp;
            }

            inline auto is_full() const noexcept -> bool {
                return keys[Constants::iNUM_HIGHKEY - 1] != nullptr;
            }

            // this child should be on the right of split_key
            auto insert(const hill_key_t *split_key, PolymorphicNodePointer child) -> Enums::OpStatus;
            auto dump() const noexcept -> void;
        };

        class OLFIT {
        public:
            // for convenience of testing
            OLFIT(int tid, Memory::Allocator *alloc_, WAL::Logger *logger_)
                : root(nullptr), alloc(alloc_), logger(logger_), agent(nullptr) {
                // NodeSplit is also for new root node creation
                auto &ptr = logger->make_log(tid, WAL::Enums::Ops::NodeSplit);
                // crashing here is ok, because no memory allocation is done;
                alloc->allocate(tid, sizeof(LeafNode), ptr);
                /*
                 * crash here is ok, allocation is done. Crash in the allocation function
                 * is fine because on recovery, the allocator scans memory regions to restore
                 * partially allocated memory blocks
                 */
                root = LeafNode::make_leaf(ptr);
                logger->commit(tid);
            }
            ~OLFIT() = default;

            static auto make_olfit(Memory::Allocator *alloc, WAL::Logger *logger) -> std::unique_ptr<OLFIT> {
#ifdef __HILL_INFO__
                std::cout << ">> OLFIT degree: " << Constants::iDEGREE << "\n";
#endif

                auto a_tid = alloc->register_thread();
                if (!a_tid.has_value()) {
                    return nullptr;
                }
                
                auto l_tid = logger->register_thread();
                if (!l_tid.has_value()) {
                    return nullptr;
                }

                if (a_tid.value() != l_tid.value()) {
                    alloc->unregister_thread(a_tid.value());
                    logger->unregister_thread(l_tid.value());
                    return nullptr;
                }
                auto ret = std::make_unique<OLFIT>(a_tid.value(), alloc, logger);
                alloc->unregister_thread(a_tid.value());
                logger->unregister_thread(l_tid.value());
                return ret;
            }

            // external interfaces use const char * as input
            auto insert(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz)
                noexcept -> std::pair<Enums::OpStatus, Memory::PolymorphicPointer>;
            auto search(const char *k, size_t k_sz) const noexcept -> std::pair<Memory::PolymorphicPointer, size_t>;
            auto update(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz)
                noexcept -> std::pair<Enums::OpStatus, Memory::PolymorphicPointer>;
            auto remove(int tid, const char *k, size_t k_sz) noexcept -> Enums::OpStatus;
            auto scan(const char *k, size_t num) -> std::vector<Memory::PolymorphicPointer>;
            
            inline auto get_root() const noexcept -> PolymorphicNodePointer {
                return root;
            }
            
            inline auto enable_agent(Memory::RemoteMemoryAgent *agent_) -> void {
                agent = agent_;
            }
            auto dump() const noexcept -> void;

        private:
            PolymorphicNodePointer root;
            Memory::Allocator *alloc;
            WAL::Logger *logger;
            Memory::RemoteMemoryAgent *agent;

            auto traverse_node(const char *k, size_t k_sz) const noexcept -> LeafNode * {
                if (root.is_leaf()) {
                    return root.get_as<LeafNode *>();
                }

                PolymorphicNodePointer current = root;
                PolymorphicNodePointer next = nullptr;
                InnerNode *inner;
                while (!current.is_leaf()) {
                    inner = current.get_as<InnerNode *>();
                    next = find_next(inner, k, k_sz);
                    current = next;
                }
                return current.get_as<LeafNode *>();
            }

            auto get_pos_of(const char *k, size_t k_sz) const noexcept -> std::pair<LeafNode *, int> {
                auto leaf = traverse_node(k, k_sz);
                auto fp = CityHash64(k, k_sz);
                int i = 0;
                for (i = 0; i < Constants::iDEGREE; i++) {
                    if (leaf->keys[i] == nullptr) {
                        return {leaf, -1};
                    }

                    if (leaf->fingerprints[i] != fp) {
                        continue;
                    }

                    if (leaf->keys[i]->compare(k, k_sz) == 0) {
                        return {leaf, i};
                    }
                }
                return {nullptr, -1};
            }

            // follow the original paper of OLFIT, OT
            auto find_next(InnerNode *current, const char *k, size_t k_sz) const noexcept -> PolymorphicNodePointer {
                PolymorphicNodePointer ret;
                hill_key_t *tmp = nullptr;
                int i;
                for (i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                    tmp = current->keys[i];
                    if (tmp == nullptr || tmp->compare(k, k_sz) > 0) {
                            return current->children[i];                                
                    }
                }
                return current->children[i];
            }

            // split an old node and return a new node with keys migrated
            auto split_leaf(int tid, LeafNode *l, const char *k, size_t k_sz, const char *v, size_t v_sz)
                -> std::pair<LeafNode *, Memory::PolymorphicPointer>;
            // split_inner is seperated from split leaf because they have different memory policies
            auto split_inner(InnerNode *l, const hill_key_t *splitkey, PolymorphicNodePointer child)
                -> std::pair<InnerNode *, hill_key_t *>;
            // push up split keys to ancestors
            auto push_up(LeafNode *new_leaf) -> Enums::OpStatus;
        };
    }
}
#endif
