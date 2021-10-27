#include "engine.hpp"
namespace Hill {
    auto Engine::check_rdma_request(int tid) noexcept -> int {
        auto socket = Misc::accept_nonblocking(sock);

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

            if (peer_connections[tid][remote_id] != nullptr) {
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
            client_connections[tid].push_back(std::move(rdma));
        } else {
            peer_connections[tid][remote_id] = std::move(rdma);            
        }

        // no longer needed
        shutdown(socket, 0);
        return 0;
    }

    auto Engine::launch() noexcept -> bool {
        sock = Misc::make_socket(true, node->port);
        if (sock == -1) {
            return false;
        }
        auto flags = fcntl(sock, F_GETFL);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        return run = node->launch();
    }

    auto Engine::stop() noexcept -> void {
        run = false;
        node->stop();
        for (auto &_p : peer_connections) {
            for (auto &p : _p)
                p.reset(nullptr);
        }

        for (auto &_c : client_connections) {
            for (auto &c : _c)
                c.reset(nullptr);
        }
    }

    auto Engine::register_thread() -> std::optional<int> {
        auto l_tid = logger->register_thread();
        if (!l_tid.has_value()) {
            return {};
        }

        auto a_tid = allocator->register_thread();
        if (!a_tid.has_value()) {
            return {};
        }

        if (l_tid.value() != a_tid.value()) {
            allocator->unregister_thread(a_tid.value());
            logger->unregister_thread(l_tid.value());
            return {};
        }

        auto tid = a_tid.value();
        
        std::thread checker([&, tid] {
            while(this->run) {
                check_rdma_request(tid);
                sleep(1);
            }
        });
        checker.detach();

        return tid;
    }

    auto Engine::unregister_thread(int tid) -> void {
        logger->unregister_thread(tid);
        allocator->unregister_thread(tid);
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
            Misc::recv_all(monitor_socket, &size, sizeof(size));
            auto buf = std::make_unique<byte_t[]>(size);
            Misc::recv_all(monitor_socket, buf.get(), size);
            meta.deserialize(buf.get());
            
            while(run) {
                size = meta.total_size();
                Misc::send_all(monitor_socket, &size, sizeof(size));
                buf = meta.serialize();
                Misc::send_all(monitor_socket, buf.get(), size);
                
                Misc::recv_all(monitor_socket, &size, sizeof(size));
                buf = std::make_unique<byte_t[]>(size);
                Misc::recv_all(monitor_socket, buf.get(), size);

                Cluster::ClusterMeta tmp;
                tmp.deserialize(buf.get());
                meta.update(tmp);
#ifdef __HILL_DEBUG__
                meta.dump();
#endif                
                sleep(1);
            }
        });
        updater.detach();

        // ensure that working threads see a valid cluster meta
        while(meta.version == 0);
        return true;
    }

    auto Client::register_thread() -> std::optional<int> {
        static std::atomic_int tid = 0;
        if (tid < Memory::Constants::iTHREAD_LIST_NUM) {
            return tid.fetch_add(1);
        }
        return {};
    }

    auto Client::unregister_thread([[maybe_unused]] int tid) -> void {

    }
    
    auto Client::connect_server(int tid, int node_id) noexcept -> bool {
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
        write(socket, &Cluster::Constants::iCLIENT_ID, sizeof(Cluster::Constants::iCLIENT_ID));        
        if (rdma->default_connect(socket, buf.get(), 16 * 1024) != 0) {
            return false;
        }

        server_connections[tid][node_id] = std::move(rdma);
        shutdown(socket, 0);
        return true;
    }

    auto Client::write_to(int tid, int node_id, const byte_ptr_t &remote_ptr, const byte_ptr_t &msg, size_t msg_len) noexcept -> RDMAUtil::StatusPair {
        if (node_id <= 0 || size_t(node_id) >= Cluster::Constants::uMAX_NODE) {
            return {RDMAUtil::Status::InvalidArguments, -1};
        }

        return server_connections[tid][node_id]->post_write(remote_ptr, msg, msg_len);
    }

    auto Client::read_from(int tid, int node_id, const byte_ptr_t &remote_ptr, size_t msg_len) noexcept -> RDMAUtil::StatusPair {
        if (node_id <= 0 || size_t(node_id) >= Cluster::Constants::uMAX_NODE) {
            return {RDMAUtil::Status::InvalidArguments, -1};
        }

        return server_connections[tid][node_id]->post_read(remote_ptr, msg_len);
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

