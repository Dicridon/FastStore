#ifndef __HILL__MEMORY_MANAGER__MEMORY_MANAGER__
#define __HILL__MEMORY_MANAGER__MEMORY_MANAGER__
#include <optional>
#include <cstring>
#include <mutex>
#include "config/config.hpp"

#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
#include <iostream>
#include <cassert>
#endif

#ifdef __HILL_PMEM__
#include <libpmem.h>
#endif
namespace Hill {
    // Specialized memory manager for KV
    // For durability, use this with a WAL
    namespace Memory {
        extern std::mutex allocator_global_lock;

        struct Page;
        namespace Constants {
#ifdef __HILL_DEBUG__
            static constexpr size_t uPAGE_SIZE = 128UL;
            static constexpr uint64_t uPAGE_MASK = 0xffffffffffffff80UL;
            static constexpr Page * pTHREAD_LIST_AVAILABLE = nullptr;
            static constexpr int iTHREAD_LIST_NUM = 8;
            static constexpr uint64_t uALLOCATOR_MAGIC = 0xabcddcbaabcddcbaUL;
            static constexpr size_t uPREALLOCATION = 1;
#else
            static constexpr size_t uPAGE_SIZE = 16 * 1024UL;
            static constexpr uint64_t uPAGE_MASK = 0xffffffffffffc000UL;
            static constexpr Page * pTHREAD_LIST_AVAILABLE = nullptr;
            static constexpr int iTHREAD_LIST_NUM = 64;
            static constexpr uint64_t uALLOCATOR_MAGIC = 0xabcddcbaabcddcbaUL;
            static constexpr size_t uPREALLOCATION = 1;
#endif
        }

        namespace Enums {
            enum class AllocatorRecoveryStatus {
                Ok,
                Corrupted,
                NoAllocator,
            };
        }

        /*
         * All pointers used in Hill are natural pointers (possibly with some embeded functional bits)
         * We do not't use offset so that memory deallocation is simplified
         *
         * Considering that ASLR is enabled for security concerns, the mapped PM device is not guaranteed
         * to be mapped to the same address upon restart. Fortunately, libpmem offsers an environment
         * variable named PMEM_MMAP_HINT, which will disable ASLR and force libpmem to TRY to map the
         * device the the address specified by PMEM_MMAP_HINT. Thus I assume we can finally find an
         * address available for such mapping.
         */
        namespace TypeAliases {
            using byte_t = uint8_t;
            using byte_ptr_t = uint8_t *;
            using const_byte_ptr_t = const uint8_t *;
        }

        namespace Util {
            inline void mfence(void) {
                asm volatile("mfence":::"memory");
            }
        }
        /*
         * A Page(16KB) is the basic memory alloction granularity, more
         * fine grained allocation is performed within each page in each
         * thread (this implies no concurrency control is required)
         *
         * 0     7     15                   63
         * |--------------------------------|
         * |  A  |  B  |    C    |    D     |
         * |--------------------------------|
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |                                |
         * |--------------------------------|
         * |             NEXT               |
         * |--------------------------------|
         * A: number of total records (valid and invalid)
         * B: number of total valid records
         * C: header cursor, grows upward
         * D: record cursor, grows downward
         * NEXT: free pages are linked as a linked list
         *
         */

        /* !!!NEVER INHERIT FROM ANY OTHER STRUCT OR CLASS!!! */
        using namespace TypeAliases;
        struct RecordHeader {
            uint16_t offset;
        };

        struct Page {
        private:
            struct PageHeader {
                // how many record headers a in use
                uint64_t records : 8;
                // how many in-use record headers are valid, if 0 on free, the page should be reclaimed
                uint64_t valid : 8;
                uint64_t header_cursor: 24;
                uint64_t record_cursor: 24;
            };

        public:
            Page() = delete;
            ~Page() {};
            Page(const Page &) =delete;
            Page(Page &&) = delete;
            auto operator=(const Page &) -> Page & = delete;
            auto operator=(Page &&) -> Page & = delete;

