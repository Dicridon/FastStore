#include "indexing.hpp"

namespace Hill {
    namespace Indexing {
        auto LeafNode::insert(int tid, WAL::Logger *log, Memory::Allocator *alloc, Memory::RemoteMemoryAgent *agent,
                              const char *k, size_t k_sz, const char *v, size_t v_sz) -> Enums::OpStatus {
            if (is_full()) {
                return Enums::OpStatus::NeedSplit;
            }
                
            const char *chars = nullptr;
            size_t sz = 0;
            int i = 0;
            for (i = 0; i < Constants::iDGREE; i++) {
                if (keys[i] == nullptr) {
                    break;
                }
                chars = keys[i]->raw_chars();
                sz = keys[i]->size();
                if (strncmp(chars, k, std::min(sz, k_sz)) <= 0) {
                    break;
                }
            }

            for (int j = Constants::iDGREE - 1; j > i; j--) {
                keys[j] = keys[j - 1];
                values[j] = values[j - 1];
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
                values[i] = Memory::PolymorphicPointer::make_ploymorphic_pointer(ptr);
            } else {
                agent->allocate(tid, total, ptr);
                values[i] = Memory::PolymorphicPointer::make_ploymorphic_pointer(ptr);
                auto &connection = agent->get_peer_connection(values[i].remote_ptr().get_node());
                auto buf = std::make_unique<byte_t[]>(total);
                auto &t = KVPair::HillString::make_string(buf.get(), v, v_sz);
                connection->post_write(t.raw_bytes(), total);
            }
            log->commit(tid);

            // new key is the last one
            if (i == Constants::iDGREE - 1 || keys[i + 1] == nullptr) {
                highkey = keys[i];
            }
            return Enums::OpStatus::Ok;
        }

        auto InnerNode::insert(hill_key_t *split_key, PolymorphicNodePointer child) -> Enums::OpStatus {
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
            keys[i] = split_key;
            children[i + 1] = child;

            for (int j = Constants::iDGREE; j < 0; j--) {
                if (!children[j].is_null()) {
                    if (children[j].is_leaf()) {
                        highkey = children[j].get_as<LeafNode *>()->highkey;
                    } else {
                        highkey = children[j].get_as<InnerNode *>()->highkey;
                    }
                    return Enums::OpStatus::Ok;
                }
            }
            return Enums::OpStatus::Ok;
        }
        
        auto OLFIT::insert(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz) noexcept -> Enums::OpStatus {
            auto [node, ans] = traverse_node(k, k_sz);
            if (ans.empty()) {
                node->version_lock.lock();
            }

            if (!node->is_full()) {
                auto ret = node->insert(tid, logger.get(), alloc, agent, k, k_sz, v, v_sz);
                node->version_lock.unlock();
                return ret;
            }

            auto new_leaf = split_leaf(tid, node, k, k_sz, v, v_sz);
            // root is a leaf
            if (ans.empty()) {
                auto new_root = InnerNode::make_inner();
                new_root->keys[0] = node->highkey;
                new_root->children[0] = node;
                new_root->children[1] = new_leaf;
                new_root->highkey = new_leaf->highkey;
                root = new_root;

                node->version_lock.unlock();
                return Enums::OpStatus::Ok;
            }
            
            node->version_lock.unlock();

            return push_up(tid, node, new_leaf, ans);
        }

        auto OLFIT::split_leaf(int tid, LeafNode *l, const char *k, size_t k_sz, const char *v, size_t v_sz) -> LeafNode * {
            auto ptr = logger->make_log(tid, WAL::Enums::Ops::NodeSplit);
            alloc->allocate(tid, sizeof(LeafNode), ptr);
            auto n = LeafNode::make_leaf(ptr);
            n->right_link = l->right_link;
            Memory::Util::mfence();

            int i = 0;
            const char *chars = nullptr;
            size_t size = 0;
            for (; i < Constants::iDGREE; i++) {
                chars = l->keys[i]->raw_chars();
                size = l->keys[i]->size();
                if (strncmp(chars, k, std::min(k_sz, size)) <= 0) {
                    break;
                }
            }

            auto split = Constants::iDGREE / 2;
            for (int k = split; k < Constants::iDGREE; k++) {
                n->keys[k - split] = l->keys[k];
                n->values[k - split] = l->values[k];
            }
            l->right_link = n;
            n->highkey = n->keys[Constants::iDGREE - 1 - split];
            
            if (i < Constants::iDGREE / 2) {
                for (int k = split; k < Constants::iDGREE; k++) {
                    l->keys[k] = nullptr;
                    l->values[k] = nullptr;                    
                }
                l->insert(tid, logger.get(), alloc, agent, k, k_sz, v, v_sz);
                // should make sure highkey is changed
                l->highkey = l->keys[split];
            } else {
                n->insert(tid, logger.get(), alloc, agent, k, k_sz, v, v_sz);
            }
            
            // Here node split is done in terms of recovery, because inner nodes are reconstructed from
            // leaf nodes, thus though new node is not added to ancestors, split is still finished.
            logger->commit(tid);
            return n;
        }
        
