#include "rdma/rdma.hpp"
#include "coloring/coloring.hpp"
#include "cmd_parser/cmd_parser.hpp"

#include <infiniband/verbs.h>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace Hill;
const ColorizedString error_msg("Error: ", Colors::Red);
const ColorizedString warning_msg("Warning: ", Colors::Magenta);

void show_connection_info(const connection_certificate &c, bool is_local = true) {
    std::string from = is_local ? "local" : "remote";
    std::cout << ">> reporting info" << "(" << from << "):";
    std::cout << "  [[ address: " << c.addr << "\n";
    std::cout << "  [[ rkey: " << c.rkey << "\n";
    std::cout << "  [[ lid: " << c.lid << "\n";
    std::cout << "  [[ qp num: " << c.qp_num << "\n";
    std::cout << "  [[ gid: ";
    const char *pat = ":%02x";
    for (int i = 0; i < 16; i++) {
        printf(pat + (i == 0), c.gid[i]);
    }
    std::cout << "\n";
}

int socket_connect(bool is_server, int socket_port) {
    struct sockaddr_in seraddr;
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cout << ">> " << error_msg << "can not open socket\n";
        exit(-1);
    }

    memset(&seraddr, 0, sizeof(struct sockaddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(socket_port);

    if (is_server) {
        seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            
        if (bind(sockfd, (struct sockaddr *)&seraddr, sizeof(struct sockaddr)) == -1) {
            std::cout << ">> " << error_msg << "can not bind socket\n";
            exit(-1);
        }

        if (listen(sockfd, 1) == -1) {
            std::cout << ">> " << error_msg << "can not listen socket\n";
            exit(-1);
        }

        auto ret = accept(sockfd, NULL, 0);
        if (ret == -1) {
            std::cout << ">> " << error_msg << "accepting connection failed\n";
            exit(-1);
        }
        return ret;
    } else {
        inet_pton(AF_INET, "127.0.0.1", &seraddr.sin_addr);
        
        if (connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) == -1) {
            std::cout << ">> " << error_msg << "connecting to server failed\n";
            exit(-1);
        }
        return sockfd;
    }
}

bool exchange_certificate(int sockfd, const connection_certificate *cm_out, connection_certificate *cm_in) {
    const int normal = sizeof(connection_certificate);
    if (write(sockfd, cm_out, normal) != normal) {
        std::cout << ">> " << error_msg << "failed to write certificate\n";
        return false;
    }

    if (read(sockfd, cm_in, normal) != normal) {
        std::cout << ">> " << error_msg << "failed to read certificate\n";
        return false;
    }
    return true;
}

// buf is not managed
struct RDMAGround {
    RDMAContext ctx;
    RDMAProtectDomain pd;
    RDMACompletionQueue cq;
    RDMAQueuePair qp;
    RDMAMemoryRegion mr;
    void *buf;

    RDMAGround(RDMAContext &&_ctx, RDMAProtectDomain &&_pd,
               RDMAQueuePair &&_qp, RDMACompletionQueue &&_cq,
               RDMAMemoryRegion &&_mr, void *&buf_)
        : ctx(std::move(_ctx)),
          pd(std::move(_pd)),
          cq(std::move(_cq)),
          qp(std::move(_qp)),
          mr(std::move(_mr)),
          buf(buf_)
    {
        buf_ = nullptr;
    }

    RDMAGround() = delete;
    RDMAGround(const RDMAGround &) = delete;
    RDMAGround &operator=(const RDMAGround &) = delete;
    RDMAGround(RDMAGround &&other)
        : ctx(std::move(other.ctx)),
          pd(std::move(other.pd)),
          cq(std::move(other.cq)),
          qp(std::move(other.qp)),
          mr(std::move(other.mr)),
          buf(other.buf)
    {
        other.buf = nullptr;
    }
    RDMAGround &operator=(RDMAGround &&other) {
        ctx = std::move(other.ctx);
        pd = std::move(other.pd);
        cq = std::move(other.cq);
        qp = std::move(other.qp);
        mr = std::move(other.mr);
        buf = other.buf;
        other.buf = nullptr;
        return *this;
    }

};

