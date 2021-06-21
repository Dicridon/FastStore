CXX=clang++
CXXFLAGS=-O2 -g -std=c++17 -I./src/components
LDFLAGS=
LDLIBS=

TARGET_DIR=./target
TARGET=$(TARGET_DIR)/hill
OBJ_DIR=./obj
SRC_DIR=./src
COMPONENTS_DIR=./src/components

SRC_MAIN=./src/main.cpp
SRC_TESTS_TESTS=./src/components/tests/tests.cpp
SRC_RDMA_RDMA=./src/components/rdma/rdma.cpp
SRC_TEST_RDMA=./tests/test_rdma.cpp

HDR_TESTS_TESTS=./src/components/tests/tests.hpp
HDR_RDMA_RDMA=./src/components/rdma/rdma.hpp

OBJ_MAIN=./obj/main.o
OBJ_TESTS_TESTS=./obj/tests_tests.o
OBJ_RDMA_RDMA=./obj/rdma_rdma.o
OBJ_TEST_RDMA=./obj/test_rdma.o

OUT_OBJS=$(OBJ_MAIN) $(OBJ_RDMA_RDMA)
TEST_OBJS=$(OBJ_TESTS_TESTS) $(OBJ_TEST_RDMA)

TEST_RDMA=./target/test_rdma
TESTS=$(TEST_RDMA)

MAIN_DEP=$(SRC_MAIN)
TESTS_TESTS_DEP=$(SRC_TESTS_TESTS) $(HDR_TESTS_TESTS)
RDMA_RDMA_DEP=$(SRC_RDMA_RDMA) $(HDR_RDMA_RDMA)
TEST_RDMA_DEP=$(SRC_TEST_RDMA) $(HDR_TEST_RDMA) $(RDMA_RDMA_DEP)

out: $(OUT_OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OUT_OBJS) $(LDFLAGS) $(LDLIBS)

test: $(TESTS)

all: out test

$(OBJ_MAIN): $(MAIN_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_MAIN)

$(OBJ_TESTS_TESTS): $(TESTS_TESTS_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TESTS_TESTS)

$(OBJ_RDMA_RDMA): $(RDMA_RDMA_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_RDMA_RDMA)

$(OBJ_TEST_RDMA): $(TEST_RDMA_DEP)
	$(CXX) $(CXXFLAGS) -o $@ -c $(SRC_TEST_RDMA)

$(TEST_RDMA): $(OBJ_TEST_RDMA) $(OBJ_RDMA_RDMA)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)


.PHONY: clean
clean: 
	rm ./target/*
	rm ./obj/*.o