        auto OLFIT::split_inner(int tid, InnerNode *l, hill_key_t *splitkey, PolymorphicNodePointer child) -> InnerNode * {
            auto right = InnerNode::make_inner();
            right->right_link = l->right_link;

            auto split_pos = Constants::iDGREE / 2;
            int i;
            for (i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (*splitkey < *l->keys[i]) {
                    break;
                }
            }
            
            if (i == split_pos) {
                l->highkey = splitkey;
                right->children[0] = child;
                int k;
                for (k = i; k < Constants::iNUM_HIGHKEY; k++) {
                    right->keys[k - i] = l->keys[k];
                    l->keys[k] = nullptr;
                    right->children[k - i + 1] = l->children[k];
                    if (right->highkey == nullptr || *right->highkey < *right->children[k - i + 1].get_highkey()) {
                        right->highkey = right->children[k - i + 1].get_highkey();
                    }
                    l->children[k] = nullptr;
                }
                right->highkey = right->children[k - i].get_highkey();
            } else {
                auto real_split_pos = split_pos;
                int start = 0;
                InnerNode *target = nullptr;
                if (i < split_pos) {
                    start = split_pos;
                    real_split_pos = split_pos - 1;
                    target = l;
                } else {
                    start = split_pos + 1;
                    target = right;
                }
                int k;
                for (k = start; k < Constants::iNUM_HIGHKEY; k++) {
                    right->keys[k - split_pos] = l->keys[k];
                    l->keys[k] = nullptr;
                    right->children[k - split_pos] = l->children[k];
                    if (right->highkey == nullptr || *right->highkey < *right->children[k - split_pos].get_highkey()) {
                        right->highkey = right->children[k - split_pos].get_highkey();
                    }
                    l->children[k] = nullptr;
                }
                right->children[k - split_pos] = l->children[k];
                l->children[k] = nullptr;
                l->highkey = l->keys[real_split_pos];                
                l->keys[real_split_pos] = nullptr;
                target->insert(splitkey, child);                
            }
            l->right_link = right;
            return right;
        }

        auto OLFIT::push_up(int tid, LeafNode *node, LeafNode *new_leaf, std::vector<InnerNode *> &ans) -> Enums::OpStatus {
            InnerNode *inner;
            PolymorphicNodePointer new_node = new_leaf;
            hill_key_t *splitkey = node->highkey;
            size_t version = 0UL;
            while(!ans.empty()) {
                inner = ans.back();
                version = inner->version_lock.version();
                if (*inner->highkey > *splitkey) {
                    if (version == inner->version_lock.version() && !inner->version_lock.is_locked()) {
                        inner->version_lock.lock();
                        //double check
                        if (*inner->highkey <= *splitkey) {
                            inner->version_lock.unlock();
                            continue;
                        }
                        
                        if (!inner->is_full()) {
                            inner->insert(splitkey, new_node);
                            inner->version_lock.unlock();
                            return Enums::OpStatus::Ok;
                        } else {
                            new_node = split_inner(tid, inner, splitkey, new_node);
                            splitkey = inner->highkey;
                            
                            // root
                            if (ans.front() == ans.back()) {
                                auto new_root = InnerNode::make_inner();
                                new_root->keys[0] = inner->highkey;
                                new_root->children[0] = inner;
                                new_root->children[1] = new_node;
                                new_root->highkey = new_node.get_as<InnerNode *>()->highkey;
                                root = new_root;
                                inner->version_lock.unlock();
                                return Enums::OpStatus::Ok;
                            }
                        }
                        inner->version_lock.unlock();
                        ans.pop_back();
                    }
                } else {
                    inner = inner->right_link;
                }
            }
            return Enums::OpStatus::Ok;
        }
            

        auto OLFIT::search(const char *k, size_t k_sz) const noexcept -> Memory::PolymorphicPointer {
        RETRY:
            auto [leaf, _] = traverse_node(k, k_sz);
            const char *chars = nullptr;
            size_t size = 0;
            auto version = leaf->version_lock.version();
            for (int i = 0; i < Constants::iDGREE; i++) {
                if (leaf->keys[i] == nullptr) {
                    return nullptr;
                }
                chars = leaf->keys[i]->raw_chars();
                size = leaf->keys[i]->size();
                if (strncmp(chars, k, std::min(k_sz, size)) == 0) {
                    if (leaf->version_lock.version() == version && !leaf->version_lock.is_locked())
                        return leaf->values[i];
                    else
                        goto RETRY;
                }
            }
            return nullptr;
        }

    }
}
