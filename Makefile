CXX=clang++
CXXFLAGS=-O0 -g -Wunused-parameter -Wunused-variable -Wunused-private-field -Wunused-const-variable -std=c++17 -Ithird-party/eRPC/src -Ithird-party/eRPC/third_party/asio/include -DERPC_INFINIBAND=true -DROCE=true -I./src/components
LDFLAGS=-Lthird-party/eRPC/build -lerpc
LDLIBS=-libverbs -lpmem -lpthread -Lthird-party/eRPC/build -lerpc -lnuma

TARGET_DIR=./target
TARGET=$(TARGET_DIR)/hill
OBJ_DIR=./obj
SRC_DIR=./src
COMPONENTS_DIR=./src/components

SRC_HILL=./src/hill.cpp
SRC_MAIN=./src/main.cpp
SRC_INDEXING_INDEXING=./src/components/indexing/indexing.cpp
SRC_COLORING_COLORING=./src/components/coloring/coloring.cpp
SRC_RPC_WRAPPER_RPC_WRAPPER=./src/components/rpc_wrapper/rpc_wrapper.cpp
SRC_KV_PAIR_KV_PAIR=./src/components/kv_pair/kv_pair.cpp
SRC_TESTS_TESTS=./src/components/tests/tests.cpp
SRC_WAL_WAL=./src/components/wal/wal.cpp
SRC_CMD_PARSER_CMD_PARSER=./src/components/cmd_parser/cmd_parser.cpp
SRC_REMOTE_MEMORY_REMOTE_MEMORY=./src/components/remote_memory/remote_memory.cpp
SRC_RDMA_RDMA=./src/components/rdma/rdma.cpp
SRC_CLUSTER_CLUSTER=./src/components/cluster/cluster.cpp
SRC_MEMORY_MANAGER_MEMORY_MANAGER=./src/components/memory_manager/memory_manager.cpp
SRC_CONFIG_CONFIG=./src/components/config/config.cpp
SRC_STORE_STORE=./src/components/store/store.cpp
SRC_READ_CACHE_READ_CACHE=./src/components/read_cache/read_cache.cpp
SRC_ENGINE_ENGINE=./src/components/engine/engine.cpp
SRC_WORKLOAD_WORKLOAD=./src/components/workload/workload.cpp
SRC_MISC_MISC=./src/components/misc/misc.cpp
SRC_TEST_CACHE=./tests/test_cache.cpp
SRC_TEST_MEMORY_MANAGER=./tests/test_memory_manager.cpp
SRC_TEST_POLYMORPHIC_POINTER=./tests/test_polymorphic_pointer.cpp
SRC_TEST_COLORING=./tests/test_coloring.cpp
SRC_TEST_WORKLOAD=./tests/test_workload.cpp
SRC_TEST_WAL=./tests/test_wal.cpp
SRC_TEST_CMD_PARSER=./tests/test_cmd_parser.cpp
SRC_TEST_RDMA=./tests/test_rdma.cpp
SRC_TEST_KV_PAIR=./tests/test_kv_pair.cpp
SRC_TEST_STORE=./tests/test_store.cpp
SRC_TEST_ERPC=./tests/test_erpc.cpp
SRC_TEST_ENGINE=./tests/test_engine.cpp
SRC_TEST_CLUSTER=./tests/test_cluster.cpp
SRC_TEST_STRING=./tests/test_string.cpp
SRC_TEST_INDEXING=./tests/test_indexing.cpp
SRC_TEST_MISC=./tests/test_misc.cpp
SRC_TEST_REMOTE_POINTER=./tests/test_remote_pointer.cpp

HDR_HILL=./src/hill.hpp
HDR_INDEXING_INDEXING=./src/components/indexing/indexing.hpp
HDR_COLORING_COLORING=./src/components/coloring/coloring.hpp
HDR_RPC_WRAPPER_RPC_WRAPPER=./src/components/rpc_wrapper/rpc_wrapper.hpp
HDR_KV_PAIR_KV_PAIR=./src/components/kv_pair/kv_pair.hpp
HDR_TESTS_TESTS=./src/components/tests/tests.hpp
HDR_WAL_WAL=./src/components/wal/wal.hpp
HDR_CMD_PARSER_CMD_PARSER=./src/components/cmd_parser/cmd_parser.hpp
HDR_REMOTE_MEMORY_REMOTE_MEMORY=./src/components/remote_memory/remote_memory.hpp
HDR_RDMA_RDMA=./src/components/rdma/rdma.hpp
HDR_CLUSTER_CLUSTER=./src/components/cluster/cluster.hpp
HDR_MEMORY_MANAGER_MEMORY_MANAGER=./src/components/memory_manager/memory_manager.hpp
HDR_CONFIG_CONFIG=./src/components/config/config.hpp
HDR_STORE_STORE=./src/components/store/store.hpp
HDR_READ_CACHE_READ_CACHE=./src/components/read_cache/read_cache.hpp
HDR_ENGINE_ENGINE=./src/components/engine/engine.hpp
HDR_WORKLOAD_WORKLOAD=./src/components/workload/workload.hpp
HDR_MISC_MISC=./src/components/misc/misc.hpp

