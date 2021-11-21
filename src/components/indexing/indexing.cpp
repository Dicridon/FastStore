#include "indexing.hpp"

namespace Hill {
    namespace Indexing {
        auto LeafNode::insert(int tid, WAL::Logger *log, Memory::Allocator *alloc, Memory::RemoteMemoryAgent *agent, const char *k, size_t k_sz, const char *v, size_t v_sz) -> Enums::OpStatus {
            if (is_full()) {
                return Enums::OpStatus::NeedSplit;
            }

            int i = 0;
            for (i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (keys[i] == nullptr) {
                    break;
                }

                auto c = keys[i]->compare(k, k_sz);
                if (c > 0) {
                    break;
                }

                if (c == 0) {
                    return Enums::OpStatus::RepeatInsert;
                }
            }

            for (int j = Constants::iNUM_HIGHKEY - 1; j > i; j--) {
                keys[j] = keys[j - 1];
                values[j] = values[j - 1];
                value_sizes[j] = value_sizes[j - 1];
            }

            auto ptr = log->make_log(tid, WAL::Enums::Ops::Insert);
            alloc->allocate(tid, sizeof(KVPair::HillStringHeader) + k_sz, ptr);
            keys[i] = &KVPair::HillString::make_string(ptr, k, k_sz);
            log->commit(tid);

            // crashing here is ok because valid keys can not find their corresponding values, so just roll
            // the keys
            ptr = log->make_log(tid, WAL::Enums::Ops::Insert);
            auto total = sizeof(KVPair::HillStringHeader) + v_sz;
            if (!agent) {
                alloc->allocate(tid, total, ptr);
                KVPair::HillString::make_string(ptr, v, v_sz);
                values[i] = Memory::PolymorphicPointer::make_polymorphic_pointer(ptr);
                value_sizes[i] = total;
            } else {
                agent->allocate(tid, total, ptr);
                values[i] = Memory::PolymorphicPointer::make_polymorphic_pointer(ptr);
                value_sizes[i] = total;
                auto &connection = agent->get_peer_connection(values[i].remote_ptr().get_node());
                auto buf = std::make_unique<byte_t[]>(total);
                auto &t = KVPair::HillString::make_string(buf.get(), v, v_sz);
                connection->post_write(t.raw_bytes(), total);
            }
            log->commit(tid);

            // new key is the last one
            if (i == Constants::iNUM_HIGHKEY - 1 || keys[i + 1] == nullptr) {
                highkey = keys[i];
            }
            return Enums::OpStatus::Ok;
        }

        auto LeafNode::dump() const noexcept -> void {
            std::stringstream ss;
            ss << this;
            ColorizedString c(ss.str(), Colors::Cyan);
            ColorizedString h(std::string(highkey->raw_chars(), highkey->size()), Colors::Cyan);
            std::cout << ">> Leaf " << c << " reporting with highkey: " << h << "\n";
            std::cout << "-->> keys: ";
            for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (keys[i] == nullptr) {
                    break;
                }
                std::cout << ColorizedString(keys[i]->to_string(), Colors::Cyan) << " ";
            }
            std::cout << "\n";
        }

        auto InnerNode::insert(const hill_key_t *split_key, PolymorphicNodePointer child) -> Enums::OpStatus {
            if (is_full()) {
                return Enums::OpStatus::NeedSplit;
            }

            int i;
            for (i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (keys[i] == nullptr || *split_key < *keys[i]) {
                    break;
                }
            }

            for (int j = Constants::iNUM_HIGHKEY - 1; j > i; j--) {
                keys[j] = keys[j - 1];
                children[j + 1] = children[j];
            }
            keys[i] = const_cast<hill_key_t *>(split_key);
            children[i + 1] = child;

            for (int j = Constants::iDEGREE - 1; j >= 0; j--) {
                if (!children[j].is_null()) {
                    highkey = children[j].get_highkey();
                    return Enums::OpStatus::Ok;
                }
            }
            return Enums::OpStatus::Ok;
        }

        auto InnerNode::dump() const noexcept -> void {
            std::stringstream ss;
            ss << this;
            ColorizedString c(ss.str(), Colors::Cyan);
            ColorizedString h(std::string(highkey->raw_chars(), highkey->size()), Colors::Yellow);
            std::cout << ">> Inner " << c << " reporting with highkey " << h << "\n";
            std::cout << "-->> keys: ";

            for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (keys[i] == nullptr) {
                    break;
                }
                std::cout << ColorizedString(keys[i]->to_string(), Colors::Yellow) << " ";
            }
            std::cout << "\n-->> children: ";

            for (int i = 0; i < Constants::iDEGREE; i++) {
                if (children[i].is_null()) {
                    break;
                }
                ss.str("");
                ss << children[i].value;
                std::cout << ColorizedString(ss.str(), Colors::Yellow) << " ";
            }
            std::cout << "\n";

