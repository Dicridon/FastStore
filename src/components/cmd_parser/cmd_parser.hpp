#ifndef __HILL__CMDPARSER__
#define __HILL__CMDPARSER__

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
namespace Hill {
    using StringMap = std::unordered_map<std::string, std::string>;
    using StringVec = std::vector<std::string>;
    class Cmdparser {
        public:
            Cmdparser() = default;
            Cmdparser(Cmdparser &) = delete;
            Cmdparser(Cmdparser &&) = delete;
            
            void Parse(char **s, char **e) noexcept;
            
            void Help() const noexcept;
            
            std::string GetIbPort() const noexcept;
            
            std::string GetSocketPort() const noexcept;
            
            std::string GetDevice() const noexcept;
            
            std::string GetIsServer() const noexcept;
            
            std::string GetHelp() const noexcept;

        private:
            StringMap arg_map;

    };
}
#endif