OBJ_HILL=./obj/hill.o
OBJ_MAIN=./obj/main.o
OBJ_INDEXING_INDEXING=./obj/indexing_indexing.o
OBJ_COLORING_COLORING=./obj/coloring_coloring.o
OBJ_RPC_WRAPPER_RPC_WRAPPER=./obj/rpc_wrapper_rpc_wrapper.o
OBJ_KV_PAIR_KV_PAIR=./obj/kv_pair_kv_pair.o
OBJ_TESTS_TESTS=./obj/tests_tests.o
OBJ_WAL_WAL=./obj/wal_wal.o
OBJ_CMD_PARSER_CMD_PARSER=./obj/cmd_parser_cmd_parser.o
OBJ_REMOTE_MEMORY_REMOTE_MEMORY=./obj/remote_memory_remote_memory.o
OBJ_RDMA_RDMA=./obj/rdma_rdma.o
OBJ_CLUSTER_CLUSTER=./obj/cluster_cluster.o
OBJ_MEMORY_MANAGER_MEMORY_MANAGER=./obj/memory_manager_memory_manager.o
OBJ_CONFIG_CONFIG=./obj/config_config.o
OBJ_STORE_STORE=./obj/store_store.o
OBJ_READ_CACHE_READ_CACHE=./obj/read_cache_read_cache.o
OBJ_ENGINE_ENGINE=./obj/engine_engine.o
OBJ_WORKLOAD_WORKLOAD=./obj/workload_workload.o
OBJ_MISC_MISC=./obj/misc_misc.o
OBJ_TEST_CACHE=./obj/test_cache.o
OBJ_TEST_MEMORY_MANAGER=./obj/test_memory_manager.o
OBJ_TEST_POLYMORPHIC_POINTER=./obj/test_polymorphic_pointer.o
OBJ_TEST_COLORING=./obj/test_coloring.o
OBJ_TEST_WORKLOAD=./obj/test_workload.o
OBJ_TEST_WAL=./obj/test_wal.o
OBJ_TEST_CMD_PARSER=./obj/test_cmd_parser.o
OBJ_TEST_RDMA=./obj/test_rdma.o
OBJ_TEST_KV_PAIR=./obj/test_kv_pair.o
OBJ_TEST_STORE=./obj/test_store.o
OBJ_TEST_ERPC=./obj/test_erpc.o
OBJ_TEST_ENGINE=./obj/test_engine.o
OBJ_TEST_CLUSTER=./obj/test_cluster.o
OBJ_TEST_STRING=./obj/test_string.o
OBJ_TEST_INDEXING=./obj/test_indexing.o
OBJ_TEST_MISC=./obj/test_misc.o
OBJ_TEST_REMOTE_POINTER=./obj/test_remote_pointer.o

OUT_OBJS=$(OBJ_HILL) $(OBJ_MAIN) $(OBJ_INDEXING_INDEXING) $(OBJ_COLORING_COLORING) $(OBJ_RPC_WRAPPER_RPC_WRAPPER) $(OBJ_KV_PAIR_KV_PAIR) $(OBJ_WAL_WAL) $(OBJ_CMD_PARSER_CMD_PARSER) $(OBJ_REMOTE_MEMORY_REMOTE_MEMORY) $(OBJ_RDMA_RDMA) $(OBJ_CLUSTER_CLUSTER) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG) $(OBJ_STORE_STORE) $(OBJ_READ_CACHE_READ_CACHE) $(OBJ_ENGINE_ENGINE) $(OBJ_WORKLOAD_WORKLOAD) $(OBJ_MISC_MISC)
TEST_OBJS=$(OBJ_TESTS_TESTS) $(OBJ_TEST_CACHE) $(OBJ_TEST_MEMORY_MANAGER) $(OBJ_TEST_POLYMORPHIC_POINTER) $(OBJ_TEST_COLORING) $(OBJ_TEST_WORKLOAD) $(OBJ_TEST_WAL) $(OBJ_TEST_CMD_PARSER) $(OBJ_TEST_RDMA) $(OBJ_TEST_KV_PAIR) $(OBJ_TEST_STORE) $(OBJ_TEST_ERPC) $(OBJ_TEST_ENGINE) $(OBJ_TEST_CLUSTER) $(OBJ_TEST_STRING) $(OBJ_TEST_INDEXING) $(OBJ_TEST_MISC) $(OBJ_TEST_REMOTE_POINTER)

