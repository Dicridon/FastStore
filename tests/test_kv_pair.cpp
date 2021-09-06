#include "kv_pair/kv_pair.hpp"

#include <iostream>
using namespace Hill;
using namespace Hill::Memory::TypeAliases;
int main() {
    auto buf =new byte_t[1024];
    std::string cnt("This is some message");

    auto &s = KVPair::HillString::make_string(buf, cnt.c_str(), cnt.size());

    if (cnt.compare(0, cnt.size(), s.raw_chars(), 0, s.size()) != 0) {
        std::cout << "content mismatched\n";
        return -1;
    }

    if (cnt.size() != s.size()) {
        std::cout << "size mismatched\n";
        return -1;
    }
    return 0;
}
