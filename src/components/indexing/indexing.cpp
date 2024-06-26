#include "indexing.hpp"
namespace Hill {
    namespace Indexing {
        auto LeafNode::insert(int tid, WAL::Logger *log,
                              Memory::Allocator *alloc,
                              Memory::RemoteMemoryAgent *agent,
                              const char *k, size_t k_sz,
                              const char *v, size_t v_sz,
                              const hill_key_t *hk,
                              const hill_value_t *hv)
            -> std::pair<Enums::OpStatus, Memory::PolymorphicPointer>
        {
            if (is_full()) {
                return {Enums::OpStatus::NeedSplit, nullptr};
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
                    return {Enums::OpStatus::RepeatInsert, nullptr};
                }
            }

            for (int j = Constants::iNUM_HIGHKEY - 1; j > i; j--) {
                fingerprints[j] = fingerprints[j - 1];
                keys[j] = keys[j - 1];
                values[j] = values[j - 1];
                value_sizes[j] = value_sizes[j - 1];
            }

            auto &ptr = log->make_log(tid, WAL::Enums::Ops::Insert);
            alloc->allocate(tid, sizeof(KVPair::HillStringHeader) + k_sz, ptr);
            auto fp = CityHash64(k, k_sz);
            memcpy(ptr, hk, hk->object_size());
            fingerprints[i] = fp;
            keys[i] = reinterpret_cast<KVPair::HillString *>(ptr);
            // keys[i] = &KVPair::HillString::make_string(ptr, k, k_sz);
            log->commit(tid);

            // crashing here is ok because valid keys can not find their corresponding values, so just roll
            // the keys
            auto &v_ptr = log->make_log(tid, WAL::Enums::Ops::Insert);
            auto total = sizeof(KVPair::HillStringHeader) + v_sz;
            if (!agent) {
                alloc->allocate(tid, total, v_ptr);
                memcpy(v_ptr, hv, hv->object_size());
                // KVPair::HillString::make_string(v_ptr, v, v_sz);
                values[i] = Memory::PolymorphicPointer::make_polymorphic_pointer(v_ptr);
                value_sizes[i] = total;
            } else {
                agent->allocate(tid, total, v_ptr);
                if (v_ptr == nullptr) {
                    return {Enums::OpStatus::NoMemory, nullptr};
                }
                values[i] = Memory::PolymorphicPointer::make_polymorphic_pointer(v_ptr);
                value_sizes[i] = total;
                auto &connection = agent->get_peer_connection(tid, values[i].remote_ptr().get_node());
                auto buf = std::make_unique<byte_t[]>(total);

                Memory::RemotePointer rp(v_ptr);
                auto &t = KVPair::HillString::make_string(buf.get(), v, v_sz);
                connection->post_write(rp.get_as<byte_ptr_t>(), t.raw_bytes(), total);
                connection->poll_completion_once();
            }
            log->commit(tid);

            return {Enums::OpStatus::Ok, values[i]};
        }

        auto LeafNode::dump() const noexcept -> void {
            std::stringstream ss;
            ss << this;
            ColorizedString c(ss.str(), Colors::Cyan);
            std::cout << ">> Leaf " << c << " reporting with parent " << parent << "\n";
            std::cout << "-->> keys: ";
            for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (keys[i] == nullptr) {
                    break;
                }
                std::cout << ColorizedString(keys[i]->to_string(), Colors::Cyan) << " ";
            }
            std::cout << "\n-->> values: ";
            for (int i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (values[i].is_nullptr()) {
                    break;
                }
                auto v = uint64_t(values[i].raw_ptr());
                std::cout << ColorizedString(std::to_string(v), Colors::Cyan) << " ";
            }
            std::cout << "\n\n";
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
            child.set_parent(this);