TEST_CACHE=./target/test_cache
TEST_MEMORY_MANAGER=./target/test_memory_manager
TEST_POLYMORPHIC_POINTER=./target/test_polymorphic_pointer
TEST_COLORING=./target/test_coloring
TEST_WORKLOAD=./target/test_workload
TEST_WAL=./target/test_wal
TEST_CMD_PARSER=./target/test_cmd_parser
TEST_RDMA=./target/test_rdma
TEST_KV_PAIR=./target/test_kv_pair
TEST_STORE=./target/test_store
TEST_ERPC=./target/test_erpc
TEST_ENGINE=./target/test_engine
TEST_CLUSTER=./target/test_cluster
TEST_STRING=./target/test_string
TEST_INDEXING=./target/test_indexing
TEST_MISC=./target/test_misc
TEST_REMOTE_POINTER=./target/test_remote_pointer
TESTS=$(TEST_CACHE) $(TEST_MEMORY_MANAGER) $(TEST_POLYMORPHIC_POINTER) $(TEST_COLORING) $(TEST_WORKLOAD) $(TEST_WAL) $(TEST_CMD_PARSER) $(TEST_RDMA) $(TEST_KV_PAIR) $(TEST_STORE) $(TEST_ERPC) $(TEST_ENGINE) $(TEST_CLUSTER) $(TEST_STRING) $(TEST_INDEXING) $(TEST_MISC) $(TEST_REMOTE_POINTER)

