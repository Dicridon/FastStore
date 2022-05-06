#include "memory_manager/memory_manager.hpp"

#include <iostream>
#include <chrono>
#include <vector>

#include <libpmem.h>

using namespace Hill::Memory;

int main() {
    size_t total = 1024 * 1024 * 1024;
    size_t mapped_size = 0;
    auto base = reinterpret_cast<byte_ptr_t>(pmem_map_file("/mnt/pmem2/pm",
                                                           total,
                                                           PMEM_FILE_CREATE, 0666,
                                                           &mapped_size, nullptr));
    if (base == nullptr) {
        std::cout << ">> Unable to map pmem file\n";
        std::cout << ">> Errno is " << errno << ": " << strerror(errno) << "\n";
        return -1;
    } else {
        std::cout << ">> " << mapped_size / 1024 / 1024 / 1024.0 << "GB pmem is mapped at "
                  << reinterpret_cast<void *>(base) << "\n";
    }
    
    auto allocator = Allocator::make_allocator(base, total);
    if (!allocator) {
        std::cout << "Allocation created failed\n";
        return -1;
    }
    
    auto _id = allocator->register_thread();
    if (!_id.has_value()) {
        std::cout << "Unable to register thread\n";
        return -1;
    }

    auto tid = _id.value();
    byte_ptr_t ptr = nullptr;

    std::vector<size_t> chunks {32, 64, 128, 512, 1024};
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    for (auto &c : chunks) {
        start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; i++) {
            allocator->allocate(tid, c, ptr);
        }

        end = std::chrono::steady_clock::now();
        std::cout << "HA takes " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "\n";

        start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; i++) {
            ptr = new byte_t[c];
        }
        end = std::chrono::steady_clock::now();
        std::cout << "Malloc takes " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "\n";

        
        
    }
}
