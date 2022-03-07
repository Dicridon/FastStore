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

        const std::string HandleSampler::sPARSE = "parsing";
        const std::string HandleSampler::sCAP_CHECK = "cap check";
        const std::string HandleSampler::sCAP_RECHECK = "cap recheck";
        const std::string HandleSampler::sINDEXING = "indexing";
        const std::string HandleSampler::sRESP_MSG = "resp msg";
        const std::string HandleSampler::sRESP = "resp";
        const std::string HandleSampler::sMERGE = "merge";

        const std::string ClientSampler::sRDMA = "rdma";
        const std::string ClientSampler::sCACHE = "cache";
        const std::string ClientSampler::sCACHE_RDMA = "cache rdma";
        const std::string ClientSampler::sCHECK_RPC = "check rpc";
        const std::string ClientSampler::sPRE_REQ = "pre req";
        const std::string ClientSampler::sRPC = "rpc";
        const std::string ClientSampler::sCONTI = "conti";


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
            std::cout << "[[" << sPARSE << ": "
                      << Misc::avg(insert_sampler.get_sample(RESP).content())
                      << "]], "
                      << "[[" << sCAP_CHECK << ": "
                      << Misc::avg(insert_sampler.get_sample((CAP_CHECK)).content())
                      << "]], "
                      << "[[" << sINDEXING << ": "
                      << Misc::avg(insert_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << sCAP_RECHECK << ": "
                      << Misc::avg(insert_sampler.get_sample((CAP_RECHECK)).content())
                      << "]], "
                      << "[[" << sRESP_MSG << ": "
                      << Misc::avg(insert_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << sRESP << ": "
                      << Misc::avg(insert_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_search() const noexcept -> void {
            std::cout << "[[" << sPARSE << ": "
                      << Misc::avg(search_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << sINDEXING << ": "
                      << Misc::avg(search_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << sRESP_MSG << ": "
                      << Misc::avg(search_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << sRESP << ": "
                      << Misc::avg(search_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_update() const noexcept -> void {
            std::cout << "[[" << sPARSE << ": "
                      << Misc::avg(update_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << sCAP_CHECK << ": "
                      << Misc::avg(update_sampler.get_sample((CAP_CHECK)).content())
                      << "]], "
                      << "[[" << sINDEXING << ": "
                      << Misc::avg(update_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << sCAP_RECHECK << ": "
                      << Misc::avg(update_sampler.get_sample((CAP_RECHECK)).content())
                      << "]], "
                      << "[[" << sRESP_MSG << ": "
                      << Misc::avg(update_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << sRESP << ": "
                      << Misc::avg(update_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_remove() const noexcept -> void {
            std::cout << "[[" << sPARSE << ": "
                      << Misc::avg(remove_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << sINDEXING << ": "
                      << Misc::avg(remove_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << sRESP_MSG << ": "
                      << Misc::avg(remove_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << sRESP << ": "
                      << Misc::avg(remove_sampler.get_sample((RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_scan() const noexcept -> void {
            std::cout << "[[" << sPARSE << ": "
                      << Misc::avg(scan_sampler.get_sample((RESP)).content())
                      << "]], "
                      << "[[" << sINDEXING << ": "
                      << Misc::avg(scan_sampler.get_sample((INDEXING)).content())
                      << "]], "
                      << "[[" << sMERGE << ": "
                      << Misc::avg(scan_sampler.get_sample((MERGE)).content())
                      << "]], "
                      << "[[" << sRESP_MSG << ": "
                      << Misc::avg(scan_sampler.get_sample((RESP_MSG)).content())
                      << "]], "
                      << "[[" << sRESP << ": "
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
            std::cout << "[[" << sCACHE << ": "
                      << Misc::avg(insert_sampler.get_sample((CACHE)).content())
                      << "]], "
                      << "[[" << sCHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << sPRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << sRPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << sCONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_search() const noexcept -> void {
            std::cout << "[[" << sRDMA << ": "
                      << Misc::avg(search_sampler.get_sample((RDMA)).content())
                      << "]], "
                      << "[[" << sCACHE << ": "
                      << Misc::avg(search_sampler.get_sample((CACHE)).content())
                      << "]], "
                      << "[[" << sCACHE_RDMA << ": "
                      << Misc::avg(search_sampler.get_sample((CACHE_RDMA)).content())
                      << "]], "
                      << "[[" << sCHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << sPRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << sRPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << sCONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_update() const noexcept -> void {
            std::cout << "[[" << sCHECK_RPC << ": "
                      << Misc::avg(update_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << sPRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << sRPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << sCONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_remove() const noexcept -> void {
            std::cout << "[[" << sCHECK_RPC << ": "
                      << Misc::avg(remove_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << sPRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << sRPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << sCONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_scan() const noexcept -> void {
            std::cout << "[[" << sRDMA << ": "
                      << Misc::avg(scan_sampler.get_sample((RDMA)).content())
                      << "]], "
                      << "[[" << sCHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample((CHECK_RPC)).content())
                      << "]], "
                      << "[[" << sPRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample((PRE_REQ)).content())
                      << "]], "
                      << "[[" << sRPC << ": "
                      << Misc::avg(common_sampler.get_sample((RPC)).content())
                      << "]], "
                      << "[[" << sCONTI << ": "
                      << Misc::avg(common_sampler.get_sample((CONTI)).content())
                      << "]]";
        }
    }
}