HILL_DEP=$(SRC_HILL) $(HDR_HILL)
MAIN_DEP=$(SRC_MAIN)
INDEXING_INDEXING_DEP=$(SRC_INDEXING_INDEXING) $(HDR_INDEXING_INDEXING) $(REMOTE_MEMORY_REMOTE_MEMORY_DEP) $(WAL_WAL_DEP) $(KV_PAIR_KV_PAIR_DEP) $(COLORING_COLORING_DEP)
COLORING_COLORING_DEP=$(SRC_COLORING_COLORING) $(HDR_COLORING_COLORING)
RPC_WRAPPER_RPC_WRAPPER_DEP=$(SRC_RPC_WRAPPER_RPC_WRAPPER) $(HDR_RPC_WRAPPER_RPC_WRAPPER)
KV_PAIR_KV_PAIR_DEP=$(SRC_KV_PAIR_KV_PAIR) $(HDR_KV_PAIR_KV_PAIR) $(MEMORY_MANAGER_MEMORY_MANAGER_DEP)
TESTS_TESTS_DEP=$(SRC_TESTS_TESTS) $(HDR_TESTS_TESTS)
WAL_WAL_DEP=$(SRC_WAL_WAL) $(HDR_WAL_WAL) $(MEMORY_MANAGER_MEMORY_MANAGER_DEP)
CMD_PARSER_CMD_PARSER_DEP=$(SRC_CMD_PARSER_CMD_PARSER) $(HDR_CMD_PARSER_CMD_PARSER)
REMOTE_MEMORY_REMOTE_MEMORY_DEP=$(SRC_REMOTE_MEMORY_REMOTE_MEMORY) $(HDR_REMOTE_MEMORY_REMOTE_MEMORY) $(RDMA_RDMA_DEP) $(CLUSTER_CLUSTER_DEP)
RDMA_RDMA_DEP=$(SRC_RDMA_RDMA) $(HDR_RDMA_RDMA) $(MEMORY_MANAGER_MEMORY_MANAGER_DEP)
CLUSTER_CLUSTER_DEP=$(SRC_CLUSTER_CLUSTER) $(HDR_CLUSTER_CLUSTER) $(MEMORY_MANAGER_MEMORY_MANAGER_DEP) $(MISC_MISC_DEP)
MEMORY_MANAGER_MEMORY_MANAGER_DEP=$(SRC_MEMORY_MANAGER_MEMORY_MANAGER) $(HDR_MEMORY_MANAGER_MEMORY_MANAGER) $(CONFIG_CONFIG_DEP)
CONFIG_CONFIG_DEP=$(SRC_CONFIG_CONFIG) $(HDR_CONFIG_CONFIG)
STORE_STORE_DEP=$(SRC_STORE_STORE) $(HDR_STORE_STORE) $(INDEXING_INDEXING_DEP) $(READ_CACHE_READ_CACHE_DEP) $(ENGINE_ENGINE_DEP) $(RPC_WRAPPER_RPC_WRAPPER_DEP) $(WORKLOAD_WORKLOAD_DEP)
READ_CACHE_READ_CACHE_DEP=$(SRC_READ_CACHE_READ_CACHE) $(HDR_READ_CACHE_READ_CACHE) $(KV_PAIR_KV_PAIR_DEP) $(REMOTE_MEMORY_REMOTE_MEMORY_DEP)
ENGINE_ENGINE_DEP=$(SRC_ENGINE_ENGINE) $(HDR_ENGINE_ENGINE) $(WAL_WAL_DEP) $(REMOTE_MEMORY_REMOTE_MEMORY_DEP)
WORKLOAD_WORKLOAD_DEP=$(SRC_WORKLOAD_WORKLOAD) $(HDR_WORKLOAD_WORKLOAD)
MISC_MISC_DEP=$(SRC_MISC_MISC) $(HDR_MISC_MISC)
TEST_CACHE_DEP=$(SRC_TEST_CACHE) $(HDR_TEST_CACHE) $(CMD_PARSER_CMD_PARSER_DEP)
TEST_MEMORY_MANAGER_DEP=$(SRC_TEST_MEMORY_MANAGER) $(HDR_TEST_MEMORY_MANAGER) $(MEMORY_MANAGER_MEMORY_MANAGER_DEP)
TEST_POLYMORPHIC_POINTER_DEP=$(SRC_TEST_POLYMORPHIC_POINTER) $(HDR_TEST_POLYMORPHIC_POINTER) $(REMOTE_MEMORY_REMOTE_MEMORY_DEP)
TEST_COLORING_DEP=$(SRC_TEST_COLORING) $(HDR_TEST_COLORING) $(COLORING_COLORING_DEP)
TEST_WORKLOAD_DEP=$(SRC_TEST_WORKLOAD) $(HDR_TEST_WORKLOAD) $(WORKLOAD_WORKLOAD_DEP)
TEST_WAL_DEP=$(SRC_TEST_WAL) $(HDR_TEST_WAL) $(WAL_WAL_DEP)
TEST_CMD_PARSER_DEP=$(SRC_TEST_CMD_PARSER) $(HDR_TEST_CMD_PARSER) $(CMD_PARSER_CMD_PARSER_DEP)
TEST_RDMA_DEP=$(SRC_TEST_RDMA) $(HDR_TEST_RDMA) $(RDMA_RDMA_DEP) $(COLORING_COLORING_DEP) $(CMD_PARSER_CMD_PARSER_DEP) $(MISC_MISC_DEP)
TEST_KV_PAIR_DEP=$(SRC_TEST_KV_PAIR) $(HDR_TEST_KV_PAIR) $(KV_PAIR_KV_PAIR_DEP)
TEST_STORE_DEP=$(SRC_TEST_STORE) $(HDR_TEST_STORE) $(STORE_STORE_DEP) $(CMD_PARSER_CMD_PARSER_DEP)
TEST_ERPC_DEP=$(SRC_TEST_ERPC) $(HDR_TEST_ERPC) $(CMD_PARSER_CMD_PARSER_DEP) $(RPC_WRAPPER_RPC_WRAPPER_DEP)
TEST_ENGINE_DEP=$(SRC_TEST_ENGINE) $(HDR_TEST_ENGINE) $(ENGINE_ENGINE_DEP) $(CMD_PARSER_CMD_PARSER_DEP)
TEST_CLUSTER_DEP=$(SRC_TEST_CLUSTER) $(HDR_TEST_CLUSTER) $(CLUSTER_CLUSTER_DEP) $(CMD_PARSER_CMD_PARSER_DEP)
TEST_STRING_DEP=$(SRC_TEST_STRING) $(HDR_TEST_STRING) $(KV_PAIR_KV_PAIR_DEP)
TEST_INDEXING_DEP=$(SRC_TEST_INDEXING) $(HDR_TEST_INDEXING) $(INDEXING_INDEXING_DEP)
TEST_MISC_DEP=$(SRC_TEST_MISC) $(HDR_TEST_MISC) $(MISC_MISC_DEP) $(CMD_PARSER_CMD_PARSER_DEP)
TEST_REMOTE_POINTER_DEP=$(SRC_TEST_REMOTE_POINTER) $(HDR_TEST_REMOTE_POINTER) $(REMOTE_MEMORY_REMOTE_MEMORY_DEP)

