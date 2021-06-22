/*
 * rdma.hpp/cpp:
 * A bunch of wrapper classes over libibverbs C bindings. These classes include 
 * RDMADeviceList, RDMADevice, RDMAProtectDomain, RDMACompleteQueue, RDMAQueuePair, etc. 
 * 
 * The main purpose of these classes is for easy use of the ibverbs. The original C bindings 
 * contains too many calling rules over many seemingly irrelative functions, while the wrappers
 * explicitly exhibit the calling order of all the necessary functions:
 *
 * RDMAMemoryRegion <- RDMAProtectDomain <- RDMAContext <- RDMADevice <- RDMADeviceList
 *                             |                |
 * RDMAQueuePair <-------------|                |
 *       |                                      |
 *       | used by                              |
 * RDMACompletionQueue <------------------------|                          
 *
 *
 * Note: 1. event based utility is not included since I'm not familiar with it
 *       2. RAII classes rely on vbers deallocation functions, but these functions do not
 *          guarantee the success of deallocation, thus resource leaks are still possible
 *       3. struct ibv_wc is not wrapped as it is close to raw data
 *       4. All attr structures are not wrapped
 * 
 */

#ifndef __HILL__RDMA__RDMA__
#define __HILL__RDMA__RDMA__
#include <memory>
#include <optional>
#include <functional>
#include <infiniband/verbs.h>
#include <byteswap.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif


namespace Hill {
    struct connection_certificate {
        uint64_t addr;   // registered memory address
        uint32_t rkey;   // remote key
        uint32_t qp_num; // local queue pair number
        uint16_t lid;    // LID of the ib port
        uint8_t gid[16]; // currently not used
    } __attribute__((packed));
    
    // A RAII class over struct ibv_cq
    class RDMACompletionQueue {
    public:
        RDMACompletionQueue() = delete;
        RDMACompletionQueue(const RDMACompletionQueue &) = delete;
        RDMACompletionQueue &operator=(const RDMACompletionQueue &) = delete;
        RDMACompletionQueue(RDMACompletionQueue &&other) {
            cq = other.cq;
            other.cq = nullptr;
        };
        RDMACompletionQueue &operator=(RDMACompletionQueue &&other) {
            cq = other.cq;
            other.cq = nullptr;
            return *this;
        }

        RDMACompletionQueue(struct ibv_cq *cq_) : cq(cq_) {};
        ~RDMACompletionQueue() {
            if (cq)
                ibv_destroy_cq(cq);
        }

        // Public APIs
        bool is_valid() const noexcept {
            return cq != nullptr;
        }
        
        struct ibv_cq *get_cq_raw() noexcept {
            return cq;
        }

        int cqe() const noexcept {
            return cq->cqe;
        }

        int resize(int cqe) noexcept {
            return ibv_resize_cq(cq, cqe);
        }
        
    private:
        struct ibv_cq *cq;
    };

    
    // A wrapper class over struct ibv_mr for unified behavior across all classes
    // This class is NOT a RAII class
    class RDMAMemoryRegion {
    public:
        RDMAMemoryRegion() = default;
        RDMAMemoryRegion(const RDMAMemoryRegion &) = default;
        RDMAMemoryRegion &operator=(const RDMAMemoryRegion &) = default;
        RDMAMemoryRegion(RDMAMemoryRegion &&) = default;
        RDMAMemoryRegion &operator=(RDMAMemoryRegion &&) = default;
        
        RDMAMemoryRegion(struct ibv_mr *mr_) : mr(mr_) {};
        ~RDMAMemoryRegion() = default;


        // Public APIs
        bool is_valid() const noexcept {
            return mr != nullptr;
        }
        
        struct ibv_mr *get_mr_raw() noexcept {
            return mr;
        }
        
        struct ibv_context *get_context_raw() noexcept {
            return mr->context;
        }

        struct ibv_pd *get_pd_raw() noexcept {
            return mr->pd;
        }

        void *get_addr_raw() noexcept {
            return mr->addr;
        }

        size_t get_length() noexcept {
            return mr->length;
        }

        uint32_t get_handle() noexcept {
            return mr->handle;
        }

        uint32_t get_lkey() noexcept {
            return mr->lkey;
        }
        
        uint32_t get_rkey() noexcept {
            return mr->rkey;
        }
        
    private:
        struct ibv_mr *mr;
    };

    struct RDMAQueuePairAttrs {
        RDMAQueuePairAttrs() = default;
        RDMAQueuePairAttrs(const RDMAQueuePairAttrs &) = default;
        RDMAQueuePairAttrs(RDMAQueuePairAttrs &&) = default;
        ~RDMAQueuePairAttrs() = default;

