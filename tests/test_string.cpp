#include "kv_pair/kv_pair.hpp"

#include <iostream>

#include <cassert>

using namespace Hill::KVPair;
int main() {
    auto buf1 = new byte_t[1024];
    auto buf2 = new byte_t[1024];

    auto str1 = &HillString::make_string(buf1, "abcd", 4);
    auto str2 = &HillString::make_string(buf2, "abcd", 4);
    assert(*str1 == *str2);
    assert(*str1 <= *str2);
    assert(*str1 >= *str2);

    str2 = &HillString::make_string(buf2, "abce", 4);
    assert(*str1 < *str2);
    assert(*str2 > *str1);
    assert(*str1 <= *str2);
    assert(*str2 >= *str1);

    str2 = &HillString::make_string(buf2, "abcde", 5);
    assert(*str1 < *str2);
    assert(*str2 > *str1);
    assert(*str1 <= *str2);
    assert(*str2 >= *str1);

    assert(str1->compare("abcd", 4) == 0);
    assert(str1->compare("abcc", 4) > 0);
    assert(str1->compare("abce", 4) < 0);
    assert(str1->compare("abcde", 5) < 0);
    assert(str1->compare("abc", 3) > 0);
    std::cout <<"Succeded\n";
}
