# FastStore: A High-Performance RDMA-enabled Distributed Key-Value Store with Persistent Memory
This is the source code of paper **FastStore: A High-Performance RDMA-enabled Distributed Key-Value Store with Persistent Memory** in ICDCS'23


## Compilation
Compiling this project can be a little annoying due to external dependencies. Make sure `libpmem`, `libnuma`, `libibverbs` are installed (Linux package manager `apt` or `yum` would be sufficient).

`C++17` support is needed for `std::optional`, `std::unique_ptr` and `std::shared_ptr`.

As for `eRPC`, if you can't compile it, please refer to its issues on github. Some compiling erros I've encountered  are as follows. Here directory `.` means the root directory of `eRPC`

0. Remember to add `./src` and `./thrid_party/asio/include` to your include path
1. "config.h": no such file or directory. If you run `cmake` in `./build`, then copy `./build/src/config.h` to `./src`.
2. "<asio/ts/include.hpp>": no such file or directory. Make sure that `./third_party/asio/include` is added to the include path.
3. "numa\_node\_of\_cpu": undefined referece. Make sure that `-lnuma` is added **AFTER** `-lerpc` on Linux platforms

For `boost`, `hill` currently uses `boost 1.77.0`. Please download `boost_1_77_0.zip` to `third-party` and unzip it to compile.

The `Makefile` can be used for building. If you encounter any issue/warning/error, please refer to building tool [canoe](https://github.com/Dicridon/canoe "canoe, cargo for C++").

A `compile_commands.json` file is already included in this repo. Any text editors (Emacs/Vim/VSCode) or IDE that uses `compile_commands.json` for LSP utility should work well with it.

# Run
Config files are needed for clients, servers and monitor. The config file template are in `./bench_config`.

Files with prefix `local_clients` are client configuration. In it, monitor address and RDMA devices should be specified. Server side eRPC address should also be specified.

Files with prefix `local_node` are server configurations. What should be specified are all listed in the template. The eRPC listen port is for dynamically establishing eRPC connection. Please choose a port different from the connection ports used by eRPC.

The `local_config.moni` is the monitor's configuration file. Monitor address, cluster node number and which key range is assigned to which node should be specified.

To run, first launch a monitor to gather/scatter infomation about all nodes, then launch servers and clients.

Launch monitor `./target/test_store -t monitor -c ./bench_config/config.moni`
Launch server with 2 threads `./target/test_store -t server -c ./bench_config/node1.info -m 2`
Launch client with 2 threads running YCSB C workload `./target/test_store -t client -c ./bench_config/node1.info -m 2 -y c`