        RDMAQueuePairAttrs(const struct ibv_qp_attr &a) {
            memcpy(&attr, &a, sizeof(struct ibv_qp_attr));
        }

        RDMAQueuePairAttrs(const struct ibv_qp_init_attr &ia) {
            memcpy(&attr, &ia, sizeof(struct ibv_qp_init_attr));
        }

        RDMAQueuePairAttrs(const struct ibv_qp_attr &a, const struct ibv_qp_init_attr &ia) {
            memcpy(&attr, &a, sizeof(struct ibv_qp_attr));
            memcpy(&init_attr, &ia, sizeof(struct ibv_qp_attr));
        }

        struct ibv_qp_attr attr;
        struct ibv_qp_init_attr init_attr;
    };

    // A RAII class managing queue pair (not shared queue pair)
    class RDMAQueuePair {
    public:
        RDMAQueuePair() = delete;
        RDMAQueuePair(const RDMAQueuePair &) = delete;
        RDMAQueuePair &operator=(const RDMAQueuePair &) = delete;
        RDMAQueuePair(RDMAQueuePair &&other) {
            qp = other.qp;
            other.qp = nullptr;
        };
        RDMAQueuePair &operator =(RDMAQueuePair &&other) {
            qp = other.qp;
            other.qp = nullptr;
            return *this;
        };


        RDMAQueuePair(struct ibv_qp *in) : qp(in) {}
        ~RDMAQueuePair() {
            if (qp)
                ibv_destroy_qp(qp);
        }

        // Public APIs
        bool is_valid() const noexcept {
            return qp != nullptr;
        }

        struct ibv_qp *get_qp_raw() noexcept {
            return qp;
        }

        uint32_t get_qp_num() const noexcept {
            return qp->qp_num;
        }

        enum ibv_qp_state get_qp_state() const noexcept {
            return qp->state;
        }

        enum ibv_qp_type get_qp_type() const noexcept {
            return qp->qp_type;
        }

        // See man page on 'ibv_modify_qp' for detailed attribute fields
        int modify_qp(struct ibv_qp_attr *attr, int attr_mask) noexcept {
            return ibv_modify_qp(qp, attr, attr_mask);
        }

        int modify_qp_init(int ib_port) noexcept {
            struct ibv_qp_attr attr;

            memset(&attr, 0, sizeof(attr));
            attr.qp_state = IBV_QPS_INIT;
            attr.port_num = ib_port;
            attr.pkey_index = 0;
            attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                IBV_ACCESS_REMOTE_WRITE;
            
            int mask = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;

            return ibv_modify_qp(qp, &attr, mask);
        }

        /*
         * For subnet RDMA communication the
         * Only ibv_qp_attr.ah_attr.is_global is set to 0
         * To enable cross subnet communication, use modify_qp above.
         * @remote_qpn: remote queue pair number
         * @dlid: remote local ID
         * @configer: a lambda or function altering other attributes of this queue pair
         */
        using qp_configer_t = std::function<void(struct ibv_qp_attr &at, int &mk)>;
        static constexpr auto default_rtr_configer = [](struct ibv_qp_attr &at, int &mk) {
            at.path_mtu = IBV_MTU_256;
            at.qp_state = IBV_QPS_RTR;
            at.rq_psn = 0;
            at.max_dest_rd_atomic = 1;
            at.min_rnr_timer = 0x12;
            at.ah_attr.is_global = 0;
            at.ah_attr.sl = 0;
            at.ah_attr.src_path_bits = 0;

            mk = IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
        };

        // ibv_qp_attr.ah_attr.port_num is set to be the port_num of this qp
        int modify_qp_rtr_subnet(uint32_t remote_qpn, uint16_t dlid, uint8_t ib_port, qp_configer_t configer = default_rtr_configer) {
            struct ibv_qp_attr attr;
            int mask = 0;
            memset(&attr, 0, sizeof(struct ibv_qp_attr));
            
            configer(attr, mask);
            attr.qp_state = IBV_QPS_RTR;
            attr.dest_qp_num = remote_qpn;
            attr.ah_attr.dlid = dlid;
            attr.ah_attr.port_num = ib_port;
            mask |= IBV_QP_STATE;
            return ibv_modify_qp(qp, &attr, mask);
        }

        int modify_qp_rts_subnet(qp_configer_t configer) {
            struct ibv_qp_attr attr;
            int mask = 0;
            memset(&attr, 0, sizeof(struct ibv_qp_attr));

            configer(attr, mask);
            mask |= IBV_QP_STATE;
            return ibv_modify_qp(qp, &attr, mask);
        }

