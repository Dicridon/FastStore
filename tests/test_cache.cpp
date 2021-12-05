#include "cmd_parser/cmd_parser.hpp"
#include "read_cache/read_cache.hpp"

#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace Hill;
using namespace Hill::ReadCache;
int main() {

    const char *t = "I";
    std::cout << t << "\n";
    Cache cache(5);
    int holder[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (int i = 0; i < 10; i++) {
        auto poly = PolymorphicPointer::make_polymorphic_pointer(&holder[i]);
        cache.insert(std::to_string(i), poly, i);
    }

    auto i = cache.get("5");
    std::cout << i->key << "\n";
    sleep(2);
    i = cache.get("5");
    if (i != nullptr) {
        std::cout << "Cache is not timely updated\n";
    }
}
