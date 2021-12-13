#ifndef __HILL__CONFIG_READER__CONFIG_READER__
#define __HILL__CONFIG_READER__CONFIG_READER__
#include <string>
#include <regex>
#include <optional>
namespace Hill {

    // all methods return a std::optional and I'll just let it crash if value is invalid
    class ConfigReader {
    public:
        ConfigReader() = delete;
        ~ConfigReader() = delete;
        // for engine
        static auto read_node_id(const std::string &content) -> std::optional<int>;
        static auto read_pmem_file(const std::string &content) -> std::optional<std::string>;
        static auto read_total_pm(const std::string &content) -> std::optional<size_t>;
        static auto read_available_pm(const std::string &content) -> std::optional<size_t>;
        static auto read_ip_addr(const std::string &content) -> std::optional<std::string>;
        static auto read_ip_port(const std::string &content) -> std::optional<int>;
        static auto read_erpc_port(const std::string &content) -> std::optional<int>;
        static auto read_erpc_listen_port(const std::string &content) -> std::optional<int>;
        static auto read_monitor_addr(const std::string &content) -> std::optional<std::string>;
        static auto read_monitor_port(const std::string &content) -> std::optional<int>;

        // for monitor
        // Monitor loops on regex matching, thus no method is offered here

        // for client
        static auto read_rpc_uri(const std::string &content) -> std::optional<std::string>;

        // ib device
        static auto read_ib_dev_name(const std::string &content) -> std::optional<std::string>;
        static auto read_ib_port(const std::string &content) -> std::optional<int>;
        static auto read_gid_idx(const std::string &content) -> std::optional<int>;
    };
}
#endif
