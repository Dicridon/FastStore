#include "cmd_parser/cmd_parser.hpp"
#include "rpc_wrapper/rpc_wrapper.hpp"

using namespace RPCWrapper;

struct ServerContext {
    erpc::Rpc<erpc::CTransport> *rpc;
};

struct ClientContext {
    erpc::Rpc<erpc::CTransport> *rpc;
    std::atomic_bool done;
    erpc::MsgBuffer req_buf;
    erpc::MsgBuffer resp_buf;    
};

auto server_handler(erpc::ReqHandle *req_handle, void *context) -> void {
    auto server_ctx = reinterpret_cast<ServerContext *>(context);
    auto requests = req_handle->get_req_msgbuf();

    auto in_data = *reinterpret_cast<uint64_t *>(requests->buf);
    std::cout << "got " << in_data << " from client\n";
    
    auto &resp = req_handle->dyn_resp_msgbuf;
    resp = server_ctx->rpc->alloc_msg_buffer_or_die(sizeof(uint64_t));
    *reinterpret_cast<uint64_t *>(resp.buf) = in_data + 1;
    server_ctx->rpc->enqueue_response(req_handle, &resp);
};


auto server() -> void {
    auto nexus = new erpc::Nexus("10.0.0.48:31850", 0, 0);
    nexus->register_req_func(0, server_handler);    
    ServerContext ctx;
    ctx.rpc = new erpc::Rpc<erpc::CTransport>(nexus, reinterpret_cast<void *>(&ctx),
                                              0, RPCWrapper::ghost_sm_handler);

    std::cout << "server waiting for income requests\n";
    ctx.rpc->run_event_loop(1000000);
}

auto res_cont(void *context, [[maybe_unused]]void *tag) {
    auto ctx = reinterpret_cast<ClientContext *>(context);
    auto &resp = ctx->resp_buf;
    std::cout << "got " << *reinterpret_cast<uint64_t *>(resp.buf) << " from server\n";
    ctx->done = true;
};


auto client() -> void {
    auto nexus = new erpc::Nexus("10.0.0.49:31850", 0, 0);
    ClientContext ctx;
    ctx.done = false;
    ctx.rpc = new erpc::Rpc<erpc::CTransport>(nexus, reinterpret_cast<void *>(&ctx),
                                              0, RPCWrapper::ghost_sm_handler);
    ctx.req_buf = ctx.rpc->alloc_msg_buffer_or_die(sizeof(uint64_t));
    ctx.resp_buf = ctx.rpc->alloc_msg_buffer_or_die(sizeof(uint64_t));
    auto session = ctx.rpc->create_session("10.0.0.48:31850", 0);
    while (!ctx.rpc->is_connected(session)) {
        std::cout << "waiting connection\n";
        ctx.rpc->run_event_loop_once();
    }

    uint64_t counter = 0;
    while(true) {
        std::cout << "sending " << counter << " to server\n";
        *reinterpret_cast<uint64_t *>(ctx.req_buf.buf) = counter++;
        ctx.done = false;
        std::cout << "req_buf is " << ctx.req_buf.buf << "\n";
        std::cout << "req_buf size is " << ctx.req_buf.get_data_size() << "\n";
        std::cout << "resp_buf is " << ctx.resp_buf.buf << "\n";
        std::cout << "resp_buf size is " << ctx.resp_buf.get_data_size() << "\n";
        
        ctx.rpc->enqueue_request(session, 0, &ctx.req_buf, &ctx.resp_buf, res_cont, nullptr);
        // sleepp(1)
        // ctx.rpc->run_event_loop_once();
        while (!ctx.done.load()) {
            ctx.rpc->run_event_loop_once();
        }
    }
}

auto main(int argc, char *argv[]) -> int {
    CmdParser::Parser parser;
    parser.add_switch("--server", "-s", true);
    parser.parse(argc, argv);

    auto is_server = parser.get_as<bool>("--server");
    if (is_server.value()) {
        server();
    } else {
        client();
    }
}