        std::pair<RDMAQueuePairAttrs, int> query_qp(int attr_mask) {
            RDMAQueuePairAttrs attrs;
            auto ret = ibv_query_qp(qp, &attrs.attr, attr_mask, &attrs.init_attr);
            return std::make_pair(std::move(attrs), ret);
        }

        int post_send(struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr) {
            return ibv_post_send(qp, wr, bad_wr);
        }

        int post_recv(struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr) {
            return ibv_post_recv(qp, wr, bad_wr);
        }

    private:
        struct ibv_qp *qp;
    };


    // A RAII class managing protect domain
    // never explicitly created by user
    class RDMAProtectDomain {
    public:
        RDMAProtectDomain() = delete;
        RDMAProtectDomain(const RDMAProtectDomain &) = delete;
        RDMAProtectDomain &operator=(const RDMAProtectDomain &) = delete;
        RDMAProtectDomain(RDMAProtectDomain &&other) {
            pd = other.pd;
            other.pd = nullptr;
        };
        RDMAProtectDomain &operator=(RDMAProtectDomain &&other) {
            pd = other.pd;
            other.pd = nullptr;
            return *this;
        };


        RDMAProtectDomain(struct ibv_pd *in) noexcept {
            pd = in;
        }
        
        ~RDMAProtectDomain() {
            if (pd)
                ibv_dealloc_pd(pd);
        }

        // Public APIs
        bool is_valid() const noexcept {
            return pd != nullptr;
        }
        
        struct ibv_pd *get_pd_raw() noexcept {
            return pd;
        }

        struct ibv_mr *reg_mr_raw(void *addr, size_t length, int access) noexcept {
            return ibv_reg_mr(pd, addr, length, access);
        }
        RDMAMemoryRegion reg_mr(void *addr, size_t length, int access) noexcept {
            return RDMAMemoryRegion(reg_mr_raw(addr, length, access));
        }

        int rereg_mr_raw(struct ibv_mr *mr, int flags, void *addr, size_t length, int access) noexcept {
            return ibv_rereg_mr(mr, flags, pd, addr, length, access);
        }
        int rereg_mr(RDMAMemoryRegion &mr, int flags, void *addr, size_t length, int access) noexcept {
            return ibv_rereg_mr(mr.get_mr_raw(), flags, pd, addr, length, access);
        }

        int dereg_mr_raw(struct ibv_mr *mr) noexcept {
            return ibv_dereg_mr(mr);
        }
        int dereg_mr(RDMAMemoryRegion &mr) noexcept {
            return ibv_dereg_mr(mr.get_mr_raw());
        }

        struct ibv_qp *create_qp_raw(struct ibv_qp_init_attr *init_attr) noexcept {
            return ibv_create_qp(pd, init_attr);
        }

        RDMAQueuePair create_qp(struct ibv_qp_init_attr *init_attr) noexcept {
            return RDMAQueuePair(ibv_create_qp(pd, init_attr));
        }

        RDMAQueuePair create_qp(std::function<void(struct ibv_qp_init_attr &)> init) {
            struct ibv_qp_init_attr attr;
            memset(&attr, 0, sizeof(struct ibv_qp_init_attr));
            init(attr);
            return create_qp(&attr);
        }
    private:
        struct ibv_pd *pd;
    };

    /*
     * RDMAContext 
     * A RAII class manageing an opened RDMADevice. 
     * This class is never explicitly instanciated, instead, a call to RDMADevice::open_device will 
     * create a RDMAContex object
     */
    class RDMAContext {
    public:
        RDMAContext() = delete;
        RDMAContext(const RDMAContext &) = delete;
        RDMAContext &operator=(const RDMAContext &) = delete;
        RDMAContext(RDMAContext &&other) {
            ctx = other.ctx;
            other.ctx = nullptr;
        };
        RDMAContext &operator=(RDMAContext &&other) {
            ctx = other.ctx;
            other.ctx = nullptr;
            return *this;
        };

        
        RDMAContext(struct ibv_context *in) {
            ctx = in;
        }

        ~RDMAContext() {
            if (ctx)
                ibv_close_device(ctx);
        };

        // Public APIs
        bool is_valid() const noexcept {
            return ctx != nullptr;
        }
        
        struct ibv_context *get_raw_context() noexcept {
            return ctx;
        }

