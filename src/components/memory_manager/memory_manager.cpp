#include "memory_manager.hpp"

namespace Hill {
    namespace Memory {
        /*
         * I put a global lock at namespace scope becauseh the memory manager can not own a transient lock
         * when it resides on PM
         */
        std::mutex allocator_global_lock;
        auto Page::allocate(size_t size, byte_ptr_t &ptr) noexcept -> void {
            auto unavailable = header.header_cursor + sizeof(RecordHeader) > header.record_cursor - size;
            if (unavailable) {
                ptr = nullptr;
                return;
            }

            auto snapshot = header;
            auto tmp_ptr = reinterpret_cast<byte_ptr_t>(this);
            ptr = tmp_ptr + snapshot.record_cursor - size;

            // order here matters;
            ++snapshot.records;
            Util::mfence();
            ++snapshot.valid;
            snapshot.record_cursor -= size;
            auto record_header = get_headers();
            record_header->offset = snapshot.record_cursor;
            snapshot.header_cursor += sizeof(RecordHeader);

            // atomic write, fence required
            header = snapshot;
            Util::mfence();
        }

        // Delete rarely occurs, we put some heavy work in it
        auto Page::free(byte_ptr_t &ptr) noexcept -> void {
            auto page_address = Page::get_page(ptr);
            auto page_as_byte_ptr = reinterpret_cast<byte_ptr_t>(page_address);
            auto headers = get_headers();
            auto offset = ptr - page_as_byte_ptr;
            for (size_t i = 0; i < page_address->header.records; i++) {
                if (headers[i].offset == offset) {
                    headers[i].offset = 0;
                }
            }
            // Crash here is fine because on recovery, we scan the page
            // atomic write, outside fence required
            Util::mfence();
            --page_address->header.valid;
        }

        auto Allocator::allocate(int id, size_t size, byte_ptr_t &ptr) -> void {
            if (size > Constants::uPAGE_SIZE) {
                throw std::invalid_argument("Object size too large");
            }

            auto page = header.thread_busy_pages[id];
            // on start, or the page is on busy_list but freed(an allocation followed by a free)
            if (page)  {
                page->allocate(size, ptr);
                if (ptr != nullptr) {
                    return;
                }
            }
            
            {
                std::unique_lock l(allocator_global_lock);
                // busy page has no enough space and no thread-local free pages are available
                if (header.thread_free_lists[id] == nullptr) {
                    // the 1 is for current page
                    auto to_be_used = header.cursor + Constants::uPREALLOCATION + 1;
                    auto remain = header.base + (header.total_size / Constants::uPAGE_SIZE) - 1;
                    
                    if (to_be_used > remain) {
                        ptr = nullptr;
                        throw std::runtime_error("Insufficient PM\n");
                    } else {
                        if (header.freelist) {
                            // check global free list
                            auto begin = header.freelist;
                            auto end = header.freelist;
                            for (size_t i = 0; i < Constants::uPREALLOCATION; i++) {
                                if (end) {
                                    end = end->next;
                                }
                            }

                            // on recovery, should check
                            header.thread_free_lists[id] = begin;
                            header.freelist = end->next;
                            end->next = nullptr;
                        } else {
                            // from global heap
                            auto tmp = header.cursor;
                            for (size_t i = 0; i < Constants::uPREALLOCATION; i++) {
                                // dependent read/write;
                                Page::make_page(reinterpret_cast<byte_ptr_t>(tmp), tmp + 1);
                                tmp = tmp->next;
                            }
                            Page::make_page(reinterpret_cast<byte_ptr_t>(tmp), nullptr);
                            Util::mfence();
                            // on recovery, should check if any thread_free_list
                            // matches cursor, if so, cursor should be incremented
                            header.thread_free_lists[id] =  header.cursor;
                            Util::mfence();
                            header.cursor += Constants::uPREALLOCATION + 1; // next usable page
                        }
                    }
                }
            }

            // on recovery
            header.thread_busy_pages[id] = header.thread_free_lists[id];
            header.thread_free_lists[id] = header.thread_free_lists[id]->next;
            Util::mfence();                
            header.thread_busy_pages[id]->next = nullptr;
            Util::mfence();
                
            header.thread_busy_pages[id]->allocate(size, ptr);
        }

        auto Allocator::register_thread() noexcept -> std::optional<int> {
#ifdef __HILL_DEBUG__
            std::cout << ">> Registering in alloctor\n";
#endif
            std::scoped_lock<std::mutex> _(allocator_global_lock);
            for (int i = 0; i < Constants::iTHREAD_LIST_NUM; i++) {
                if (header.thread_free_lists[i] == Constants::pTHREAD_LIST_AVAILABLE ||
                    header.thread_busy_pages[i] == nullptr) {
                    if (header.thread_pending_pages[i] != nullptr) {
                        header.thread_busy_pages[i] = header.thread_pending_pages[i];
                        header.thread_pending_pages[i] = nullptr;
                    }
                    return i;
                }
            }
            return {};
        }
        
        auto Allocator::unregister_thread(int id) noexcept -> void {
            if (id < 0 || id > Constants::iTHREAD_LIST_NUM) {
                return;
            }

            // already unregistered
            if (!header.thread_busy_pages[id]) {
                return;
            }

            // on recovery, should check if any thread_pending_list matches thread_busy_page
            // if so, free list should be AVAILABLE
            header.thread_pending_pages[id] = header.thread_busy_pages[id];
            header.thread_busy_pages[id] = nullptr;
        }
        
        auto Allocator::free(int id, byte_ptr_t &ptr) -> void {
            if (!ptr)
                return;
            
            // auto page = reinterpret_cast<Page *>(reinterpret_cast<uint64_t>(ptr) & Constants::uPAGE_MASK);
            auto page = Page::get_page(ptr);
            // on recovery, should check
            header.to_be_freed[id] = page;
            Util::mfence();
            if (--page->header.valid == 0) {
                page->reset_cursor();
                if (header.thread_busy_pages[id] == page) {
                    header.to_be_freed[id] = nullptr;                     
                    return;
                }
                
                page->next = header.thread_free_lists[id];
                header.thread_free_lists[id] = page;
            }

            header.to_be_freed[id] = nullptr;
        }

        auto Allocator::recover() -> Enums::AllocatorRecoveryStatus {
            if (header.magic != Constants::uALLOCATOR_MAGIC) {
                return Enums::AllocatorRecoveryStatus::NoAllocator;
            }

            recover_pending_list();
            recover_global_heap();
            recover_free_lists();
            // recover_pending_list();
            recover_to_be_freed();
            
            return Enums::AllocatorRecoveryStatus::Ok;
        }
    }
}
