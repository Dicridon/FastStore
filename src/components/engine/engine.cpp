#include "engine.hpp"
namespace Hill {
    auto Engine::check_rdma_request() noexcept -> int {
        auto socket = Misc::accept_blocking(sock);

        if (socket == -1) {
            return -1;
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

        return 0;
    }

    auto Engine::launch() noexcept -> void {
        
    }

    auto Engine::stop() noexcept -> void {
        
    }

    auto Engine::dump() const noexcept -> void {
        node->dump();
        std::cout << "---->> RDMA device: " << rdma_dev_name << "\n";
        std::cout << "---->> ib port: " << ib_port << "\n";
        std::cout << "---->> gid index: " << gid_idx << "\n";
    }

    auto Engine::parse_ib(const std::string &config) noexcept -> bool {
        std::ifstream c(config);
        if (!c.is_open()) {
            return false;
        }
        
        std::stringstream buf;
        buf << c.rdbuf();
        auto content = buf.str();

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