        // see man page on 'ibv_query_device' for detailed attribute fields
        std::pair<std::unique_ptr<struct ibv_device_attr>, int> query_device() noexcept {
            auto attr = std::make_unique<struct ibv_device_attr>();
            auto ret = ibv_query_device(ctx, attr.get());
            return std::make_pair(std::move(attr), ret);
        }

        // see man page on 'ibv_query_port' for detailed attribute fields
        std::pair<std::unique_ptr<struct ibv_port_attr>, int> query_port(uint8_t port_num) noexcept {
            auto attr = std::make_unique<struct ibv_port_attr>();
            auto ret = ibv_query_port(ctx, port_num, attr.get());
            return std::make_pair(std::move(attr), ret);
        }

        // int ibv_query_gid(struct ibv_context *, uint8_t, int, union ibv_gid *) not wrapped
        // int ibv_query_pkey(struct ibv_context *, uint8_t, int, __be16 *pkey) not wrapped
        
        struct ibv_pd *alloc_pd_raw() noexcept {
            return ibv_alloc_pd(ctx);
        }

        int dealloc_pd_raw(struct ibv_pd *pd) noexcept {
            return ibv_dealloc_pd(pd);
        }

        RDMAProtectDomain create_pd() noexcept {
            return RDMAProtectDomain(alloc_pd_raw());
        }

        struct ibv_cq *create_cq_raw(int cqe) noexcept {
            return ibv_create_cq(ctx, cqe, nullptr, nullptr, 0);
        }

        int destroy_cq(struct ibv_cq *cq) noexcept {
            return ibv_destroy_cq(cq);
        }

        RDMACompletionQueue create_cq(int cqe) noexcept {
            return RDMACompletionQueue(ibv_create_cq(ctx, cqe, nullptr, nullptr, 0));
        }
        
    private:
        struct ibv_context *ctx;
    };


    class RDMADevice {
    public:
        RDMADevice() = delete;
        ~RDMADevice() = default;

        RDMADevice(struct ibv_device *in) {
            dev = in;
            // memcpy(&dev, in, sizeof(struct ibv_device));
        }

        RDMADevice(const RDMADevice &) = default;
        RDMADevice(RDMADevice &&) = default;


        // Public APIs
        bool is_valid() const noexcept {
            return dev != nullptr;
        }

        // The prefered way to open a ddvice,
        // the RDMAContext will close the associated device upon destruction
        RDMAContext open_device() {
            return RDMAContext(open_device_raw());
        }

        struct ibv_device *get_device_raw() noexcept {
            return dev;
        }

        std::string get_device_name() noexcept {
            return std::string(ibv_get_device_name(dev));
        }

        __be64 get_device_guid() noexcept {
            return ibv_get_device_guid(dev);
        }

        struct ibv_context *open_device_raw() noexcept {
            return ibv_open_device(dev);
        }

        int close_device_raw(struct ibv_context *ctx) {
            return ibv_close_device(ctx);
        }
        
    private:
        struct ibv_device *dev;
    };

    
    class RDMADeviceList {
    public:
        RDMADeviceList() {
            dev_list = ibv_get_device_list(&dev_num);
        }

        RDMADeviceList(const RDMADeviceList &) = delete;
        RDMADeviceList(RDMADeviceList &&) = delete;

        ~RDMADeviceList() {
            ibv_free_device_list(dev_list);
        }

        // Public APIs
        inline bool is_valid() const noexcept {
            return dev_num != 0;
        }
        
        RDMADevice get_device(const std::string &dev_name) noexcept {
            auto raw = borrow_device_raw(dev_name);
            return RDMADevice(raw);
        }

        // the returned device is OWNED by the RDMADevieList object
        struct ibv_device *borrow_device_raw(const std::string &dev_name) noexcept {
            for (int i = 0; i < dev_num; i++) {
                if (dev_name.compare(ibv_get_device_name(dev_list[i])) == 0) {
                    return dev_list[i];
                }
            }
            return nullptr;
        }

        // the returned device is OWNED by the caller;
        std::unique_ptr<struct ibv_device> get_device_raw(const std::string &dev_name) noexcept {
            auto ret = std::make_unique<struct ibv_device>();
            memset(ret.get(), 0, sizeof(struct ibv_device));
            for (int i = 0; i < dev_num; i++) {
                if (dev_name.compare(ibv_get_device_name(dev_list[i])) == 0) {
                    memcpy(ret.get(), dev_list[i], sizeof(struct ibv_device));
                    return ret;
                }
            }
            return nullptr;
        }
        
    private:
        struct ibv_device **dev_list;
        int dev_num;
    };
}
    
#endif
