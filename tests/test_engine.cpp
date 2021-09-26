#include "engine/engine.hpp"
using namespace Hill;
using namespace Hill::Memory::TypeAliases;
int main() {
    auto buf = new byte_t[1024 * 1024 * 1024];
    auto engine = Hill::Engine::make_engine(buf, "node1.info");
    engine->dump();
}
