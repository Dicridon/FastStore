#ifndef __HILL__MEMORY_MANAGER__MEMORY_MANAGER__
#define __HILL__MEMORY_MANAGER__MEMORY_MANAGER__
#include <optional>
#include <cstring>

#define PMEM
#ifdef PMEM
#include <libpmem.h>
#endif
namespace Hill {
    // Specialized memory manager for KV
    // For durability, use this with a WAL
    namespace Memory {
        inline void mfence(void) {
            asm volatile("mfence":::"memory");
        }

        
        /*
         * A Page(16KB) is the basic memory alloction granularity, more 
         * fine grained allocation is performed within each page in each
         * thread (this implies no concurrency control is required)
         *
         * 0     7     15                   63
         * |--------------------------------|
         * |  A  |  B  |         C          |
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
         * A: record counter: 255 at most
         * B: reserved
         * C: free space cursor
         * NEXT: free pages are linked as a linked list
         *
         */
        using byte_t = uint8_t;
        using byte_ptr_t = uint8_t *;

        struct Page {
        private:
            struct PageHeader {
                uint64_t records : 8;
                uint64_t _reserved: 8;
                uint64_t cursor: 48;

                auto operator=(const PageHeader &in) -> PageHeader & {
                    (*(uint64_t *)this) = (*(uint64_t *)&in);
                    return *this;
                }
            };
            
        public:
            Page() = delete;
            ~Page() {};
            Page(const Page &) =delete;
            Page(Page &&) = delete;
            auto operator=(const Page &) -> Page & = delete;
            auto operator=(Page &&) -> Page & = delete;

            static auto make_page(const byte_ptr_t &in) -> Page & {
                auto page_ptr = reinterpret_cast<Page *>(in);
                page_ptr->header.records = 0;
                page_ptr->header.cursor = 8; // offsetting the header;
                page_ptr->next = 0;
                void(page_ptr->_content); // silent the warnings
                return *page_ptr;
            }

            auto allocate(size_t size, byte_ptr_t &ptr) noexcept -> void;
            auto free(const byte_ptr_t &ptr) noexcept -> void;
            
            inline auto is_empty() const noexcept -> bool {
                return header.records == 0;
            }

            inline auto link_next(Page *p) -> void {
                next = p;
#ifdef PMEM
                pmem_persist(&next, sizeof(Page *));
#endif
            }

            static constexpr size_t page_size = 16 * 1024;

            PageHeader header;
            uint64_t _content[16 * 1024 - 16];
            Page *next;
        };

        namespace Constants {
            static const Page * pTHREAD_LIST_AVAILABLE = nullptr;
            static const int iTHREAD_LIST_NUM = 64;            
            static const uint64_t iALLOCATOR_MAGIC = 0xabcddcbaUL;

        }

        namespace Enums {
            enum class AllocatorRecoveryStatus {
                Ok,
                Corrupted,
                NoAllocator,
            };
        }

        /*
         * Given a continuous memory region, this calss manages it at 16KB granularity
         * 
         * The allocation starts at the first 16KB-aligned page and any space ahead this
         * page is reserved for metadata. If the given region is already 16KB aligned, 
         * the first page is reserved for metadata.
         * 
         */
        class Allocator {
        private:
            static const Page *AVAILABLE;
            struct AllocatorHeader {
                uint64_t magic;
                size_t total_size;
                Page *freelist;              // only for page reuse
                Page *cursor;
                Page *thread_free_lists[Constants::iTHREAD_LIST_NUM]; // avoid memory leaks

                // This list is purely for the convenience of unregisteration
                // Free pages in each thread's free list is moved here upon
                // unregisteration. If a new thread is to register, the 
                Page *thread_pending_lists[Constants::iTHREAD_LIST_NUM];
                Page *thread_busy_pages[Constants::iTHREAD_LIST_NUM];
            };
        public:
            Allocator() = delete;
            ~Allocator() {};
            Allocator(const Allocator &) = delete;
            Allocator(Allocator &&) = delete;
            auto operator=(const Allocator &) -> Allocator & = delete;
            auto operator=(Allocator &&) -> Allocator & = delete;

            static auto make_allocator(const byte_ptr_t &base, size_t size) -> Allocator * {
                auto allocator = reinterpret_cast<Allocator *>(base);
                switch(allocator->recover()){
                case Enums::AllocatorRecoveryStatus::Ok:
                    return allocator;
                case Enums::AllocatorRecoveryStatus::Corrupted:
                    return nullptr;
                case Enums::AllocatorRecoveryStatus::NoAllocator:
                    [[fallthrough]];
                default:
                    break;
                }
                
                allocator->header.magic = Constants::iALLOCATOR_MAGIC;
                allocator->header.total_size = size;
                allocator->header.freelist = nullptr;
                allocator->header.cursor = reinterpret_cast<Page *>(base + sizeof(AllocatorHeader));

                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    allocator->header.thread_free_lists[i] = const_cast<Page *>(Constants::pTHREAD_LIST_AVAILABLE);
                    allocator->header.thread_pending_lists[i] = const_cast<Page *>(Constants::pTHREAD_LIST_AVAILABLE);
                    allocator->header.thread_busy_pages[i] = nullptr;
                }
                return allocator;
            }

            auto register_thread() noexcept -> std::optional<int> {
                for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                    if (header.thread_free_lists[i] == Constants::pTHREAD_LIST_AVAILABLE) {
                        return i;
                    }
                }
                return {};
            }

            auto unregister_thread(int id) noexcept -> void {
                if (id < 0 || id > Constants::iTHREAD_LIST_NUM) {
                    return;
                }

                header.thread_pending_lists[id] = header.thread_busy_pages[id];
                header.thread_busy_pages[id] = const_cast<Page *>(Constants::pTHREAD_LIST_AVAILABLE);
            }

            auto allocate(size_t size, byte_ptr_t &ptr) -> byte_ptr_t {
                
            }

            auto free(byte_ptr_t &ptr) -> void {

            }

            auto recover() -> Enums::AllocatorRecoveryStatus {
                
            }
        private:
            AllocatorHeader header;
        };
    }
}
#endif