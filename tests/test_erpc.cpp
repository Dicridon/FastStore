#include "cmd_parser/cmd_parser.hpp"
#include "rpc_wrapper/rpc_wrapper.hpp"

#include <chrono>

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
    
    auto &resp = req_handle->dyn_resp_msgbuf;
    resp = server_ctx->rpc->alloc_msg_buffer_or_die(sizeof(uint64_t));
    *reinterpret_cast<uint64_t *>(resp.buf) = in_data + 1;
    server_ctx->rpc->enqueue_response(req_handle, &resp);
};


auto server(const std::string &uri) -> void {
    auto nexus = new erpc::Nexus(uri, 0, 0);
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


auto client(const std::string &uri, const std::string &server) -> void {
    auto nexus = new erpc::Nexus(uri, 0, 0);
    ClientContext ctx;
    ctx.done = false;
    ctx.rpc = new erpc::Rpc<erpc::CTransport>(nexus, reinterpret_cast<void *>(&ctx),
                                              0, RPCWrapper::ghost_sm_handler);
    ctx.req_buf = ctx.rpc->alloc_msg_buffer_or_die(32);
    ctx.resp_buf = ctx.rpc->alloc_msg_buffer_or_die(32);
    auto session = ctx.rpc->create_session(server, 0);
    while (!ctx.rpc->is_connected(session)) {
        std::cout << "Waiting connection\n";
        ctx.rpc->run_event_loop_once();
    }

    std::cout << "Connected\n";

    uint64_t msg = 0;
    std::vector<std::string> workload;
    while(true) {
        ctx.done = false;        
        *reinterpret_cast<uint64_t *>(ctx.req_buf.buf) = msg;
        ctx.rpc->enqueue_request(session, 0, &ctx.req_buf, &ctx.resp_buf, res_cont, nullptr);
        while (!ctx.done.load()) {
            ctx.rpc->run_event_loop_once();
        }

        msg = *reinterpret_cast<uint64_t *>(ctx.resp_buf.buf);
        sleep(1);
    }
}

auto main(int argc, char *argv[]) -> int {
    CmdParser::Parser parser;
    parser.add_option("--server", "-s");
    parser.add_option<std::string>("--uri", "-u", "127.0.0.1:31850");
    parser.parse(argc, argv);

    auto uri = parser.get_as<std::string>("--uri").value();
    auto server_uri = parser.get_as<std::string>("--server");

    auto is_server = !server_uri.has_value();
    if (is_server) {
        server(uri);
    } else {
        client(uri, server_uri.value());
    }
}
