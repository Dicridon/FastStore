#include "cmd_parser.hpp"
namespace Hill {
    const std::string ib_port_full = "--ib_port";
    const std::string ib_port_short = "-p";
    const std::string socket_port_full = "--socket_port";
    const std::string socket_port_short = "-P";
    const std::string device_full = "--device";
    const std::string device_short = "-d";
    const std::string is_server_full = "--is_server";
    const std::string is_server_short = "-s";
    const std::string help_full = "--help";
    const std::string __missing__ = "__missing__";
    
    const std::string description = 
        "    rdma app command line parse\n";
    const std::string ib_port_doc = "";
    const std::string socket_port_doc = "";
    const std::string device_doc = "";
    const std::string is_server_doc = "";
    const std::string help_doc = "";
    
    bool ParseHelper(const StringVec &args, const std::string &full, const std::string &shrt, StringMap &parsed_args, bool boolean)  {
        auto finder = [&](std::string shrt, std::string full) {
            return [shrt, full](const std::string &in) {
                if (in == shrt || in == full)
                {
                    return true;
                }
                return false;
            };
        };
        
        auto ret = std::find_if(args.cbegin(), args.cend(), finder(shrt, full));
        if (ret != args.cend())
        {
            if (boolean)
            {
                parsed_args[full] = "true";
                return true;
            }
        
            auto distance = std::distance(args.cbegin(), ret);
            if (distance + 1 >= args.size())
            {
                std::cout << "Error: a value is expected for option " << *ret << "\n";
                return false;
            }
        
            auto value = args[distance + 1];
            if (value.rfind("-") == 0)
            {
                std::cout << "Error: a value is expected for option " << *ret << "\n";
                return false;
            }
            parsed_args[full] = value;
            return true;
        }
        return false;
    }
    
    void Cmdparser::Parse(char **s, char **e) noexcept {
        auto args = StringVec(s, e);
        std::string args_ib_port = "1";
        if (!ParseHelper(args, ib_port_full, ib_port_short, arg_map, false)) {
            arg_map[ib_port_full] = args_ib_port;
        }
        
        std::string args_socket_port = "6666";
        if (!ParseHelper(args, socket_port_full, socket_port_short, arg_map, false)) {
            arg_map[socket_port_full] = args_socket_port;
        }
        
        std::string args_device = "mlx5_1";
        if (!ParseHelper(args, device_full, device_short, arg_map, false)) {
            arg_map[device_full] = args_device;
        }
        
        std::string args_is_server = "true";
        if (!ParseHelper(args, is_server_full, is_server_short, arg_map, false)) {
            arg_map[is_server_full] = args_is_server;
        }
        
        std::string args_help = "false";
        if (!ParseHelper(args, help_full, __missing__, arg_map, true)) {
            arg_map[help_full] = args_help;
        }
        
    }
    
    void Cmdparser::Help() const noexcept {
        std::cout << "Cmdparser\n";
        std::cout << description;
        std::cout << "\nOPTIONS\n";
    }
    
    std::string Cmdparser::GetIbPort() const noexcept {
        return arg_map.find(ib_port_full)->second;
    }
    
    std::string Cmdparser::GetSocketPort() const noexcept {
        return arg_map.find(socket_port_full)->second;
    }
    
    std::string Cmdparser::GetDevice() const noexcept {
        return arg_map.find(device_full)->second;
    }
    
    std::string Cmdparser::GetIsServer() const noexcept {
        return arg_map.find(is_server_full)->second;
    }
    
    std::string Cmdparser::GetHelp() const noexcept {
        return arg_map.find(help_full)->second;
    }
}