RDMAGround get_ground(RDMADevice &device) {
    // open the device to get associated context for communication
    auto ctx =  device.open_device();
    if (!ctx.is_valid()) {
        std::cout << ">> " << error_msg << "can not open device " << "\n";
        exit(-1);
    }
    std::cout << ">> Context created\n";

    // associate a protect domain with the context
    auto pd = ctx.create_pd();
    if (!pd.is_valid()) {
        std::cout << ">> " << error_msg << "can not create protect domain\n";
        exit(-1);
    }
    std::cout << ">> Protect domain created\n";

    // obtain a completion queue from the context
    auto cq = ctx.create_cq(1);
    if (!cq.is_valid()) {
        std::cout << ">> " << error_msg << "can not create completion queue\n";
        exit(-1);
    }
    std::cout << ">> Completion queue created\n";

    // register memory retion in the protect domain
    void *buf = malloc(1024);
    auto mr = pd.reg_mr(buf, 1024, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (!mr.is_valid()) {
        std::cout << ">> " << error_msg << "can not register memory\n";
    }
    std::cout << ">> Memory region registered\n";

    // initialize queue pair for the protect domain for real communication
    // the completion queue previously obtained is associated with this queue pair
    auto qp = pd.create_qp([&](struct ibv_qp_init_attr &attr) {
        attr.qp_type = IBV_QPT_RC;
        attr.sq_sig_all = 1;
        attr.send_cq = cq.get_cq_raw();
        attr.recv_cq = cq.get_cq_raw();
        attr.cap.max_send_wr = 1;
        attr.cap.max_recv_wr = 1;
        attr.cap.max_send_sge = 1;
        attr.cap.max_recv_sge = 1;
    });
    if (!qp.is_valid()) {
        std::cout << ">>" << error_msg << "can not create queue pair\n";
        exit(-1);
    }
    std::cout << ">> Queue pair created\n";
    return RDMAGround(std::move(ctx), std::move(pd), std::move(qp), std::move(cq), std::move(mr), buf);
}

void modify_qp(RDMAGround &rdma, int ib_port, const connection_certificate &remote,
               const connection_certificate &local, int dgid_idx = -1) {
    
    if (rdma.qp.modify_qp_init(ib_port) != 0) {
        std::cout << ">> " << error_msg << "failed to modify qp to init\n";
        exit(-1);
    }

    auto ret = rdma.qp.modify_qp_rtr([&](struct ibv_qp_attr &attr, int &mk) {
        attr.qp_state = IBV_QPS_RTR;
        attr.path_mtu = IBV_MTU_256;
        attr.dest_qp_num = remote.qp_num;
        attr.rq_psn = 0;
        attr.max_dest_rd_atomic = 1;
        attr.min_rnr_timer = 0x12;

        attr.ah_attr.is_global = 0;
        attr.ah_attr.dlid = remote.lid;
        attr.ah_attr.sl = 0;
        attr.ah_attr.src_path_bits = 0;
        attr.ah_attr.port_num = ib_port;
        
        if (dgid_idx >= 0) {
            attr.ah_attr.is_global = 1;
            attr.ah_attr.port_num = 1;
            memcpy(&attr.ah_attr.grh.dgid, remote.gid, 16);
            attr.ah_attr.grh.flow_label = 0;
            attr.ah_attr.grh.hop_limit = 1;
            attr.ah_attr.grh.sgid_index = dgid_idx;
            attr.ah_attr.grh.traffic_class = 0;
        }

        mk = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    });
    
    if (ret != 0) {
        auto code = std::to_string(ret);
        std::cout << ">> " << error_msg << "failed to modify qp to rtr, error code: "
                  << ColorizedString(code, Colors::Yellow) << "\n";
        exit(-1);
    }

    ret = rdma.qp.modify_qp_rts([&](struct ibv_qp_attr &attr, int &mask) {
        attr.qp_state = IBV_QPS_RTS;
        attr.timeout = 0x12; // 18
        attr.retry_cnt = 6;
        attr.rnr_retry = 0;
        attr.sq_psn = 0;
        attr.max_rd_atomic = 1;

        mask = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    });

    if (ret != 0) {
        auto code = std::to_string(ret);
        std::cout << ">> " << error_msg << "failed to modify qp to rtr, error code: "
                  << ColorizedString(code, Colors::Yellow);
        exit(-1);
    }    
}

RDMADevice find_device(const std::string &dev_name) {
    auto dev_list = RDMADeviceList();
    if (!dev_list.is_valid()) {
        std::cout << ">> " << error_msg << "no device list created on this machine\n";
        exit(-1);
    }
    std::cout << ">> Device list obtained\n";

    // find target device by its name
    // device name can be obtained using command 'ibstat'
    auto device = dev_list.get_device(dev_name);
    if (!device.is_valid()) {
        std::cout << ">> " << error_msg << "no device " << dev_name << " found\n";
        exit(-1);
    }
    std::cout << ">> Device obtained\n";
    return device;
}

int main(int argc, char *argv[]) {
    Cmdparser parser;
    if (argc < 2) {
        parser.Help();
        return -1;
    }
    parser.Parse(argv, argv + argc);

    std::string dev_name = parser.GetDevice();
    int ib_port = std::stoi(parser.GetIbPort());
    int socket_port = std::stoi(parser.GetSocketPort());
    bool is_server = parser.GetIsServer() == "true";

    const int gid_idx = 2;
    
    // first get device list and find the target device
    auto device = find_device(dev_name);
    // see comments in following function
    auto rdma = get_ground(device);
    
    // exchange certificate with peer
    auto port_attr = rdma.ctx.query_port(ib_port);
    if (port_attr.second != 0) {
        std::cout << ">> " << error_msg << "failed to query port attritbue\n";
        exit(-1);
    }

    auto sock = socket_connect(is_server, socket_port);
    connection_certificate local, remote;
    local.addr = htonll((uint64_t)rdma.buf);
    local.rkey = htonl(rdma.mr.get_rkey());
    local.qp_num = htonl(rdma.qp.get_qp_num());
    local.lid = htons(port_attr.first->lid);
    if (!exchange_certificate(sock, &local, &remote)) {
        exit(-1);
    }
    remote.addr = ntohll(remote.addr);
    remote.rkey = ntohl(remote.rkey);
    remote.qp_num = ntohl(remote.qp_num);
    remote.lid = ntohs(remote.lid);

    local.addr = ntohll((uint64_t)rdma.buf);
    local.rkey = ntohl(rdma.mr.get_rkey());
    local.qp_num = ntohl(rdma.qp.get_qp_num());
    local.lid = ntohs(port_attr.first->lid);


    show_connection_info(local);
    show_connection_info(remote, false);

    modify_qp(rdma, ib_port, remote, local, gid_idx);

    return 0;
}
