#include "engine.hpp"
namespace Hill {
    auto Engine::check_rdma_request() noexcept -> int {
        auto socket = Misc::accept_blocking(sock);

        if (socket == -1) {
            return -1;
        }

        // check node id, 0xff is a client
        auto remote_id = 0;
        read(socket, &remote_id, sizeof(remote_id));
        if (remote_id != Cluster::Constants::iCLIENT_ID) {
            if (remote_id < 0 || size_t(remote_id) >= Cluster::Constants::uMAX_NODE) {
                return -1;
            }

            if (peer_connections[remote_id] != nullptr) {
                return 0;
            }
        }

        auto [rdma, status] = RDMA::make_rdma(rdma_dev_name, ib_port, gid_idx);
        if (!rdma) {
            std::cerr << "Failed to create RDMA, error code: " << decode_rdma_status(status) << "\n";
            return -1;
        }

        if (auto err = rdma->default_connect(socket, base, node->available_pm); err != 0) {
            std::cerr << "RDMA connection failed\n";
            return err;
        }

        if (remote_id == Cluster::Constants::iCLIENT_ID) {
            // just keep this connection alive
            client_connections.push_back(std::move(rdma));
        } else {
            peer_connections[remote_id] = std::move(rdma);            
        }

        // no longer needed
        shutdown(socket, 0);
        return 0;
    }

    auto Engine::launch() noexcept -> void {
        node->launch();
        run = true;
        std::thread checker([&] {
            while(run) {
                check_rdma_request();
                sleep(1);
            }
        });
        checker.detach();
    }

    auto Engine::stop() noexcept -> void {
        run = false;
        node->stop();
        for (auto &p : peer_connections) {
            p.reset(nullptr);
        }

        for (auto &c : client_connections) {
            c.reset(nullptr);
        }
    }

    auto Engine::dump() const noexcept -> void {
        node->dump();
        std::cout << "---->> RDMA device: " << rdma_dev_name << "\n";
        std::cout << "---->> ib port: " << ib_port << "\n";
        std::cout << "---->> gid index: " << gid_idx << "\n";
    }

    auto Engine::parse_ib(const std::string &config) noexcept -> bool {
        auto content_ = Misc::file_as_string(config);
        if (!content_.has_value()) {
            return false;
        }
        auto content = content_.value();
        std::regex rdev_name("dev_name:\\s+(\\S+)");
        std::regex rib_port("ib_port:\\s+(\\d+)");
        std::regex rgid_idx("gid_idx:\\s+(\\d+)");

        std::smatch vdev_name, vib_port, vgid_idx;
        if (!std::regex_search(content, vdev_name, rdev_name)) {
            std::cerr << ">> Error: invalid or unspecified ib device name\n";
            return -1;
        }

        if (!std::regex_search(content, vib_port, rib_port)) {
            std::cerr << ">> Error: invalid or unspecified IB port\n";
            return -1;
        }

        if (!std::regex_search(content, vgid_idx, rgid_idx)) {
            std::cerr << ">> Error: invalid or unspecified GID index\n";
            return -1;
        }

        rdma_dev_name = vdev_name[1];
        ib_port = atoi(vib_port[1].str().c_str());
        gid_idx = atoi(vgid_idx[1].str().c_str());
        return true;
    }

    auto Client::connect_monitor() noexcept -> bool {
        run = true;
        monitor_socket = Misc::socket_connect(false, monitor_port, monitor_addr.to_string().c_str());
        if (monitor_socket == -1) {
            return false;
        }

        std::thread updater([&]() {
            size_t size = 0UL;
            read(monitor_socket, &size, sizeof(size));
            auto buf = std::make_unique<byte_t[]>(size);
            read(monitor_socket, buf.get(), size);
            meta.deserialize(buf.get());
            
            while(run) {
                size = meta.total_size();
                write(monitor_socket, &size, sizeof(size));
                buf = meta.serialize();
                write(monitor_socket, buf.get(), size);
                
                read(monitor_socket, &size, sizeof(size));
                buf = std::make_unique<byte_t[]>(size);
                read(monitor_socket, buf.get(), size);

                Cluster::ClusterMeta tmp;
                tmp.deserialize(buf.get());
                meta.update(tmp);
                meta.dump();
                sleep(1);
            }
        });
        updater.detach();
        return true;
    }

    auto Client::connect_server(int node_id) noexcept -> bool {
        if (node_id <= 0 || size_t(node_id) > Cluster::Constants::uMAX_NODE) {
            return false;
        }
        auto [rdma, status] = RDMA::make_rdma(rdma_dev_name, ib_port, gid_idx);
        if (!rdma) {
            std::cerr << "Failed to create RDMA, error code: " << decode_rdma_status(status) << "\n";
            return false;
        }

        auto addr = meta.cluster.nodes[node_id].addr;
        auto port = meta.cluster.nodes[node_id].port;

        auto socket = Misc::socket_connect(false, port, addr.to_string().c_str());
        if (socket == -1) {
            return false;
        }
        if (rdma->default_connect(socket, buf.get(), 16 * 1024) != 0) {
            return false;
        }
        return 0;
    }

    auto Client::parse_ib(const std::string &config) noexcept -> bool {
        auto content_ = Misc::file_as_string(config);
        if (!content_.has_value()) {
            return false;
        }
        auto content = content_.value();
        std::regex rdev_name("dev_name:\\s+(\\S+)");
        std::regex rib_port("ib_port:\\s+(\\d+)");
        std::regex rgid_idx("gid_idx:\\s+(\\d+)");

        std::smatch vdev_name, vib_port, vgid_idx;
        if (!std::regex_search(content, vdev_name, rdev_name)) {
            std::cerr << ">> Error: invalid or unspecified ib device name\n";
            return -1;
        }

        if (!std::regex_search(content, vib_port, rib_port)) {
            std::cerr << ">> Error: invalid or unspecified IB port\n";
            return -1;
        }

        if (!std::regex_search(content, vgid_idx, rgid_idx)) {
            std::cerr << ">> Error: invalid or unspecified GID index\n";
            return -1;
        }

        rdma_dev_name = vdev_name[1];
        ib_port = atoi(vib_port[1].str().c_str());
        gid_idx = atoi(vgid_idx[1].str().c_str());
        return true;
    }
    
}

