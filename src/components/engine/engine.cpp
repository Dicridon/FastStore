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

        struct ibv_qp_init_attr at;
        memset(&at, 0, sizeof(struct ibv_qp_init_attr));
        int mr_access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
        at.qp_type = IBV_QPT_RC;
        at.sq_sig_all = 1;
        at.cap.max_send_wr = 128;
        at.cap.max_recv_wr = 128;
        at.cap.max_send_sge = 128;
        at.cap.max_recv_sge = 128;

        if (auto status = rdma->open(base, node->available_pm, 128, mr_access, at); status != RDMAStatus::Ok) {
            std::cerr << "Failed to open RDMA, error code: " << decode_rdma_status(status) << "\n";
            return -1;
        }

        if (rdma->exchange_certificate(socket) != RDMAStatus::Ok) {
            std::cerr << "Failed to exchange RDMA, error code: " << decode_rdma_status(status) << "\n";
            return -1;
        }

        auto init_attr = RDMA::get_default_qp_init_attr();
        if (auto [status, err] = rdma->modify_qp(*init_attr, RDMA::get_default_qp_init_attr_mask()); status != RDMAStatus::Ok) {
            std::cerr << "Modify QP to Init failed, error code: " << err << "\n";
            return -1;
        }

        auto rtr_attr = RDMA::get_default_qp_rtr_attr(rdma->get_remote(), rdma->get_ib_port(), rdma->get_gid_idx());
        if (auto [status, err] = rdma->modify_qp(*rtr_attr, RDMA::get_default_qp_rtr_attr_mask()); status != RDMAStatus::Ok) {
            std::cerr << "Modify QP to Rtr failed, error code: " << err << "\n";
            return -1;
        }

        auto rts_attr = RDMA::get_default_qp_rts_attr();
        if (auto [status, err] = rdma->modify_qp(*rts_attr, RDMA::get_default_qp_rts_attr_mask()); status != RDMAStatus::Ok) {
            std::cerr << "Modify QP to Rts failed, error code: " << err << "\n";
            return -1;
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
}

