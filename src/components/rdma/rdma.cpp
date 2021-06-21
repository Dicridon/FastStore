#include "rdma.hpp"

namespace Hill {
    RDMAContext RDMADevice::open_device() {
        return RDMAContext(ibv_open_device(&dev));
    }
}
