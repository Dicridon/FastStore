#ifndef __HILL__CMDPARSER__
#define __HILL__CMDPARSER__

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <regex>

namespace CmdParser {
    namespace CmdParserValues {
        class BasicValue {
        public:
            BasicValue() = default;
            virtual ~BasicValue() = default;
        };

        template <typename T>
        class GenericValue : public BasicValue {
        public:
            GenericValue() = default;
            GenericValue(const GenericValue &) = default;
            GenericValue(GenericValue &&) = default;            
            GenericValue(const T& t) : content(t) {};
            GenericValue(T &&t) : content(t) {};
            ~GenericValue() = default;
            
            auto operator=(const GenericValue<T> &) -> GenericValue<T>& = default;
            auto operator=(GenericValue<T>&&) -> GenericValue<T>& = default;
            auto operator=(const T& t) -> GenericValue<T>& {
                content = t;
                return *this;
            }
            auto operator=(T&& t) -> GenericValue<T>& {
                content = t;
                return *this;                
            }

            auto unwrap() -> T& {
                return content;
            }

            auto unwrap_const() const -> const T& {
                return content;
            }

        private:
            T content;
        };
    }

    namespace CmdParserOptions {
        using namespace CmdParserValues;

        // long_name is used as index to find an option
        class BasicOption : public std::enable_shared_from_this<BasicOption> {
        protected:
            std::string full_name;
            std::string short_name;
            bool is_switch_;

        public:
            BasicOption() : is_switch_(false) {};
            BasicOption(const std::string &f, const std::string s, bool is = false) : full_name(f), short_name(s), is_switch_(is) {};
            virtual ~BasicOption() = default;

            auto is_switch() const noexcept -> bool {
                return is_switch_;
            }

            auto get_full_name() const noexcept -> const std::string & {
                return full_name;
            }

            auto get_short_name() const noexcept -> const std::string & {
                return short_name;
            }
        };

        template<typename T>
        class SingleOption : public BasicOption {
        public:
            SingleOption() : BasicOption() {};
            SingleOption(const std::string &f, const std::string &s) : BasicOption(f, s) {};
            SingleOption(const std::string &f, const std::string &s, const T& in) : BasicOption(f, s), value(in) {};
            SingleOption(const SingleOption &) = default;
            SingleOption(SingleOption &&) = default;
            ~SingleOption() = default;
            
            auto operator=(const SingleOption<T>&) -> SingleOption<T>& = default;
            auto operator=(SingleOption<T>&&) -> SingleOption<T>& = default;

            auto operator=(const T& t) -> SingleOption<T>& {
                value = t;
                return *this;
            }
            auto operator=(T&& t) -> SingleOption<T>& {
                value = t;
                return *this;                
            }
            
            auto get_value() -> T& {
                return value.unwrap();
            }

            auto get_value_const() -> const T& {
                return value.unwrap_const();
            }
        private:
            GenericValue<T> value;
        };

        class SwitchOption : public BasicOption {
        public:
            SwitchOption() : BasicOption(), value(false) {};
            SwitchOption(const std::string &f, const std::string &s, bool in = false) : BasicOption(f, s, true), value(in) {};
            SwitchOption(const SwitchOption &) = default;
            SwitchOption(SwitchOption &&) = default;            
            ~SwitchOption() = default;

            SwitchOption& operator=(const bool& t) {
                value = t;
                return *this;
            }

            SwitchOption& operator=(bool&& t) {
                value = t;
                return *this;
            }

            auto is_true() const noexcept -> bool {
                return value.unwrap_const() == true;
            }

            auto is_false() const noexcept -> bool {
                return value.unwrap_const() == false;
            }

        private:
            GenericValue<bool> value;
        };
    }

    class Parser {
    public:
        using BasicOptionPtr = std::shared_ptr<CmdParserOptions::BasicOption>;
        using OptionMap = std::unordered_map<std::string, std::string>;
        using RegexMap = std::unordered_map<std::string, std::pair<std::string, std::string>>;
        using ParsedOptionMap = std::unordered_map<std::string, BasicOptionPtr>;

        Parser() = default;
        Parser(const Parser &) =default;
        Parser(Parser &&) =default;
        auto operator=(const Parser &) -> Parser & = default;
        auto operator=( Parser &&) -> Parser & = default;
        ~Parser() = default;

        auto add_switch(const std::string &full, const std::string &shrt) -> bool {
            return regex_map.insert({full, {full + regex_long_suffix, shrt + regex_short_suffix}}).second;
        }
        
        auto add_switch(const std::string &full, const std::string &shrt, bool default_value) -> bool {
            auto pair = std::make_pair(full + regex_long_suffix, shrt + regex_short_suffix);

            BasicOptionPtr ptr = CmdParserOptions::SwitchOption(full, shrt, default_value).shared_from_this();
            if (auto ret = regex_map.insert({full, pair}).second; ret) {
                return parsed_map.insert({full, ptr}).second;
            }
            
            return false;
        }

        auto add_option(const std::string &full, const std::string &shrt) -> bool {
            return regex_map.insert({full, {full + regex_long_suffix, shrt + regex_short_suffix}}).second;
        }
        
        template<typename T>
        auto add_option(const std::string &full, const std::string &shrt, const T& default_value) -> bool {
            auto pair = std::make_pair(full + regex_long_suffix, shrt + regex_short_suffix);
            BasicOptionPtr ptr = CmdParserOptions::SingleOption<T>(default_value).shared_from_this();

            if (auto ret = regex_map.insert({full, pair}).second; ret) {
                return parsed_map.insert({full, ptr}).second;
            }
            return false;
        }

        auto parse(int argc, char *argv[]) -> void {
            
        }
    private:
        const std::string regex_long_suffix = "=(\\S+)";
        const std::string regex_short_suffix = "\\s+(\\S+)";
        OptionMap plain_map;
        RegexMap regex_map;
        ParsedOptionMap parsed_map;
    };

}
#endif