            return Enums::OpStatus::Ok;
        }

        auto InnerNode::dump() const noexcept -> void {
            std::stringstream ss;
            ss << this;
            ColorizedString c(ss.str(), Colors::Cyan);
            std::cout << ">> Inner " << c << " reporting and " << parent << "\n";
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

        auto OLFIT::insert(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz,
                           const hill_key_t *hk, const hill_value_t *hv)
            noexcept -> std::pair<Enums::OpStatus, Memory::PolymorphicPointer>
        {
            auto node = traverse_node(k, k_sz);

            if (!node->is_full()) {
                return node->insert(tid, logger, alloc, agent, k, k_sz, v, v_sz, hk, hv);
            }

            auto [new_leaf, value] = split_leaf(tid, node, k, k_sz, v, v_sz, hk, hv);
            // root is a leaf
            if (!node->parent) {
                auto new_root = InnerNode::make_inner();
                new_root->keys[0] = new_leaf->keys[0];
                new_root->children[0] = node;
                new_root->children[1] = new_leaf;
                node->parent = new_leaf->parent = new_root;
                root = new_root;
                return {Enums::OpStatus::Ok, value};
            }

            auto ret = push_up(new_leaf);
            return {ret, value};
        }

        auto OLFIT::split_leaf(int tid, LeafNode *l, const char *k, size_t k_sz, const char *v, size_t v_sz,
                               const hill_key_t *hk, const hill_value_t *hv)
            -> std::pair<LeafNode *, Memory::PolymorphicPointer> {
#ifdef __HILL_PINDEX__
            auto &ptr = logger->make_log(tid, WAL::Enums::Ops::NodeSplit);
            alloc->allocate(tid, sizeof(LeafNode), ptr);
#else
            auto ptr = new byte_t[sizeof(LeafNode)];
#endif
            auto n = LeafNode::make_leaf(ptr);
            n->parent = l->parent;
            n->next = l->next;
            l->next = n;
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
                n->fingerprints[k - split] = l->fingerprints[k];
                n->keys[k - split] = l->keys[k];
                n->values[k - split] = l->values[k];
                n->value_sizes[k - split] = l->value_sizes[k];
            }

            for (int k = split; k < Constants::iNUM_HIGHKEY; k++) {
                l->fingerprints[k] = 0;
                l->keys[k] = nullptr;
                l->values[k] = nullptr;
                l->value_sizes[k] = 0;
            }

            Memory::PolymorphicPointer ret_ptr;
            if (i < Constants::iNUM_HIGHKEY / 2) {
                ret_ptr = l->insert(tid, logger, alloc, agent, k, k_sz, v, v_sz, hk, hv).second;
            } else {
                ret_ptr = n->insert(tid, logger, alloc, agent, k, k_sz, v, v_sz, hk, hv).second;
            }

            // Here node split is done in terms of recovery, because inner nodes are reconstructed from
            // leaf nodes, thus though new node is not added to ancestors, split is still finished.
            logger->commit(tid);
            return {n, ret_ptr};
        }

        auto OLFIT::split_inner(InnerNode *l, const hill_key_t *splitkey, PolymorphicNodePointer child) -> std::pair<InnerNode *, hill_key_t *> {
            auto right = InnerNode::make_inner();
            right->parent = l->parent;

            auto split_pos = Constants::iDEGREE / 2;
            int i;
            for (i = 0; i < Constants::iNUM_HIGHKEY; i++) {
                if (*splitkey < *l->keys[i]) {
                    break;
                }
            }

            hill_key_t *ret_split_key = const_cast<hill_key_t *>(splitkey);
            if (i == split_pos) {
                right->children[0] = child;
                right->children[0].set_parent(right);
                int k;
                for (k = i; k < Constants::iNUM_HIGHKEY; k++) {
                    right->keys[k - i] = l->keys[k];
                    l->keys[k] = nullptr;
                    right->children[k - i + 1] = l->children[k + 1];
                    right->children[k - i + 1].set_parent(right);
                    l->children[k + 1] = nullptr;
                }
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
                    right->children[k - start].set_parent(right);
                    l->children[k] = nullptr;
                }
                right->children[k - start] = l->children[k];
                right->children[k - start].set_parent(right);
                l->children[k] = nullptr;
                l->keys[real_split_pos] = nullptr;
                target->insert(splitkey, child);
            }
            return {right, ret_split_key};
        }

        auto OLFIT::push_up(LeafNode *new_leaf) -> Enums::OpStatus {
            InnerNode *inner;
            PolymorphicNodePointer new_node = new_leaf;
            hill_key_t *splitkey = new_leaf->keys[0];

            inner = new_leaf->parent;
            while(inner) {
                if (!inner->is_full()) {
                    inner->insert(splitkey, new_node);
                    new_node.set_parent(inner);
                    return Enums::OpStatus::Ok;
                } else {
                    auto split_context = split_inner(inner, splitkey, new_node);
                    new_node = split_context.first;
                    splitkey = split_context.second;
                    // root
                    if (!inner->parent) {
                        auto new_root = InnerNode::make_inner();
                        new_root->keys[0] = splitkey;
                        inner->parent = new_root;
                        new_node.set_parent(new_root);
                        new_root->children[0] = inner;
                        new_root->children[1] = new_node;
                        root = new_root;
                        return Enums::OpStatus::Ok;
                    }
                }
                inner = inner->parent;
            }
            return Enums::OpStatus::Ok;
        }

        auto OLFIT::search(const char *k, size_t k_sz) const noexcept -> std::pair<Memory::PolymorphicPointer, size_t> {
            auto [leaf, i] = get_pos_of(k, k_sz);
            if (i == -1) {
                return {nullptr, 0};
            }
            if (leaf->keys[i]->compare(k, k_sz) == 0) {
                return {leaf->values[i], leaf->value_sizes[i]};
            }
            return {nullptr, 0};
        }

        auto OLFIT::update(int tid, const char *k, size_t k_sz, const char *v, size_t v_sz)
            noexcept -> std::pair<Enums::OpStatus, Memory::PolymorphicPointer>
        {
            auto [leaf, i] = get_pos_of(k, k_sz);
            if (i == -1) {
                return {Enums::OpStatus::Failed, nullptr};
            }

            auto &ptr = logger->make_log(tid, WAL::Enums::Ops::Update);
            auto total = sizeof(KVPair::HillStringHeader) + v_sz;
            if (!agent) {
                alloc->allocate(tid, total, ptr);
                if (ptr == nullptr) {
                    return {Enums::OpStatus::NoMemory, nullptr};
                }

                KVPair::HillString::make_string(ptr, v, v_sz);
                auto &old = logger->make_log(tid, WAL::Enums::Ops::Delete);
                old = leaf->values[i].get_as<byte_ptr_t>();
                leaf->values[i] = ptr;
                leaf->value_sizes[i] = v_sz;
                alloc->free(tid, old);

                logger->commit(tid);
            } else {
                agent->allocate(tid, total, ptr);
                if (ptr == nullptr) {
                    return {Enums::OpStatus::NoMemory, nullptr};
                }

                auto &old = logger->make_log(tid, WAL::Enums::Ops::Delete);
                old = leaf->values[i].remote_ptr().raw_ptr();

                auto r = leaf->values[i];
                leaf->values[i] = ptr;
                leaf->value_sizes[i] = v_sz;

                auto &connection = agent->get_peer_connection(tid, leaf->values[i].remote_ptr().get_node());
                auto buf = std::make_unique<byte_t[]>(total);
                auto &t = KVPair::HillString::make_string(buf.get(), v, v_sz);

                Memory::RemotePointer rp(ptr);
                connection->post_write(rp.get_as<byte_ptr_t>(), t.raw_bytes(), total);
                connection->poll_completion_once() ;

                if (r.is_local()) {
                    alloc->free(tid, old);
                } else {
                    auto remote = r.remote_ptr();
                    agent->free(tid, remote);
                }

                logger->commit(tid);
            }
            logger->commit(tid);
            return {Enums::OpStatus::Ok, leaf->values[i]};
        }

        auto OLFIT::remove(int tid, const char *k, size_t k_sz) noexcept -> Enums::OpStatus {
            auto [leaf, i] = get_pos_of(k, k_sz);
            if (i == -1) {
                return Enums::OpStatus::Failed;
            }

            if (leaf->values[i].is_remote()) {
                auto &ptr = logger->make_log(tid, WAL::Enums::Ops::Delete);
                ptr = reinterpret_cast<byte_ptr_t>(leaf->keys[i]);
                // we only need to remember the key here because leaf node is a natural log recording both key and value
                leaf->keys[i]->invalidate();

                auto &connection = agent->get_peer_connection(tid, leaf->values[i].remote_ptr().get_node());
                KVPair::HillStringHeader buf {
                    .valid = 0,
                    .length = 0,
                };

                connection->post_write(leaf->values[i].get_as<byte_ptr_t>(),
                                       reinterpret_cast<uint8_t *>(&buf),
                                       sizeof(KVPair::HillStringHeader));
                connection->poll_completion_once();
                leaf->keys[i]->invalidate();
                alloc->free(tid, ptr);
            } else {
                auto &ptr = logger->make_log(tid, WAL::Enums::Ops::Delete);
                ptr = reinterpret_cast<byte_ptr_t>(leaf->keys[i]);
                auto vp = reinterpret_cast<byte_ptr_t>(leaf->values[i].local_ptr());
                leaf->values[i].get_as<KVPair::HillString *>()->invalidate();
                leaf->keys[i]->invalidate();
                alloc->free(tid, vp);
                alloc->free(tid, ptr);
            }
            logger->commit(tid);
            return Enums::OpStatus::Ok;
        }

        auto OLFIT::scan(const char *k, size_t k_sz, size_t num) -> std::vector<ScanHolder> {
            std::vector<ScanHolder> ret;
            ret.reserve(num);

            auto leaf = traverse_node(k, k_sz);
            auto cursor = 0;
            for (; cursor < Constants::iNUM_HIGHKEY; cursor++) {
                if (leaf->keys[cursor] == nullptr) {
                    leaf = leaf->next;
                    break;
                }
                if (leaf->keys[cursor]->compare(k, k_sz) >= 0)
                    break;
            }

            while (num > 0 && leaf != nullptr) {
                for (; cursor < Constants::iNUM_HIGHKEY && num > 0; cursor++) {
                    if (leaf->keys[cursor] == nullptr)
                        break;
                    ret.emplace_back(leaf->keys[cursor], leaf->values[cursor]);
                    --num;
                }

                leaf = leaf->next;
                cursor = 0;
            }

            return ret;
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
