#include "memory_manager/memory_manager.hpp"

#include <iostream>

using namespace Hill::Memory;

int main() {
    constexpr size_t total = 3 * Constants::uPAGE_SIZE;
    
    auto space = new byte_t[total];

    auto allocator = Allocator::make_allocator(space, total);
    auto _id = allocator->register_thread();
    if (_id.has_value()) {
        auto id = _id.value();
        
        byte_ptr_t ptr = nullptr;
        allocator->allocate(id, Constants::uPAGE_SIZE - 1024, ptr);
        allocator->unregister_thread(id);
    } else {
        std::cout << "registeration failed\n";
    }

    _id = allocator->register_thread();

    if (_id.has_value()) {
        auto id = _id.value();
        byte_ptr_t ptr = nullptr;
        allocator->allocate(id, Constants::uPAGE_SIZE - 1024, ptr);
        allocator->free(id, ptr);
    }
}
