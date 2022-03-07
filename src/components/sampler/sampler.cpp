#include "sampler.hpp"
namespace Hill {
    namespace Sampling {
        const SampleType HandleSampler::PARSE = 0;
        const SampleType HandleSampler::CAP_CHECK = 1;
        const SampleType HandleSampler::CAP_RECHECK = 2;
        const SampleType HandleSampler::INDEXING = 3;
        const SampleType HandleSampler::RESP_MSG = 4;
        const SampleType HandleSampler::RESP = 5;
        const SampleType HandleSampler::MERGE = 6;

        const SampleType ClientSampler::RDMA = 0;
        const SampleType ClientSampler::CACHE = 1;
        const SampleType ClientSampler::CACHE_RDMA = 2;
        const SampleType ClientSampler::CHECK_RPC = 3;
        const SampleType ClientSampler::PRE_REQ = 4;
        const SampleType ClientSampler::RPC = 5;
        const SampleType ClientSampler::CONTI = 6;
        
        auto HandleSampler::prepare() -> void {
            for (int i = 0; i < 7; i++) {
                insert_sampler.new_sampling_type(batch_size).value();                
                search_sampler.new_sampling_type(batch_size);
                update_sampler.new_sampling_type(batch_size);
                remove_sampler.new_sampling_type(batch_size);
                scan_sampler.new_sampling_type(batch_size);
            }
        }

        auto HandleSampler::report_insert() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(insert_sampler.get_sample(RESP).content())
                      << "]], "
                      << "[[" << CAP_CHECK << ": "
                      << Misc::avg(insert_sampler.get_sample((CAP_CHECK)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(insert_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << CAP_RECHECK << ": "
                      << Misc::avg(insert_sampler.get_sample((CAP_RECHECK)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(insert_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(insert_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_search() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(search_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(search_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(search_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(search_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_update() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(update_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << CAP_CHECK << ": "
                      << Misc::avg(update_sampler.get_sample((CAP_CHECK)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(update_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << CAP_RECHECK << ": "
                      << Misc::avg(update_sampler.get_sample((CAP_RECHECK)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(update_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(update_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_remove() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(remove_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(remove_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(remove_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(remove_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_scan() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(scan_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(scan_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << MERGE << ": "
                      << Misc::avg(scan_sampler.get_sample((MERGE)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(scan_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(scan_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto ClientSampler::prepare() -> void {
            for (int i = 0; i < 7; i++) {
                insert_sampler.new_sampling_type(batch_size).value();                
                search_sampler.new_sampling_type(batch_size);
                update_sampler.new_sampling_type(batch_size);
                remove_sampler.new_sampling_type(batch_size);
                scan_sampler.new_sampling_type(batch_size);
                common_sampler.new_sampling_type(batch_size);
            }
        }

        auto ClientSampler::report_insert() const noexcept -> void {
            std::cout << "[[" << CACHE << ": "
                      << Misc::avg(insert_sampler.get_sample((CACHE)).content())
                      << "]], "
                      << "[[" << CHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_search() const noexcept -> void {
            std::cout << "[[" << RDMA << ": "
                      << Misc::avg(search_sampler.get_sample((RDMA)).content())
                      << "]], "
                      << "[[" << CACHE << ": "
                      << Misc::avg(search_sampler.get_sample((CACHE)).content())
                      << "]], "
                      << "[[" << CACHE_RDMA << ": "
                      << Misc::avg(search_sampler.get_sample((CACHE_RDMA)).content())
                      << "]], "
                      << "[[" << CHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_update() const noexcept -> void {
            std::cout << "[[" << CHECK_RPC << ": "
                      << Misc::avg(update_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_remove() const noexcept -> void {
            std::cout << "[[" << CHECK_RPC << ": "
                      << Misc::avg(remove_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_scan() const noexcept -> void {
            std::cout << "[[" << RDMA << ": "
                      << Misc::avg(scan_sampler.get_sample((RDMA)).content())
                      << "]], "
                      << "[[" << CHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }
    }
}
