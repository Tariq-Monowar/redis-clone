#include "miniredis/kv_store.h"
#include "miniredis/server.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    int port = 6380;
    if (argc >= 2) {
        port = std::atoi(argv[1]);
    }

    KVStore store;
    std::cout << "MiniRedis server listening on port " << port << "\n"
              << "  Client: miniredis-cli -p " << port << "\n";
    run_server(port, store);
    return 0;
}
