#include "memory_manager/memory_manager.hpp"

#include <iostream>

using namespace Hill::Memory;

int main() {
    constexpr size_t total = 3 * Constants::uPAGE_SIZE;
    
    auto space = new byte_t[total];

    auto allocator = Allocator::make_allocator(space, total);
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
    for (int i = 0; i < 10; i++) {
        allocator->allocate(tid, 34, ptr);
        if (i == 0)
            *(uint64_t *)ptr = 1234554321UL + i;
        allocator->free(tid, ptr);
    }

    std::cout << *(uint64_t *)ptr << "\n";
}
