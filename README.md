# Hill: Combine the power of NVM and RDMA for disaggregated persistent memory key-value store


Perhaps I will need a simple object system to support recovery

Remember to test a B+ Tree on NVM and RDMA. Find out which one will be exhausted, CPU or NIC?

Can't believe this mess is working and working correctly

## Compilation
Compiling this project can be a little annoying due to external dependencies. Make sure `libpmem`, `libnuma`, `libibverbs` are installed (Linux package manager `apt` or `yum` would be sufficient). 

As for `eRPC`, if you can't compile it, please refer to its issues on github. Some compiling erros I've encountered  are as follows. Here directory `.` means the root directory of `eRPC`

0. Remember to add `./src` and `./thrid_party/asio/include` to your include path
1. "config.h": no such file or directory. If you run `cmake` in `./build`, then copy `./build/src/config.h` to `./src`.
2. "<asio/ts/include.hpp>": no such file or directory. Make sure that `./third_party/asio/include` is added to the include path.
3. "numa\_node\_of\_cpu": undefined referece. Make sure that `-lnuma` is added **AFTER** `-lerpc` on Linux platforms

For `boost`, `hill` currently uses `boost 1.77.0`. Please download `boost_1_77_0.zip` to `third-party` and unzip it to compile.
