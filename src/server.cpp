#include "miniredis/server.h"

#include "miniredis/kv_store.h"
#include "miniredis/resp.h"

#include <arpa/inet.h>
#include <cctype>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

std::string to_upper(std::string_view s) {
    std::string o;
    o.reserve(s.size());
    for (unsigned char c : s) {
        o += static_cast<char>(std::toupper(c));
    }
    return o;
}

std::string dispatch(const std::vector<std::string>& cmd, KVStore& store, bool& close_conn) {
    close_conn = false;
    if (cmd.empty()) {
        return resp_error("empty command");
    }
    const std::string op = to_upper(cmd[0]);

    if (op == "PING") {
        if (cmd.size() >= 2) {
            return resp_bulk(cmd[1]);
        }
        return resp_simple("PONG");
    }
    if (op == "ECHO") {
        if (cmd.size() < 2) {
            return resp_error("wrong number of arguments for 'echo' command");
        }
        return resp_bulk(cmd[1]);
    }
    if (op == "SET") {
        if (cmd.size() < 3) {
            return resp_error("wrong number of arguments for 'set' command");
        }
        store.set(cmd[1], cmd[2]);
        return resp_simple("OK");
    }
    if (op == "GET") {
        if (cmd.size() < 2) {
            return resp_error("wrong number of arguments for 'get' command");
        }
        const auto v = store.get(cmd[1]);
        if (!v) {
            return resp_null_bulk();
        }
        return resp_bulk(*v);
    }
    if (op == "DEL") {
        if (cmd.size() < 2) {
            return resp_error("wrong number of arguments for 'del' command");
        }
        std::vector<std::string> keys(cmd.begin() + 1, cmd.end());
        return resp_integer(static_cast<long long>(store.del(keys)));
    }
    if (op == "EXISTS") {
        if (cmd.size() < 2) {
            return resp_error("wrong number of arguments for 'exists' command");
        }
        long long count = 0;
        for (size_t i = 1; i < cmd.size(); ++i) {
            if (store.exists(cmd[i])) {
                ++count;
            }
        }
        return resp_integer(count);
    }
    if (op == "FLUSHDB" || op == "FLUSHALL") {
        store.flush();
        return resp_simple("OK");
    }
    if (op == "KEYS") {
        if (cmd.size() != 2) {
            return resp_error("wrong number of arguments for 'keys' command");
        }
        return resp_array(store.keys(cmd[1]));
    }
    if (op == "QUIT") {
        close_conn = true;
        return resp_simple("OK");
    }

    return resp_error("unknown command `" + cmd[0] + "`");
}

void handle_client(int fd, KVStore& store) {
    std::string buf;
    char tmp[4096];
    for (;;) {
        const ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n <= 0) {
            break;
        }
        buf.append(tmp, static_cast<size_t>(n));
        while (true) {
            std::vector<std::string> cmd;
            size_t consumed = 0;
            if (!resp_try_parse_command(std::string_view(buf.data(), buf.size()), consumed, cmd)) {
                break;
            }
            buf.erase(0, consumed);
            bool close_conn = false;
            const std::string reply = dispatch(cmd, store, close_conn);
            if (write(fd, reply.data(), reply.size()) < 0) {
                close(fd);
                return;
            }
            if (close_conn) {
                close(fd);
                return;
            }
        }
    }
    close(fd);
}

}  // namespace

void run_server(int port, KVStore& store) {
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "socket failed\n";
        return;
    }

    const int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind failed (port in use?)\n";
        close(listen_fd);
        return;
    }
    if (listen(listen_fd, 128) < 0) {
        std::cerr << "listen failed\n";
        close(listen_fd);
        return;
    }

    for (;;) {
        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);
        const int conn =
            accept(listen_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
        if (conn < 0) {
            continue;
        }
        std::thread(handle_client, conn, std::ref(store)).detach();
    }
}
