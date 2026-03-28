#include "miniredis/resp.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

void print_scalar(const RespMessage& msg) {
    switch (msg.kind) {
    case RespMessage::Kind::SimpleString:
        std::cout << msg.text;
        break;
    case RespMessage::Kind::Error:
        std::cout << "(error) " << msg.text;
        break;
    case RespMessage::Kind::Integer:
        std::cout << msg.integer;
        break;
    case RespMessage::Kind::BulkString:
        std::cout << msg.text;
        break;
    case RespMessage::Kind::NullBulk:
        std::cout << "(nil)";
        break;
    case RespMessage::Kind::Array:
        break;
    }
}

void print_message(const RespMessage& msg) {
    if (msg.kind != RespMessage::Kind::Array) {
        print_scalar(msg);
        std::cout << "\n";
        return;
    }
    int idx = 1;
    for (const auto& el : msg.elements) {
        std::cout << idx << ") ";
        if (el.kind == RespMessage::Kind::BulkString) {
            std::cout << "\"" << el.text << "\"\n";
        } else if (el.kind == RespMessage::Kind::Array) {
            std::cout << "\n";
            print_message(el);
        } else {
            print_scalar(el);
            std::cout << "\n";
        }
        ++idx;
    }
}

bool split_line(std::string_view line, std::vector<std::string>& args) {
    args.clear();
    const std::string line_owned(line);
    std::istringstream iss(line_owned);
    std::string w;
    while (iss >> w) {
        args.push_back(std::move(w));
    }
    return !args.empty();
}

int connect_tcp(const char* host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    std::snprintf(port_str, sizeof(port_str), "%d", port);

    addrinfo* res = nullptr;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || res == nullptr) {
        return -1;
    }

    int fd = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        fd = static_cast<int>(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (fd < 0) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [-h host] [-p port]\n"
              << "  Default: 127.0.0.1:6380\n"
              << "  Type commands (e.g. PING, SET key val, GET key), EXIT to quit.\n";
}

}  // namespace

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 6380;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (std::strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    const int sock = connect_tcp(host, port);
    if (sock < 0) {
        std::cerr << "Could not connect to " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "miniredis-cli " << host << ":" << port << " — type EXIT to quit\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::vector<std::string> args;
        if (!split_line(line, args)) {
            continue;
        }
        if (args[0] == "EXIT" || args[0] == "exit") {
            break;
        }

        const std::string payload = resp_array(args);
        if (write(sock, payload.data(), payload.size()) < 0) {
            std::cerr << "send failed\n";
            break;
        }

        std::string buf;
        char tmp[4096];
        for (;;) {
            RespMessage msg;
            size_t consumed = 0;
            if (resp_try_parse_message(std::string_view(buf.data(), buf.size()), consumed, msg)) {
                buf.erase(0, consumed);
                print_message(msg);
                break;
            }
            const ssize_t n = read(sock, tmp, sizeof(tmp));
            if (n <= 0) {
                std::cerr << "connection closed\n";
                close(sock);
                return 1;
            }
            buf.append(tmp, static_cast<size_t>(n));
        }
    }

    close(sock);
    return 0;
}