            static auto make_page(const byte_ptr_t &in, Page *n = nullptr) -> Page & {
                auto page_ptr = reinterpret_cast<Page *>(in);
                page_ptr->header.records = 0;
                page_ptr->header.valid = 0;
                page_ptr->header.header_cursor = sizeof(PageHeader); // offsetting the header;
                page_ptr->header.record_cursor = sizeof(Page) - sizeof(Page *); // offsetting the next pointer
                page_ptr->next = n;
                void(page_ptr->_content); // silent the warnings
                return *page_ptr;
            }

            static auto get_page(const byte_ptr_t &ptr) -> Page * {
                return reinterpret_cast<Page *>(reinterpret_cast<uint64_t>(ptr) & Constants::uPAGE_MASK);
            }

            auto allocate(size_t size, byte_ptr_t &ptr) noexcept -> void;
            auto free(byte_ptr_t &ptr) noexcept -> void;

            inline auto get_headers() noexcept -> RecordHeader * {
                auto tmp = reinterpret_cast<byte_ptr_t>(this);
                return reinterpret_cast<RecordHeader *>(tmp + sizeof(PageHeader));
            }

            inline auto is_empty() const noexcept -> bool {
                return header.records == 0;
            }

            inline auto reset_cursor() noexcept -> void {
                header.header_cursor = sizeof(PageHeader);
                header.record_cursor = sizeof(Page) - sizeof(Page *);
#ifdef PMEM
                pmem_persist(&header, sizeof(PageHeader));
#endif
            }

            inline auto link_next(Page *p) noexcept -> void {
                next = p;
#ifdef PMEM
                pmem_persist(&next, sizeof(Page *));
#endif
            }

            PageHeader header;
            byte_t _content[Constants::uPAGE_SIZE - sizeof(PageHeader) - sizeof(Page *)]; // never used
            Page *next;
        };


        /*
         * !!!NEVER INHERIT FROM ANY OTHER CLASSES OR STRUCTS!!!
         * Given a continuous memory region, this calss manages it at 16KB granularity
         *
         * The memory region is 16KB aligned and the first page is always reserved
         * for metadata. Moreover, this class is used to manage a node's own memory
         * for this node's own use. To manage another node's memory, use the RemoteAllocator
         */

        class Allocator {
        public:
            Allocator() = delete;
            ~Allocator() = default;
            Allocator(const Allocator &) = delete;
            Allocator(Allocator &&) = delete;
            auto operator=(const Allocator &) -> Allocator & = delete;
            auto operator=(Allocator &&) -> Allocator & = delete;

            static auto make_allocator(const byte_ptr_t &base, size_t size) -> Allocator * {
                auto allocator = reinterpret_cast<Allocator *>(base);
                allocator->header.magic = Constants::uALLOCATOR_MAGIC;
                allocator->header.total_size = size;
                allocator->header.freelist = nullptr;

                auto aligned = reinterpret_cast<Page *>(reinterpret_cast<uint64_t>(base + sizeof(AllocatorHeader)) & Constants::uPAGE_MASK);
                allocator->header.base = reinterpret_cast<Page *>(aligned + 1);
                allocator->header.cursor = allocator->header.base;

                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    allocator->header.thread_free_lists[i] = const_cast<Page *>(Constants::pTHREAD_LIST_AVAILABLE);
                    allocator->header.thread_pending_pages[i] = nullptr;
                    allocator->header.thread_busy_pages[i] = nullptr;
                    allocator->header.to_be_freed[i] = nullptr;
                    allocator->header.in_use[i] = false;
                }
                return allocator;
            }

            static auto recover_or_makie_allocator(const byte_ptr_t &base, size_t size) -> Allocator * {
                auto allocator = reinterpret_cast<Allocator *>(base);

                switch(allocator->recover()){
                case Enums::AllocatorRecoveryStatus::Ok:
                    for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                        allocator->header.in_use[i] = false;                    
                    }
                    return allocator;
                case Enums::AllocatorRecoveryStatus::Corrupted:
                    return nullptr;
                case Enums::AllocatorRecoveryStatus::NoAllocator:
                    [[fallthrough]];
                default:
                    break;
                }

