#include "memory_manager.hpp"

namespace Hill {
    namespace Memory {
        
        
        auto Page::allocate(size_t size, byte_ptr_t &ptr) noexcept -> void {
            auto available = page_size - header.cursor;
            if (available < size)
                return;

            auto snapshot = header;
            auto tmp_ptr = reinterpret_cast<byte_ptr_t>(this);
            ptr = tmp_ptr + snapshot.cursor;

            snapshot.cursor += size;
            ++snapshot.records;

            header = snapshot;
            pmem_persist(this, sizeof(PageHeader));
        }

        auto Page::free(const byte_ptr_t &ptr) noexcept -> void {
            auto page_address = reinterpret_cast<Page *>((reinterpret_cast<uint64_t>(ptr)) & 0xffffc000UL);
            --page_address->header.records;
            pmem_persist(this, sizeof(PageHeader));
        }
    }
}
