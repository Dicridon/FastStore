#include "wal/wal.hpp"
#include "memory_manager/memory_manager.hpp"

#include <iostream>

using namespace Hill;
using namespace Hill::WAL;
using namespace Hill::Memory;


// 2020.8.19: More test to go
int main() {
    byte_ptr_t region = new byte_t[1024 * 1024 * 64];

    byte_ptr_t memory = new byte_t[1024 * 1024 * 128];
    auto logger = Logger::make_unique_logger(region);
    auto alloc = Allocator::make_allocator(memory, 1024 * 1024 * 128);

    auto _log_id = logger->register_thread();
    auto _mem_id = alloc->register_thread();
    if (!_log_id.has_value()) {
        std::cout << "Can not register thread for logger\n";
    }
    if (!_mem_id.has_value()) {
        std::cout << "Can not register thread for memory\n";
    }


    auto log_id = _log_id.value();
    auto mem_id = _mem_id.value();
    for (size_t i = 0; i < Hill::WAL::Constants::uBATCH_SIZE; i++) {
        auto &addr = logger->make_log(log_id, WAL::Enums::Ops::Insert);
        alloc->allocate(mem_id , 16, addr);
        *((size_t *)addr) = i;
        std::cout << ">> reading " << *((size_t *)addr) << "\n";
    }
}
