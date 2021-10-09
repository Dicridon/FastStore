#ifndef __HILL__KV_PAIR__KV_PAIR__
#define __HILL__KV_PAIR__KV_PAIR__
#include "memory_manager/memory_manager.hpp"
#include <cstring>

namespace Hill {
    namespace KVPair {
        using namespace ::Hill::Memory::TypeAliases;

        struct HillString;
        namespace TypeAliases {
            using hill_key_t = HillString;
            using hill_value_t = HillString;
        }

        struct HillStringHeader {
            uint16_t valid : 1;
            uint16_t length : 15;
        };
        
        /*
         * This is a simple compact string implementation
         * The length field should be copied to the index to avoid unnecessary PM accesses
         * 
         */
        struct HillString {
            HillStringHeader header;
            // not [0] so no warning. 
            byte_t content[1];

            static auto make_string(const byte_ptr_t &chunk, const_byte_ptr_t bytes, size_t size) -> HillString & {
                auto ret = reinterpret_cast<HillString *>(chunk);
                ret->header.length = size;
                memcpy(&ret->content, bytes, size);
                ret->header.valid = 1;
                return *ret;
            }

            static auto make_string(const byte_ptr_t &chunk, const char *bytes, size_t size) -> HillString & {
                return make_string(chunk, reinterpret_cast<const_byte_ptr_t>(bytes), size);
            }

            auto compare(const char *rhs, size_t r_sz) -> int {
                auto chars = raw_chars();
                auto bound = std::min(size(), r_sz);
                if (auto ret = strncmp(chars, rhs, bound); ret == 0) {
                    return size() - r_sz;
                } else {
                    return ret;
                }
            }

            auto operator==(const HillString &rhs) -> bool {
                if (header.length != rhs.header.length) {
                    return false;
                }
                
                for (size_t i = 0; i < header.length; i++) {
                    if (content[i] != rhs.content[i]) {
                        return false;                        
                    }
                }
                return true;
            }

            auto operator!=(const HillString &rhs) -> bool {
                return !(*this == rhs);
            }

            auto operator<(const HillString &rhs) -> bool {
                for (size_t i = 0; i < std::min(header.length, rhs.header.length); i++) {
                    if (content[i] > rhs.content[i]) {
                        return false;
                    }

                    if (content[i] < rhs.content[i]) {
                        return true;
                    }
                }

                return header.length < rhs.header.length;
            }

            auto operator>(const HillString &rhs) -> bool {
                for (size_t i = 0; i < std::min(header.length, rhs.header.length); i++) {
                    if (content[i] > rhs.content[i]) {
                        return true;
                    }

                    if (content[i] < rhs.content[i]) {
                        return false;
                    }
                }

                return header.length > rhs.header.length;
            }

            auto operator<=(const HillString &rhs) -> bool {
                for (size_t i = 0; i < std::min(header.length, rhs.header.length); i++) {
                    if (content[i] > rhs.content[i]) {
                        return false;
                    }
                }
                return true;
            }

            auto operator>=(const HillString &rhs) -> bool {
                for (size_t i = 0; i < std::min(header.length, rhs.header.length); i++) {
                    if (content[i] < rhs.content[i]) {
                        return false;
                    }
                }
                return true;
            }            

            inline auto is_valid() const noexcept -> bool {
                return header.valid;
            }

            inline auto validate() noexcept -> void {
                header.valid = 1;
            }

            inline auto invalidate() noexcept -> void {
                header.valid = 0;
            }

            inline auto raw_bytes() const noexcept -> const_byte_ptr_t {
                return &content[0];
            }

            inline auto raw_chars() const noexcept -> const char * {
                return reinterpret_cast<const char *>(&content[0]);
            }

            inline auto size() const noexcept -> size_t {
                return header.length;
            }

            inline auto inplace_update(const_byte_ptr_t bytes, size_t size) noexcept -> bool {
                if (size > header.length)
                    return false;

                memcpy(&content, bytes, size);
                header.length = size;
                return true;
            }
            
            inline auto inplace_update(const char *bytes, size_t size) noexcept -> bool {
                return inplace_update(reinterpret_cast<const_byte_ptr_t>(bytes), size);
            }
            HillString() = delete;
            ~HillString() = default;
            HillString(const HillString &) = delete;
            HillString(HillString &&) = delete;
            auto operator=(const HillString &) = delete;
            auto operator=(HillString &&) = delete;
        };
    }
}
#endif