out: $(OUT_OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OUT_OBJS) $(LDFLAGS) $(LDLIBS)

test: $(TESTS)

all: out test

$(OBJ_HILL): $(HILL_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_HILL)

$(OBJ_MAIN): $(MAIN_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_MAIN)

$(OBJ_INDEXING_INDEXING): $(INDEXING_INDEXING_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_INDEXING_INDEXING)

$(OBJ_COLORING_COLORING): $(COLORING_COLORING_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_COLORING_COLORING)

$(OBJ_RPC_WRAPPER_RPC_WRAPPER): $(RPC_WRAPPER_RPC_WRAPPER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_RPC_WRAPPER_RPC_WRAPPER)

$(OBJ_KV_PAIR_KV_PAIR): $(KV_PAIR_KV_PAIR_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_KV_PAIR_KV_PAIR)

$(OBJ_TESTS_TESTS): $(TESTS_TESTS_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TESTS_TESTS)

$(OBJ_WAL_WAL): $(WAL_WAL_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_WAL_WAL)

$(OBJ_CMD_PARSER_CMD_PARSER): $(CMD_PARSER_CMD_PARSER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_CMD_PARSER_CMD_PARSER)

$(OBJ_REMOTE_MEMORY_REMOTE_MEMORY): $(REMOTE_MEMORY_REMOTE_MEMORY_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_REMOTE_MEMORY_REMOTE_MEMORY)

$(OBJ_RDMA_RDMA): $(RDMA_RDMA_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_RDMA_RDMA)

$(OBJ_CLUSTER_CLUSTER): $(CLUSTER_CLUSTER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_CLUSTER_CLUSTER)

$(OBJ_MEMORY_MANAGER_MEMORY_MANAGER): $(MEMORY_MANAGER_MEMORY_MANAGER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_MEMORY_MANAGER_MEMORY_MANAGER)

$(OBJ_CONFIG_CONFIG): $(CONFIG_CONFIG_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_CONFIG_CONFIG)

$(OBJ_STORE_STORE): $(STORE_STORE_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_STORE_STORE)

$(OBJ_READ_CACHE_READ_CACHE): $(READ_CACHE_READ_CACHE_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_READ_CACHE_READ_CACHE)

$(OBJ_ENGINE_ENGINE): $(ENGINE_ENGINE_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_ENGINE_ENGINE)

$(OBJ_WORKLOAD_WORKLOAD): $(WORKLOAD_WORKLOAD_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_WORKLOAD_WORKLOAD)

$(OBJ_MISC_MISC): $(MISC_MISC_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_MISC_MISC)

$(OBJ_TEST_CACHE): $(TEST_CACHE_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_CACHE)

$(OBJ_TEST_MEMORY_MANAGER): $(TEST_MEMORY_MANAGER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_MEMORY_MANAGER)

$(OBJ_TEST_POLYMORPHIC_POINTER): $(TEST_POLYMORPHIC_POINTER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_POLYMORPHIC_POINTER)

$(OBJ_TEST_COLORING): $(TEST_COLORING_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_COLORING)

$(OBJ_TEST_WORKLOAD): $(TEST_WORKLOAD_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_WORKLOAD)

$(OBJ_TEST_WAL): $(TEST_WAL_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_WAL)

$(OBJ_TEST_CMD_PARSER): $(TEST_CMD_PARSER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_CMD_PARSER)

$(OBJ_TEST_RDMA): $(TEST_RDMA_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_RDMA)

$(OBJ_TEST_KV_PAIR): $(TEST_KV_PAIR_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_KV_PAIR)

$(OBJ_TEST_STORE): $(TEST_STORE_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_STORE)

$(OBJ_TEST_ERPC): $(TEST_ERPC_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_ERPC)

$(OBJ_TEST_ENGINE): $(TEST_ENGINE_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_ENGINE)

$(OBJ_TEST_CLUSTER): $(TEST_CLUSTER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_CLUSTER)

$(OBJ_TEST_STRING): $(TEST_STRING_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_STRING)

$(OBJ_TEST_INDEXING): $(TEST_INDEXING_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_INDEXING)

