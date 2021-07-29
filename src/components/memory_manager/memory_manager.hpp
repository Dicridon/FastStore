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
            struct AllocatorHeader {
                size_t total_size;
                Page *freelist;
                Page *cursor;
            };
        public:
            Allocator() = delete;
            ~Allocator() {};
            Allocator(const Allocator &) = delete;
            Allocator(Allocator &&) = delete;
            auto operator=(const Allocator &) -> Allocator & = delete;
            auto operator=(Allocator &&) -> Allocator & = delete;

            static auto make_allocator(const byte_ptr_t &base, size_t size) -> Allocator & {
                auto allocator = reinterpret_cast<Allocator *>(base);
                allocator->header.total_size = size;

                // only for page reuse
                allocator->header.freelist = nullptr;
                allocator->header.cursor = reinterpret_cast<Page *>(base + sizeof(AllocatorHeader));
                return *allocator;
            }

        private:
            AllocatorHeader header;
        };
    }
}
#endif
