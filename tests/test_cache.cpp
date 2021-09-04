#include "cmd_parser/cmd_parser.hpp"

#include <iostream>
#include <fstream>
#include <unordered_map>


static constexpr size_t cache_cap = 1000;
class HashCache {
public:
    explicit HashCache() : hit(0), total(0) {
        for (auto &c : content) {
            c = nullptr;
        }
    };
    ~HashCache() = default;

    auto get(const std::string &in) noexcept -> void {
        ++total;
        auto pos = hasher(in) % cache_cap;
        if (content[pos] != nullptr) {
            if (*content[pos] == in)
                ++hit;
        }
        
        content[pos] = &in;
    }

    auto report() const noexcept -> void {
        std::cout << ">> Hit ratio of HashCache is: " << double(hit) / total << "\n";
    }
    
private:
    static constexpr std::hash<std::string> hasher{};
    size_t hit;
    size_t total;
    
    const std::string *content[cache_cap];
};

struct LinkedListNode {
    const std::string *content;
    LinkedListNode *prev;
    LinkedListNode *next;
};

class HotHashCache {
public:
    explicit HotHashCache() : hit(0), total(0) {
        for (size_t i = 0; i < cache_cap / 2; i++) {
            first[i] = second[i] = nullptr;
            hot[i] = 0;
        }
    }

    auto get(const std::string &in) noexcept -> void {
        ++total;
        auto pos = hasher(in) % (cache_cap / 2);
        // first[pos] == nullptr while second[pos] != nullptr is impossible
        if (first[pos] == nullptr) {
            first[pos] = &in;
            hot[pos] = 0;
            return;
        }

        if(*first[pos] != in) {
            if (second[pos] == nullptr) {
                second[pos] = &in;
                hot[pos] = 1;
                return;
            }
        } else {
            ++hit;
            return;
        }

        // now first and second are all non-null
        if (*second[pos] != in) {
            if (hot[pos] == 0) {
                second[pos] = &in;
                hot[pos] = 1;
            } else {
                first[pos] = &in;
                hot[pos] = 0;
            }
        } else {
            ++hit;
        }
    }

    auto report() const noexcept -> void {
        std::cout << ">> Hit ratio of HotHashCache is: " << double(hit) / total << "\n";
    }
private:
    static constexpr std::hash<std::string> hasher{};
    size_t hit;
    size_t total;

    const std::string *first[cache_cap / 2];
    const std::string *second[cache_cap / 2];
    int8_t hot[cache_cap];
};

class LRUCache {
public:
    LRUCache() : cap(0), hit(0), total(0) {
        header.content = nullptr;
        header.prev = &header;
        header.next = &header;
    }

    auto get(const std::string &in) noexcept -> void {
        // std::cout << "caught " << in << "\n";
        ++total;
        if (auto pair = map.find(in); pair != map.end()) {
            ++hit;
            // LRU move
            auto node = pair->second;
            node->next->prev = node->prev;
            node->prev->next = node->next;

            node->next = header.next;
            node->prev = &header;
            header.next->prev = node;
            header.next = node;

            // print_list();
            return;
        }

        if (cap == cache_cap) {
            auto tail = header.prev;
            map.erase(*tail->content);
            tail->next->prev = tail->prev;
            tail->prev->next = tail->next;
            delete tail;

            --cap;
        }

        if (cap < cache_cap) {
            // add to head
            auto new_node = new LinkedListNode;
            new_node->content = &in;
            
            new_node->next = header.next;
            new_node->prev = &header;
            header.next->prev = new_node;            
            header.next = new_node;
            
            map[in] = new_node;
            ++cap;            
        }
        // print_list();
    }

    auto report() const noexcept -> void {
        std::cout << ">> Hit ratio of LRUCache is: " << double(hit) / total << "\n";        
    }

    auto print_list() const noexcept -> void {
        auto begin = header.next;
        while (begin != &header) {
            std::cout << *begin->content << "\n";
            begin = begin->next;
        }
    }
    
private:
    LinkedListNode header;
    std::unordered_map<std::string , LinkedListNode *> map;
    size_t cap;
    size_t hit;
    size_t total;
};

int main() {
    HashCache hash_cache;
    LRUCache lru;
    HotHashCache hot_cache;

    std::vector<std::string> vec;

    std::ifstream in_file("./third-party/data");
    std::string buf;
    while(std::getline(in_file, buf)) {
        vec.push_back(buf);
    }

    for (const auto &s : vec) {
        hash_cache.get(s);
    }
    hash_cache.report();

    for (const auto &s : vec) {
        lru.get(s);
    }
    lru.report();

    for (const auto &s : vec) {
        hot_cache.get(s);
    }
    hot_cache.report();
}