                allocator->header.magic = Constants::uALLOCATOR_MAGIC;
                allocator->header.total_size = size;
                allocator->header.freelist = nullptr;

                auto aligned = reinterpret_cast<Page *>(reinterpret_cast<uint64_t>(base + sizeof(AllocatorHeader)) & Constants::uPAGE_MASK);
                allocator->header.base = reinterpret_cast<Page *>(aligned + 1);
                allocator->header.cursor = allocator->header.base;

                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    allocator->header.thread_free_lists[i] = const_cast<Page *>(Constants::pTHREAD_LIST_AVAILABLE);
                    allocator->header.thread_pending_pages[i] = nullptr;
                    allocator->header.thread_busy_pages[i] = nullptr;
                    allocator->header.to_be_freed[i] = nullptr;
                    allocator->header.in_use[i] = false;
                }
                return allocator;
            }

            auto register_thread() noexcept -> std::optional<int>;
            auto unregister_thread(int id) noexcept -> void;

            auto allocate(int id, size_t size, byte_ptr_t &ptr) -> void;
            auto free(int id, byte_ptr_t &ptr) -> void;

            auto recover() -> Enums::AllocatorRecoveryStatus;
        private:
            struct AllocatorHeader {
                uint64_t magic;
                size_t total_size;
 Page *freelist;              // only for page reuse
                Page *base;
                Page *cursor;
                Page *thread_free_lists[Constants::iTHREAD_LIST_NUM]; // avoid memory leaks

                // This list is purely for the convenience of unregisteration
                // Free pages in each thread's free list is moved here upon
                // unregisteration. If a new thread is to register, the move
                // it back
                Page *thread_pending_pages[Constants::iTHREAD_LIST_NUM];

                // This list is used to prevent page leak during a free
                Page *to_be_freed[Constants::iTHREAD_LIST_NUM];
                Page *thread_busy_pages[Constants::iTHREAD_LIST_NUM];
                bool in_use[Constants::iTHREAD_LIST_NUM];
            } header;

            auto recover_global_free_list() -> void {
                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    // on-going allocation is detected
                    if (header.thread_free_lists[i] == header.freelist) {
                        auto end = header.freelist;
                        for (size_t i = 0; i < Constants::uPREALLOCATION; i++) {
                            if (end) {
                                end = end->next;
                            }
                        }

                        header.freelist = end->next;
                        end->next = nullptr;
                    }
                }
            }

            auto recover_free_lists() -> void {
                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    // on-going allocation
                    if (header.thread_busy_pages[i] == header.thread_free_lists[i]) {
                        header.thread_free_lists[i] = header.thread_free_lists[i]->next;
                        header.thread_busy_pages[i]->next = nullptr;
                    }
                }
            }

            auto recover_global_heap() -> void {
                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    if (header.thread_free_lists[i] == header.cursor) {
                        header.cursor += Constants::uPREALLOCATION;
                    }
                }
            }

            auto recover_pending_list() -> void {
                // on-going unregisteration
                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    if (header.thread_pending_pages[i] == header.thread_busy_pages[i]) {
                        header.thread_busy_pages[i]->next = header.thread_free_lists[i];
                        header.thread_free_lists[i] = header.thread_busy_pages[i];
                        header.thread_busy_pages[i] = nullptr;
                        header.thread_pending_pages[i] = nullptr;
                    }
                }
            }

            auto recover_to_be_freed() -> void {
                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    if (header.to_be_freed[i]->next != nullptr) {
                        // freelists may have changed during recovery
                        header.to_be_freed[i]->next = header.thread_free_lists[i];
                        header.thread_free_lists[i] = header.to_be_freed[i];
                        header.to_be_freed[i] = nullptr;
                    }
                }
            }
        };
    }
}
#endif
