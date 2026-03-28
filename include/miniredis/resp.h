#pragma once

#include <string>
#include <string_view>
#include <vector>

std::string resp_simple(std::string_view s);
std::string resp_error(std::string_view msg);
std::string resp_integer(long long n);
std::string resp_bulk(std::string_view s);
std::string resp_null_bulk();
std::string resp_array(const std::vector<std::string>& items);

// Parses one RESP array of bulk strings (a typical command from the client).
bool resp_try_parse_command(std::string_view buf, size_t& consumed, std::vector<std::string>& out);

// One RESP value (simple / error / int / bulk / null bulk / nested array).
struct RespMessage {
    enum class Kind { SimpleString, Error, Integer, BulkString, NullBulk, Array } kind{};
    std::string text;
    long long integer = 0;
    std::vector<RespMessage> elements;
};

// Parses a full server reply from the start of `buf`. Returns false if more bytes are needed.
bool resp_try_parse_message(std::string_view buf, size_t& consumed, RespMessage& out);
