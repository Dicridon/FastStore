#ifndef __HILL__RPC_WRAPPER__RPC_WRAPPER__
#define __HILL__RPC_WRAPPER__RPC_WRAPPER__

 #include "rpc.h"

#include <iostream>
#include <memory>
namespace RPCWrapper {
    static auto ghost_sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) -> void {};
    
    class CTransportRPC {
    public:
        CTransportRPC() = default;
        CTransportRPC(const CTransportRPC &) = delete;        
        CTransportRPC(CTransportRPC &&) = delete;
        auto operator=(const CTransportRPC &) -> CTransportRPC & = delete;
        auto operator=(CTransportRPC &&) -> CTransportRPC & = delete;
        ~CTransportRPC() {
            delete _rpc;
            // don't touch the nexus
        }
        
        static auto make_ctransport_rpc(erpc::Nexus *nexus, void *context, int tid,
                                        erpc::sm_handler_t sm_handler) -> std::unique_ptr<CTransportRPC>
        {
            auto ret = std::make_unique<CTransportRPC>();

            ret->_nexus = nexus;
            ret->_rpc = new erpc::Rpc<erpc::CTransport>(ret->_nexus, context, tid, sm_handler);
            return ret;
        }

        auto rpc() noexcept -> erpc::Rpc<erpc::CTransport> * {
            return _rpc;
        }

        auto nexus() noexcept -> erpc::Nexus * {
            return _nexus;
        }

        auto run(size_t time) -> void {
            _rpc->run_event_loop(time);
        }

        auto d() -> void;

    private:
        erpc::Nexus *_nexus;
        erpc::Rpc<erpc::CTransport> *_rpc;
    };
}

#endif
