#include "remote_memory/remote_memory.hpp"

#include <iostream>

using namespace Hill;
using namespace Hill::Memory;
using namespace Hill::Memory::TypeAliases;

int main() {
    int nodeid = 1;
    byte_t alpha = 'a';
    byte_t beta = 'b';

    auto remote = RemotePointer::make_remote_pointer(nodeid, &alpha);

    auto poly_remote = PolymorphicPointer::make_ploymorphic_pointer(remote);
    auto poly_local = PolymorphicPointer::make_ploymorphic_pointer(&beta);

    if (!poly_remote.is_remote()) {
        std::cout << "It is remote!\n";
        return -1;
    }

    if (!poly_local.is_local()) {
        std::cout << "It is local!\n";
        return -1;
    }

    *poly_remote.remote_ptr().get_as<byte_ptr_t>() = 'r';
    if (*poly_remote.remote_ptr().get_as<byte_ptr_t>() != 'r') {
        std::cout << "It should be \'r\'";
        return -1;
    }

    *poly_local.local_ptr() = 'l';
    if (*poly_local.local_ptr() != 'l') {
        std::cout << "It should be \'l\'\n";
        return -1;
    }

    std::cout << "Tests passed\n";
    return 0;
}
