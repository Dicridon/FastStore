#include "remote_memory/remote_memory.hpp"

#include <iostream>
using namespace Hill::Memory;
int main() {
    byte_t tmp = 0;

    if (RemotePointer::is_remote_pointer(&tmp)) {
        std::cout << "Are you insane, it's not a remote pointer?\n";
        return -1;
    }

    auto remote = RemotePointer::make_remote_pointer(1, &tmp);

    if (remote.get_as<byte_ptr_t>() != &tmp) {
        std::cout << "They should be the same\n";
        return -1;
    }

    if (*remote.get_as<byte_ptr_t>() != 0) {
        std::cout << "It should be zero\n";
        return -1;
    }

    if (remote.get_node() != 1) {
        std::cout << "IT IS ON NODE 1!!!\n";
        return -1;
    }

    std::cout << "Tests passed\n";
}
