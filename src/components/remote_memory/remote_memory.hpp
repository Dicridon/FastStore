#ifndef __HILL__REMOTE_MEMORY__REMOTE_MEMORY__
#define __HILL__REMOTE_MEMORY__REMOTE_MEMORY__

#include "memory_manager/memory_manager.hpp"
#include "rdma/rdma.hpp"
#include "misc/misc.hpp"
#include "cluster/cluster.hpp"

namespace Hill {
    namespace Memory {
        namespace Constants {
            static constexpr uint64_t uREMOTE_POINTER_MASK = ~0xffff000000000000UL;
            static constexpr uint64_t uREMOTE_POINTER_BITS_MASK = 0xc000000000000000UL;
            static constexpr uint64_t uREMOTE_POINTER_BITS = 0x2UL;
            static constexpr uint64_t uREMOTE_REGIONS = 32;
        }

        namespace Enums {
            
        }

        using namespace Memory::TypeAliases;
        /* !!!NEVER INHERIT FROM ANY OTHER STRUCT OR CLASS!!! */
        /*
         * RemotePointer is a pointer with node infomation embeded in the highest 16 bits.
         * To get the correct address in x86, the 16 bits should be the same as bit 47 of
         * the original pointer, i.e., it is a canonical pointer. 
         *
         * Current RemotePointer layout is as follows
         *
         * 63 62 61 60 59 58 57 56 55              48             0
         * --------------------------------------------------------
         * |  A  |       B      |  |       C       |              |
         * --------------------------------------------------------
         * A: remote pointer bits, 0b'10 indicates a remote pointer
         * B: node ID (64 machines at most)
         * C: filling hint
         */
        class RemotePointer {
        public:
            inline static auto is_remote_pointer(const byte_ptr_t &ptr) -> bool {
                auto bits = ((reinterpret_cast<uint64_t>(ptr) & Constants::uREMOTE_POINTER_BITS_MASK) >> 62);
                return bits == Constants::uREMOTE_POINTER_BITS;
            }

            inline static auto make_remote_pointer(uint64_t node, uint64_t address) -> RemotePointer {
                auto value = address & Constants::uREMOTE_POINTER_MASK;
                auto meta = (Constants::uREMOTE_POINTER_BITS << 6) | (node & (0x3fUL));
                auto tmp = (meta << 56) | value;

                // dangerous operation, copy(move) assignment is necessary
                return *reinterpret_cast<RemotePointer *>(&tmp);
            }

            inline static auto make_remote_pointer(uint64_t node, const byte_ptr_t &address) -> RemotePointer {
                auto value = reinterpret_cast<uint64_t>(address) & Constants::uREMOTE_POINTER_MASK;
                auto meta = (Constants::uREMOTE_POINTER_BITS << 6) | (node & (~0xc0UL));
                auto tmp = (meta << 56) | value;

                // dangerous operation, copy(move) assignment is necessary
                return *reinterpret_cast<RemotePointer *>(&tmp);
            }
            
            RemotePointer() = default;
            RemotePointer(std::nullptr_t nu) : ptr(nu) {};
            RemotePointer(byte_ptr_t &p) : ptr(p) {};
            
            ~RemotePointer() = default;
            RemotePointer(const RemotePointer &) = default;
            RemotePointer(RemotePointer &&) = default;
            auto operator=(const RemotePointer &) -> RemotePointer & = default;
            auto operator=(RemotePointer &&) -> RemotePointer & = default;
            
            template<typename T, typename = typename std::enable_if_t<std::is_pointer_v<T>>>
            auto get_as() const noexcept -> T {
                auto copy = ptr;
                auto cursor = reinterpret_cast<byte_ptr_t>(&copy);
                cursor[7] = cursor[6];
                return reinterpret_cast<T>(copy);
            }

            inline auto get_node() const noexcept -> int {
                auto value = reinterpret_cast<uint64_t>(ptr);
                return (value >> 56) & (0x3fUL);
            }

            inline auto raw_ptr() const noexcept -> byte_ptr_t {
                return ptr;
            }

            inline auto is_nullptr() const noexcept -> bool {
                return ptr == nullptr;
            }
        private:
            byte_ptr_t ptr;
        };

        class PolymorphicPointer {
        public:
            PolymorphicPointer() = default;
            PolymorphicPointer(const RemotePointer &re) {
                ptr.remote = re;
            }
            PolymorphicPointer(std::nullptr_t nu) {
                ptr.local = nu;
            }
            template<typename T, typename = typename std::enable_if_t<std::is_pointer_v<T>>>
            PolymorphicPointer(const T &lo) {
                ptr.local = lo;
            }
            ~PolymorphicPointer() = default;
            PolymorphicPointer(const PolymorphicPointer &) = default;
            PolymorphicPointer(PolymorphicPointer &&) = default;
            auto operator=(const PolymorphicPointer &) -> PolymorphicPointer & = default;
            auto operator=(PolymorphicPointer &&) -> PolymorphicPointer & = default;
            auto operator=(const RemotePointer &re) -> PolymorphicPointer & {
                ptr.remote = re;
                return *this;
            }
            auto operator=(const byte_ptr_t &lo) -> PolymorphicPointer & {
                ptr.local = lo;
                return *this;
            }
            auto operator=(const std::nullptr_t &nu) -> PolymorphicPointer {
                ptr.local = nu;
                return *this;
            }

            inline auto operator==(const PolymorphicPointer &rhs) -> bool {
                return ptr.local == rhs.ptr.local;
            }

            inline auto operator!=(const PolymorphicPointer &rhs) -> bool {
                return ptr.local != rhs.ptr.local;
            }