            for (int i = 0; i < Constants::iDEGREE; i++) {
                if (children[i].is_null()) {
                    break;
                }
                if (children[i].is_leaf()) {
                    children[i].get_as<LeafNode *>()->dump();
                } else {
                    children[i].get_as<InnerNode *>()->dump();
                }
            }
        }

        auto OLFIT::insert(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz) noexcept -> Enums::OpStatus {
            std::stringstream ss;
            ss << "Thread " << tid << " start traversing for " << std::string(k, k_sz);
            debug_logger->log_info(ss.str());
            ss.str("");

            auto [node_, ans] = traverse_node(k, k_sz);
            node_->lock();
            auto node = move_right(node_, k, k_sz);

            ss << "Thread " << tid << " trying writing " << std::string(k, k_sz) << " to node " << node;
            debug_logger->log_info(ss.str());
            ss.str("");
            if (!node->is_full()) {
                auto ret = node->insert(tid, logger, alloc, agent, k, k_sz, v, v_sz);
                node->unlock();
                debug_logger->log_info("Done");
                return ret;
            }

            auto new_leaf = split_leaf(tid, node, k, k_sz, v, v_sz);
            ss << "Splitting leaf node " << node << " and got " << new_leaf;
            debug_logger->log_info(ss.str());
            ss.str("");
            // root is a leaf
            if (ans.empty()) {
                auto new_root = InnerNode::make_inner();
                new_root->keys[0] = new_leaf->keys[0];
                new_root->children[0] = node;
                new_root->children[1] = new_leaf;
                new_root->highkey = new_leaf->highkey;
                root = new_root;
                ss << "New root created: " << new_root << " with ";
                for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                    if (new_root->keys[i]) {
                        ss << new_root->keys[i]->to_string() << " ";
                    }
                }
                ss << " and highkey " << new_root->highkey->to_string();
                debug_logger->log_info(ss.str());
                ss.str("");
                node->unlock();
                return Enums::OpStatus::Ok;
            }

            auto ret = push_up(new_leaf, ans);
            node->unlock();
            return ret;
        }

        auto OLFIT::split_leaf(int tid, LeafNode *l, const char *k, size_t k_sz, const char *v, size_t v_sz) -> LeafNode * {
            auto ptr = logger->make_log(tid, WAL::Enums::Ops::NodeSplit);
            alloc->allocate(tid, sizeof(LeafNode), ptr);
            auto n = LeafNode::make_leaf(ptr);
            n->right_link = l->right_link;
            Memory::Util::mfence();

            int i = 0;
            for (; i < Constants::iNUM_HIGHKEY; i++) {
                if (l->keys[i]->compare(k, k_sz) > 0) {
                    break;
                }
            }

            auto split = Constants::iNUM_HIGHKEY / 2;
            if (i < split) {
                split -= 1;
            }
            for (int k = split; k < Constants::iNUM_HIGHKEY; k++) {
                n->keys[k - split] = l->keys[k];
                n->values[k - split] = l->values[k];
                n->value_sizes[k - split] = l->value_sizes[k];
            }
            n->highkey = n->keys[Constants::iNUM_HIGHKEY - 1 - split];
            l->right_link = n;

            for (int k = split; k < Constants::iNUM_HIGHKEY; k++) {
                l->keys[k] = nullptr;
                l->values[k] = nullptr;
                l->value_sizes[k] = 0;
            }

            std::stringstream ss;
            if (i < Constants::iNUM_HIGHKEY / 2) {
                ss << "Actually write to " << l;
                debug_logger->log_info(ss.str());
                l->insert(tid, logger, alloc, agent, k, k_sz, v, v_sz);
                // should make sure highkey is changed
                l->highkey = l->keys[split];
            } else {
                ss << "Actually write to " << n;
                debug_logger->log_info(ss.str());
                n->insert(tid, logger, alloc, agent, k, k_sz, v, v_sz);
                l->highkey = l->keys[split - 1];
            }

            // Here node split is done in terms of recovery, because inner nodes are reconstructed from
            // leaf nodes, thus though new node is not added to ancestors, split is still finished.
            logger->commit(tid);
            return n;
        }

        auto OLFIT::split_inner(InnerNode *l, const hill_key_t *splitkey, PolymorphicNodePointer child) -> std::pair<InnerNode *, hill_key_t *> {
            auto right = InnerNode::make_inner();
            right->right_link = l->right_link;

            auto split_pos = Constants::iDEGREE / 2;
            int i;
            for (i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (*splitkey < *l->keys[i]) {
                    break;
                }
            }

            hill_key_t *ret_split_key = const_cast<hill_key_t *>(splitkey);
            if (i == split_pos) {
                l->highkey = l->children[split_pos].get_highkey();
                right->children[0] = child;
                int k;
                for (k = i; k < Constants::iNUM_HIGHKEY; k++) {
                    right->keys[k - i] = l->keys[k];
                    l->keys[k] = nullptr;
                    right->children[k - i + 1] = l->children[k + 1];
                    l->children[k + 1] = nullptr;
                }
                right->highkey = right->children[k - i].get_highkey();
            } else {
                auto real_split_pos = split_pos;
                int start = 0;
                InnerNode *target = nullptr;
                if (i < split_pos) {
                    // one key after split_pos to keep >= between keys and children
                    start = split_pos;
                    real_split_pos = split_pos - 1;

                    target = l;
                } else {
                    // one key after split_pos to keep >= between keys and children
                    start = split_pos + 1;
                    target = right;
                }

                ret_split_key = l->keys[real_split_pos];
                int k;
                for (k = start; k < Constants::iNUM_HIGHKEY; k++) {
                    right->keys[k - start] = l->keys[k];
                    l->keys[k] = nullptr;
                    right->children[k - start] = l->children[k];
                    l->children[k] = nullptr;
                }
                right->children[k - start] = l->children[k];
                right->highkey = right->children[k - start].get_highkey();
                l->highkey = l->children[real_split_pos].get_highkey();
                l->children[k] = nullptr;
                l->keys[real_split_pos] = nullptr;
                target->insert(splitkey, child);
            }
            l->right_link = right;
            return {right, ret_split_key};
        }

        auto OLFIT::push_up(LeafNode *new_leaf, std::vector<InnerNode *> &ans) -> Enums::OpStatus {
            InnerNode *inner;
            PolymorphicNodePointer new_node = new_leaf;
            hill_key_t *splitkey = new_leaf->keys[0];
            std::stringstream ss;
            inner = ans.back();
            while(!ans.empty()) {
                inner->lock();

                ss << "Checking node " << inner << " with rightlinkg being " << inner->right_link;
                ss << " ,highkey is " << inner->highkey->to_string() << " and splitkey is " << splitkey->to_string();
                debug_logger->log_info(ss.str());
                ss.str("");
                //double check if a split is done
                auto less = *inner->highkey <= *splitkey;
                if (less) {
                    if (inner->right_link) {
                        ss << "Moving from " << inner << " to " << inner->right_link;
                        debug_logger->log_info(ss.str());
                        ss.str("");
                        auto old = inner;
                        inner = inner->right_link;
                        inner->lock();
                        old->unlock();
                        continue;
                    }
                }

                if (!inner->is_full()) {
                    ss << "Inserting new node " << new_node.value << " to " << inner;
                    debug_logger->log_info(ss.str());
                    ss.str("");
                    inner->insert(splitkey, new_node);
                    inner->unlock();
                    return Enums::OpStatus::Ok;
                } else {
                    auto split_context = split_inner(inner, splitkey, new_node);
                    new_node = split_context.first;
                    splitkey = split_context.second;
                    ss << "Splitting inner node " << inner << " and got " << new_node.value;
                    debug_logger->log_info(ss.str());
                    ss.str("");
                    // root
                    if (ans.front() == ans.back()) {
                        auto new_root = InnerNode::make_inner();
                        new_root->keys[0] = splitkey;
                        new_root->children[0] = inner;
                        new_root->children[1] = new_node;
                        new_root->highkey = new_node.get_highkey();
                        root = new_root;
                        ss << "New root created: " << new_root << " with ";
                        for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                            if (new_root->keys[i]) {
                                ss << new_root->keys[i]->to_string() << " ";
                            }
                        }
                        ss << " and highkey " << new_root->highkey->to_string();
                        inner->unlock();
                        return Enums::OpStatus::Ok;
                    }
                }
                inner->unlock();
                ans.pop_back();
                inner = ans.back();
            }
            return Enums::OpStatus::Ok;
        }

        auto OLFIT::search(const char *k, size_t k_sz) const noexcept -> std::pair<Memory::PolymorphicPointer, size_t> {
        RETRY:
            auto leaf = traverse_node_no_tracing(k, k_sz);
            auto version = leaf->version();
            for (int i = 0; i < Constants::iDEGREE; i++) {
                if (leaf->keys[i] == nullptr) {
                    return {nullptr, 0};
                }
                if (leaf->keys[i]->compare(k, k_sz) == 0) {
                    if (leaf->version() == version && !leaf->is_locked())
                        return {leaf->values[i], leaf->value_sizes[i]};
                    else
                        goto RETRY;
                }
            }
            return {nullptr, 0};
        }

        auto OLFIT::dump() const noexcept -> void {
            if (root.is_leaf()) {
                root.get_as<LeafNode *>()->dump();
            } else {
                root.get_as<InnerNode *>()->dump();
            }
        }
    }
}
