#include "sampler.hpp"
namespace Hill {
    namespace Sampling {
        const std::string HandleSampler::PARSE = "parse";
        const std::string HandleSampler::CAP_CHECK = "cap_check";
        const std::string HandleSampler::CAP_RECHECK = "cap_recheck";
        const std::string HandleSampler::INDEXING = "indexing";
        const std::string HandleSampler::RESP_MSG = "resp_msg";
        const std::string HandleSampler::RESP = "resp";
        const std::string HandleSampler::MERGE = "merge";

        const std::string ClientSampler::RDMA = "rdma";
        const std::string ClientSampler::CACHE = "cache";
        const std::string ClientSampler::CACHE_RDMA = "cache_rdma";
        const std::string ClientSampler::CHECK_RPC = "check_rpc";
        const std::string ClientSampler::PRE_REQ = "pre_req";
        const std::string ClientSampler::RPC = "rpc";
        const std::string ClientSampler::CONTI = "conti";


        auto HandleSampler::prepare() -> void {
            auto total = 0;
            auto i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({PARSE, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({CAP_CHECK, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({INDEXING, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({CAP_RECHECK, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({RESP_MSG, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({RESP, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({MERGE, i});
            ++total;

            for (int i = 0; i < total; i++) {
                search_sampler.new_sampling_type(batch_size);
                update_sampler.new_sampling_type(batch_size);
                remove_sampler.new_sampling_type(batch_size);
                scan_sampler.new_sampling_type(batch_size);
            }
        }

        auto HandleSampler::report_insert() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]], "
                      << "[[" << CAP_CHECK << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(CAP_CHECK)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(INDEXING)).content())
                      << "]], "
                      << "[[" << CAP_RECHECK << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(CAP_RECHECK)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_search() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(INDEXING)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_update() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]], "
                      << "[[" << CAP_CHECK << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(CAP_CHECK)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(INDEXING)).content())
                      << "]], "
                      << "[[" << CAP_RECHECK << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(CAP_RECHECK)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_remove() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(remove_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(remove_sampler.get_sample(to_sample_type(INDEXING)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(remove_sampler.get_sample(to_sample_type(RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(remove_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]]";
        }

        auto HandleSampler::report_scan() const noexcept -> void {
            std::cout << "[[" << PARSE << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]], "
                      << "[[" << INDEXING << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(INDEXING)).content())
                      << "]], "
                      << "[[" << MERGE << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(MERGE)).content())
                      << "]], "
                      << "[[" << RESP_MSG << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(RESP_MSG)).content())
                      << "]], "
                      << "[[" << RESP << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(RESP)).content())
                      << "]]";
        }

        auto ClientSampler::prepare() -> void {
            auto total = 0;
            auto i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({RDMA, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({CACHE, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({CACHE_RDMA, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({CHECK_RPC, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({PRE_REQ, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({RPC, i});
            ++total;

            i = insert_sampler.new_sampling_type(batch_size).value();
            sample_map.insert({CONTI, i});
            ++total;

            for (int i = 0; i < total; i++) {
                search_sampler.new_sampling_type(batch_size);
                update_sampler.new_sampling_type(batch_size);
                remove_sampler.new_sampling_type(batch_size);
                scan_sampler.new_sampling_type(batch_size);
                common_sampler.new_sampling_type(batch_size);
            }
        }

        auto ClientSampler::report_insert() const noexcept -> void {
            std::cout << "[[" << CACHE << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(CACHE)).content())
                      << "]], "
                      << "[[" << CHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(insert_sampler.get_sample(to_sample_type(CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_search() const noexcept -> void {
            std::cout << "[[" << RDMA << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(RDMA)).content())
                      << "]], "
                      << "[[" << CACHE << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(CACHE)).content())
                      << "]], "
                      << "[[" << CACHE_RDMA << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(CACHE_RDMA)).content())
                      << "]], "
                      << "[[" << CHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(search_sampler.get_sample(to_sample_type(CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_update() const noexcept -> void {
            std::cout << "[[" << CHECK_RPC << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(update_sampler.get_sample(to_sample_type(CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_remove() const noexcept -> void {
            std::cout << "[[" << CHECK_RPC << ": "
                      << Misc::avg(remove_sampler.get_sample(to_sample_type(CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "
                      << Misc::avg(remove_sampler.get_sample(to_sample_type(CONTI)).content())
                      << "]]";
        }

        auto ClientSampler::report_scan() const noexcept -> void {
            std::cout << "[[" << RDMA << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(RDMA)).content())
                      << "]], "
                      << "[[" << CHECK_RPC << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(CHECK_RPC)).content())
                      << "]], "
                      << "[[" << PRE_REQ << ": "
                      << Misc::avg(common_sampler.get_sample(to_sample_type(PRE_REQ)).content())
                      << "]], "
                      << "[[" << RPC << ": "
                      << Misc::avg(scan_sampler.get_sample(to_sample_type(RPC)).content())
                      << "]], "
                      << "[[" << CONTI << ": "

                      << Misc::avg(scan_sampler.get_sample(to_sample_type(CONTI)).content())
                      << "]]";
        }
    }
}
