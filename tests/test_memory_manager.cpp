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

    byte_ptr_t ptr = nullptr;
    std::string data("This is some data\n");
    allocator->allocate(_id.value(), Constants::uPAGE_SIZE - 128, ptr);
    allocator->allocate(_id.value(), 64, ptr);    
    memcpy(ptr, data.c_str(), data.size());

    char buf[128];
    memcpy(buf, ptr, data.size());
    allocator->free(_id.value(), ptr);
    std::cout << "seems works\n";
}