$(OBJ_TEST_MISC): $(TEST_MISC_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_MISC)

$(OBJ_TEST_REMOTE_POINTER): $(TEST_REMOTE_POINTER_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_REMOTE_POINTER)

$(TEST_CACHE): $(OBJ_TEST_CACHE) $(OBJ_CMD_PARSER_CMD_PARSER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_MEMORY_MANAGER): $(OBJ_TEST_MEMORY_MANAGER) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_POLYMORPHIC_POINTER): $(OBJ_TEST_POLYMORPHIC_POINTER) $(OBJ_REMOTE_MEMORY_REMOTE_MEMORY) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_RDMA_RDMA) $(OBJ_MISC_MISC) $(OBJ_CLUSTER_CLUSTER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_COLORING): $(OBJ_TEST_COLORING) $(OBJ_COLORING_COLORING)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_WORKLOAD): $(OBJ_TEST_WORKLOAD) $(OBJ_WORKLOAD_WORKLOAD)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_WAL): $(OBJ_TEST_WAL) $(OBJ_WAL_WAL) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_CMD_PARSER): $(OBJ_TEST_CMD_PARSER) $(OBJ_CMD_PARSER_CMD_PARSER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_RDMA): $(OBJ_TEST_RDMA) $(OBJ_RDMA_RDMA) $(OBJ_COLORING_COLORING) $(OBJ_CMD_PARSER_CMD_PARSER) $(OBJ_MISC_MISC) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_KV_PAIR): $(OBJ_TEST_KV_PAIR) $(OBJ_KV_PAIR_KV_PAIR) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_STORE): $(OBJ_TEST_STORE) $(OBJ_STORE_STORE) $(OBJ_CMD_PARSER_CMD_PARSER) $(OBJ_INDEXING_INDEXING) $(OBJ_REMOTE_MEMORY_REMOTE_MEMORY) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_READ_CACHE_READ_CACHE) $(OBJ_ENGINE_ENGINE) $(OBJ_RPC_WRAPPER_RPC_WRAPPER) $(OBJ_KV_PAIR_KV_PAIR) $(OBJ_WORKLOAD_WORKLOAD) $(OBJ_CONFIG_CONFIG) $(OBJ_WAL_WAL) $(OBJ_MISC_MISC) $(OBJ_COLORING_COLORING) $(OBJ_RDMA_RDMA) $(OBJ_CLUSTER_CLUSTER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_ERPC): $(OBJ_TEST_ERPC) $(OBJ_CMD_PARSER_CMD_PARSER) $(OBJ_RPC_WRAPPER_RPC_WRAPPER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_ENGINE): $(OBJ_TEST_ENGINE) $(OBJ_ENGINE_ENGINE) $(OBJ_CMD_PARSER_CMD_PARSER) $(OBJ_WAL_WAL) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_REMOTE_MEMORY_REMOTE_MEMORY) $(OBJ_CLUSTER_CLUSTER) $(OBJ_RDMA_RDMA) $(OBJ_MISC_MISC) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_CLUSTER): $(OBJ_TEST_CLUSTER) $(OBJ_CLUSTER_CLUSTER) $(OBJ_MISC_MISC) $(OBJ_CMD_PARSER_CMD_PARSER) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_STRING): $(OBJ_TEST_STRING) $(OBJ_KV_PAIR_KV_PAIR) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_INDEXING): $(OBJ_TEST_INDEXING) $(OBJ_INDEXING_INDEXING) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_REMOTE_MEMORY_REMOTE_MEMORY) $(OBJ_WAL_WAL) $(OBJ_KV_PAIR_KV_PAIR) $(OBJ_MISC_MISC) $(OBJ_COLORING_COLORING) $(OBJ_CONFIG_CONFIG) $(OBJ_RDMA_RDMA) $(OBJ_CLUSTER_CLUSTER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_MISC): $(OBJ_TEST_MISC) $(OBJ_MISC_MISC) $(OBJ_CMD_PARSER_CMD_PARSER)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TEST_REMOTE_POINTER): $(OBJ_TEST_REMOTE_POINTER) $(OBJ_REMOTE_MEMORY_REMOTE_MEMORY) $(OBJ_MEMORY_MANAGER_MEMORY_MANAGER) $(OBJ_RDMA_RDMA) $(OBJ_MISC_MISC) $(OBJ_CLUSTER_CLUSTER) $(OBJ_CONFIG_CONFIG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)


.PHONY: clean
clean: 
	rm ./target/*
	rm ./obj/*.o
