#include "wal.hpp"
namespace Hill {
    namespace WAL {
        auto LogRegion::recover(LogEntryAction action) noexcept -> page_vector_ptr {
            std::unordered_set<Memory::Page *> page_set;
            auto freed_pages = std::make_unique<std::vector<Memory::Page *>>();
            for (size_t i = checkpointed; i < cursor; i++) {
                if (entries[i].status == Enums::LogStatus::Uncommited && entries[i].address != 0) {
                    if (!action(recover_op(entries[i], page_set, freed_pages))) {
                        return nullptr;
                    }
                }
            }
            checkpointed = 0;
            Memory::Util::mfence();
            cursor = 0;
            return freed_pages;
        }

        auto LogRegion::recover_op(LogEntry &entry,
                                   std::unordered_set<Memory::Page *> &set,
                                   page_vector_ptr &freed_pages) noexcept -> LogEntry & {
            auto addr = entry.address;
            auto page_ptr = Memory::Page::get_page(addr);
            auto page_as_byte_ptr = reinterpret_cast<byte_ptr_t>(page_ptr);
            auto headers = page_ptr->get_headers();
            auto offset = addr - page_as_byte_ptr;

            for (size_t i = 0; i < page_ptr->header.records; i++) {
                if (headers[i].offset == offset) {
                    // always reclaim uncommited memory
                    headers[i].offset = 0;
                }
            }

            if (set.find(page_ptr) == set.end()) {
                auto ret = recover_page(page_ptr);
                set.insert(page_ptr);
                
                if (ret.has_value()) {
                    freed_pages->push_back(ret.value());
                }
            }

            return entry;
        }

        auto LogRegion::recover_page(Memory::Page *page_ptr) noexcept -> std::optional<Memory::Page *>{
            auto headers = page_ptr->get_headers();
            size_t valid = 0;
            for (size_t i = 0; i < page_ptr->header.records; i++) {
                if (headers[i].offset != 0) {
                    ++valid;
                }
            }

            if (valid == 0) {
                page_ptr->reset_cursor();
                return page_ptr;
            }

            page_ptr->header.valid = valid;
#ifdef PMEM
            pmem_persist(&page_ptr->header, sizeof(page_ptr->header));
#endif
            return {};
        }

        auto LogRegion::make_log(Enums::Ops op) noexcept ->  byte_ptr_t & {
            entries[cursor].address = nullptr;
            Memory::Util::mfence();
            entries[cursor].op = op;
            entries[cursor].status = Enums::LogStatus::Uncommited;
            Memory::Util::mfence();
            ++cursor;

            return entries[cursor].address;
        }

        auto LogRegion::checkpoint() noexcept -> void {
            for (size_t i = checkpointed; i < cursor; i++) {
                entries[i].commit();
            }
            checkpointed = 0;
            Memory::Util::mfence();            
            cursor = checkpointed;

#ifdef PMEM
            // checkpointed is not forced to persist since recover just replays the checkpointing
            pmem_persist(&cursor, sizeof(cursor));
#endif
        }

        auto Logger::register_thread() noexcept -> std::optional<int> {
            for (int i = 0; i < Constants::iREGION_NUM; i++) {
                if (in_use[i] == false) {
                    in_use[i] = true;
                    return i;
                }
            }
            return {};
        }

        auto Logger::unregister_thread(int id) noexcept -> void {
            regions->regions[id].checkpoint();
            in_use[id] = false;
            counters[id] = 0;
        }

        auto Logger::make_log(int id, Enums::Ops op) noexcept -> byte_ptr_t & {
            return regions->regions[id].make_log(op);
        }
        
        auto Logger::commit(int id) noexcept -> void {
            if (++counters[id] == Constants::uBATCH_SIZE) {
                regions->regions[id].checkpoint();
            }
        }
    }
}
