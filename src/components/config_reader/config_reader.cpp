#include "config_reader.hpp"

#include <iostream>
namespace Hill {
    auto ConfigReader::read_node_id(const std::string &content) -> std::optional<int> {
        std::regex rnode_id("node_id:\\s*(\\d+)");
        std::smatch vnode_id;

        if (!std::regex_search(content, vnode_id, rnode_id)) {
            std::cerr << ">> Error: invalid or unspecified node id\n";
            return {};
        }

        return atoi(vnode_id[1].str().c_str());
    }
    
    auto ConfigReader::read_pmem_file(const std::string &content) -> std::optional<std::string> {
        std::regex rpmem_file("pmem_file:\\s+(\\S+)");
        std::smatch vpmem_file;
        
        if (!std::regex_search(content, vpmem_file, rpmem_file)) {
            std::cerr << ">> Error: invalid or unspecified pmem file\n";
            return {};
        }
        
        return vpmem_file[1];
    }
    
    auto ConfigReader::read_total_pm(const std::string &content) -> std::optional<size_t> {
        std::regex rtotal_pm("total_pm:\\s*(\\d+)");
        std::smatch vtotal_pm;
        if (!std::regex_search(content, vtotal_pm, rtotal_pm)) {
            std::cerr << ">> Error: invalid or unspecified total PM\n";
            return {};
        }

        return atoll(vtotal_pm[1].str().c_str());
    }
    
    auto ConfigReader::read_available_pm(const std::string &content) -> std::optional<size_t> {
        std::regex ravailable_pm("available_pm:\\s*(\\d+)");
        std::smatch vavailable_pm;
        if (!std::regex_search(content, vavailable_pm, ravailable_pm)) {
            std::cerr << ">> Error: invalid or unspecified available PM\n";
            return {};
        }

        return atoll(vavailable_pm[1].str().c_str());
    }
    
    auto ConfigReader::read_ip_addr(const std::string &content) -> std::optional<std::string> {
        std::regex raddr("addr:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
        std::smatch vaddr;
        if (!std::regex_search(content, vaddr, raddr)) {
            std::cerr << ">> Error: invalid or unspecified IP address\n";
            return {};
        }

        return vaddr[1].str();
    }
    
    auto ConfigReader::read_ip_port(const std::string &content) -> std::optional<int> {
        std::regex raddr("addr:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
        std::smatch vaddr;
        if (!std::regex_search(content, vaddr, raddr)) {
            std::cerr << ">> Error: invalid or unspecified IP port\n";
            return {};
        }

        return atoi(vaddr[2].str().c_str());
    }
    
    auto ConfigReader::read_erpc_port(const std::string &content) -> std::optional<int> {
        std::regex rerpc_port("erpc_port:\\s*(\\d+)");
        std::smatch verpc_port;
        if (!std::regex_search(content, verpc_port, rerpc_port)) {
            std::cerr << ">> Error: invalid or unspecified RPC port\n";
            return {};
        }

        return atoi(verpc_port.str().c_str());
    }
    
    auto ConfigReader::read_erpc_listen_port(const std::string &content) -> std::optional<int> {
        std::regex rerpc_listen_port("erpc_listen_port:\\s*(\\d+)");
        std::smatch verpc_listen_port;
        if (!std::regex_search(content, verpc_listen_port, rerpc_listen_port)) {
            std::cerr << ">> Error: invalid or unspecified RPC listen port\n";
            return {};
        }

        return atoi(verpc_listen_port[1].str().c_str());
    }
    
    auto ConfigReader::read_monitor_addr(const std::string &content) -> std::optional<std::string> {
        std::regex rmonitor("monitor:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
        std::smatch vmonitor;
        if (!std::regex_search(content, vmonitor, rmonitor)) {
            std::cerr << ">> Error: invalid or unspecified monitor\n";
            return {};
        }

        return vmonitor[1].str();
    }
    
    auto ConfigReader::read_monitor_port(const std::string &content) -> std::optional<int> {
        std::regex rmonitor("monitor:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
        std::smatch vmonitor;
        if (!std::regex_search(content, vmonitor, rmonitor)) {
            std::cerr << ">> Error: invalid or unspecified monitor\n";
            return {};
        }

        return atoi(vmonitor[2].str().c_str());
    }

    // for monitor
    // Monitor loops on regex matching, thus no method is offered here

    // for client
    auto ConfigReader::read_rpc_uri(const std::string &content) -> std::optional<std::string> {
        std::regex rrpc_uri("rpc_uri:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}:\\d+)");
        std::smatch vrpc_uri;
        if (!std::regex_search(content, vrpc_uri, rrpc_uri)) {
            std::cerr << ">> Error: invalid or unspecified uri\n";
            return {};
        }

        return vrpc_uri[1].str();
    }

    auto ConfigReader::read_ib_dev_name(const std::string &content) -> std::optional<std::string> {
        std::regex rdev_name("dev_name:\\s+(\\S+)");
        std::smatch vdev_name;
        if (!std::regex_search(content, vdev_name, rdev_name)) {
            std::cerr << ">> Error: invalid or unspecified ib device name\n";
            return {};
        }

        return vdev_name[1].str();
    }
    
    auto ConfigReader::read_ib_port(const std::string &content) -> std::optional<int> {
        std::regex rib_port("ib_port:\\s+(\\d+)");

        std::smatch vib_port;
        if (!std::regex_search(content, vib_port, rib_port)) {
            std::cerr << ">> Error: invalid or unspecified IB port\n";
            return {};
        }

        return atoi(vib_port[1].str().c_str());
    }
    
    auto ConfigReader::read_gid_idx(const std::string &content) -> std::optional<int> {
        std::regex rgid_idx("gid_idx:\\s+(\\d+)");
        std::smatch vgid_idx;

        if (!std::regex_search(content, vgid_idx, rgid_idx)) {
            std::cerr << ">> Error: invalid or unspecified GID index\n";
            return {};
        }

        return atoi(vgid_idx[1].str().c_str());
    }
}
