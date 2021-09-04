#ifndef __HILL__KV_PAIR__KV_PAIR__
#define __HILL__KV_PAIR__KV_PAIR__
#include "memory_manager/memory_manager.hpp"

namespace Hill {
    namespace KVPair {
        using namespace ::Hill::Memory::TypeAliases;

        struct HillString;
        namespace TypeAliases {
            using hill_key_t = HillString;
            using hill_value_t = HillString;
        }

        /*
         * This is a simple compact string implementation
         * The length field should be copied to the index to avoid unnecessary PM accesses
         * 
         */
        struct HillString {
            uint16_t valid : 1;
            uint16_t length : 15;
            byte_t content[1];

            static auto make_string(const byte_ptr_t &chunk, const_byte_ptr_t bytes, size_t size) -> HillString & {
                auto ret = reinterpret_cast<HillString *>(chunk);
                ret->length = size;
                memcpy(&ret->content, bytes, size);
                ret->valid = 1;
                return *ret;
            }

            static auto make_string(const byte_ptr_t &chunk, const char *bytes, size_t size) -> HillString & {
                return make_string(chunk, reinterpret_cast<const_byte_ptr_t>(bytes), size);
            }

            inline auto is_valid() const noexcept -> bool {
                return valid;
            }

            inline auto validate() noexcept -> void {
                valid = 1;
            }

            inline auto invalidate() noexcept -> void {
                valid = 0;
            }

            inline auto raw_bytes() const noexcept -> const_byte_ptr_t {
                return &content[0];
            }

            inline auto raw_chars() const noexcept -> const char * {
                return reinterpret_cast<const char *>(&content[0]);
            }

            inline auto size() const noexcept -> size_t {
                return length;
            }

            inline auto inplace_update(const_byte_ptr_t bytes, size_t size) noexcept -> bool {
                if (size > length)
                    return false;

                memcpy(&content, bytes, size);
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