            inline auto operator==(std::nullptr_t nu) -> bool {
                return ptr.local == nu;
            }

            inline auto operator!=(std::nullptr_t nu) -> bool {
                return ptr.local != nu;
            }

            template<typename T, typename = std::enable_if_t<std::is_pointer_v<T>>>
            static auto make_polymorphic_pointer(const T &t) -> PolymorphicPointer {
                PolymorphicPointer ret;
                ret.ptr.local = reinterpret_cast<byte_ptr_t>(t);
                return ret;
            }

            static auto make_polymorphic_pointer(const RemotePointer &t) -> PolymorphicPointer {
                PolymorphicPointer ret;
                ret.ptr.remote = t;
                return ret;
            }

            inline auto is_remote() const noexcept -> bool {
                return RemotePointer::is_remote_pointer(ptr.remote.raw_ptr());
            }

            inline auto is_local() const noexcept -> bool {
                return !RemotePointer::is_remote_pointer(ptr.remote.raw_ptr());
            }

            inline auto is_nullptr() const noexcept -> bool {
                return ptr.local == nullptr;
            }

            inline auto raw_ptr() const noexcept -> byte_ptr_t {
                return ptr.local;
            }

            inline auto remote_ptr() const noexcept -> RemotePointer {
                return ptr.remote;
            }

            inline auto local_ptr() const noexcept -> byte_ptr_t {
                return ptr.local;
            }

            template<typename T, typename = typename std::enable_if_t<std::is_pointer_v<T>>>
            auto get_as() const noexcept -> T {
                if (is_local()) {
                    return reinterpret_cast<T>(ptr.local);
                } else {
                    return ptr.remote.get_as<T>();
                }
            }
            
        private:
            union {
                RemotePointer remote;
                byte_ptr_t local;
            } ptr;
        };


        using namespace RDMAUtil;
        /*
         * !!! NEVER INHERIT FROM ANY OTHER CLASSES OR STRUCTS
         * This class is not thread-safe, intending for thread-local use only
         */
        class RemoteAllocator {
        public:
            RemoteAllocator() : base(nullptr) {
                *reinterpret_cast<uint64_t *>(&meta) = 0UL;
            };
            RemoteAllocator(const RemotePointer &) = delete;
            RemoteAllocator(RemotePointer &&) = delete;
            auto operator=(const RemotePointer &) -> RemoteAllocator & = delete;
            auto operator=(RemoteAllocator &&) -> RemoteAllocator & = delete;

            inline auto set_base(const RemotePointer &remote) noexcept -> void {
                base = remote;
            }
            
            auto allocate(size_t size, byte_ptr_t &ptr) noexcept -> void {
                if (meta.counter + size >= Constants::uREMOTE_REGION_SIZE) {
                    ptr = nullptr;
                }

                ptr = base.raw_ptr() + meta.counter;
                auto snap = meta;
                ++snap.counter;
                snap.cursor += size;
                meta = snap;
            }

            auto available() const noexcept -> bool {
                return !base.is_nullptr() && meta.cursor < Constants::uREMOTE_REGION_SIZE;
            }
            
            auto free(RemotePointer &ptr) noexcept -> void {
                UNUSED(ptr);
                --meta.counter;
            }

            inline auto is_empty() const noexcept -> bool {
                return meta.counter == 0;
            }
            
        private:
            RemotePointer base;
            // for persistent atomicity
            struct {
                // No. of current objects                
                uint64_t counter : 32;
                // Offset within this region => Regions size should not exceed 4GB
                uint64_t cursor : 32;
            } meta;
        };
        
        /*
         * My purpose of writing this class is for accessing remote PM. So I will always assume
         * RDMA connections exposing PM on other nodes are recorded here
         */
        class RemoteMemoryAgent {
        public:
            RemoteMemoryAgent() = default;
            RemoteMemoryAgent(const RemoteMemoryAgent &) = delete;
            RemoteMemoryAgent(RemoteMemoryAgent &&) = delete;
            auto operator=(const RemotePointer &) -> RemotePointer & = delete;
            auto operator=(RemotePointer &&) -> RemotePointer & = delete;

            static auto make_agent(const byte_ptr_t &pm, std::array<std::unique_ptr<RDMAContext>, Cluster::Constants::uMAX_NODE> *p) -> RemoteMemoryAgent * {
                auto tmp = reinterpret_cast<RemoteMemoryAgent *>(pm);
                memset(tmp, 0, sizeof(RemoteMemoryAgent));
                tmp->peer_connections = p;
                return tmp;
            }

            auto add_region(int tid, const RemotePointer &ptr) -> bool {
                if (++cursors[tid] < Constants::uREMOTE_REGIONS) {
                    allocators[tid][++cursors[tid]].set_base(ptr);
                    return true;
                }
                return false;
            }

            inline auto allocate(int tid, size_t size, byte_ptr_t &ptr) -> void {
                allocators[tid][cursors[tid]].allocate(size, ptr);
            }

            inline auto available(int tid) const noexcept -> bool {
                return allocators[tid][cursors[tid]].available();
            }

            inline auto free(int tid, RemotePointer &ptr) {
                allocators[tid][cursors[tid]].free(ptr);
            }

            inline auto get_peer_connection(int tid, int node_id) -> std::unique_ptr<RDMAContext> & {
                return peer_connections[tid][node_id];
            }

        private:
            RemoteAllocator allocators[Constants::iTHREAD_LIST_NUM][Constants::uREMOTE_REGIONS];
            size_t cursors[Constants::iTHREAD_LIST_NUM];
            // a reference to the engine's peer connections
            std::array<std::unique_ptr<RDMAContext>, Cluster::Constants::uMAX_NODE> *peer_connections;
        };
    }
}
#endif

