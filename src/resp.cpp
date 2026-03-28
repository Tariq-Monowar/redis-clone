#include "miniredis/resp.h"

#include <cctype>

namespace {

bool read_crlf_line(std::string_view s, size_t& p, std::string_view& line) {
    if (p >= s.size()) {
        return false;
    }
    const size_t crlf = s.find("\r\n", p);
    if (crlf == std::string::npos) {
        return false;
    }
    line = s.substr(p, crlf - p);
    p = crlf + 2;
    return true;
}

bool parse_int(std::string_view sv, long long& out) {
    if (sv.empty()) {
        return false;
    }
    size_t i = 0;
    int sign = 1;
    if (sv[0] == '-') {
        sign = -1;
        ++i;
    }
    if (i >= sv.size()) {
        return false;
    }
    long long v = 0;
    for (; i < sv.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(sv[i]))) {
            return false;
        }
        v = v * 10 + (sv[i] - '0');
    }
    out = sign * v;
    return true;
}

bool parse_bulk_string(std::string_view s, size_t& p, std::string& out) {
    if (p >= s.size() || s[p] != '$') {
        return false;
    }
    ++p;
    std::string_view len_line;
    if (!read_crlf_line(s, p, len_line)) {
        return false;
    }
    long long len = 0;
    if (!parse_int(len_line, len)) {
        return false;
    }
    if (len == -1) {
        out.clear();
        return true;
    }
    if (len < 0) {
        return false;
    }
    const auto ulen = static_cast<size_t>(len);
    if (p + ulen + 2 > s.size()) {
        return false;
    }
    out.assign(s.data() + p, ulen);
    p += ulen;
    if (s[p] != '\r' || s[p + 1] != '\n') {
        return false;
    }
    p += 2;
    return true;
}

bool parse_value(std::string_view s, size_t& p, RespMessage& out) {
    const size_t start = p;
    if (p >= s.size()) {
        return false;
    }
    switch (s[p]) {
    case '+': {
        ++p;
        std::string_view line;
        if (!read_crlf_line(s, p, line)) {
            p = start;
            return false;
        }
        out.kind = RespMessage::Kind::SimpleString;
        out.text = std::string(line);
        return true;
    }
    case '-': {
        ++p;
        std::string_view line;
        if (!read_crlf_line(s, p, line)) {
            p = start;
            return false;
        }
        out.kind = RespMessage::Kind::Error;
        out.text = std::string(line);
        return true;
    }
    case ':': {
        ++p;
        std::string_view line;
        if (!read_crlf_line(s, p, line)) {
            p = start;
            return false;
        }
        long long v = 0;
        if (!parse_int(line, v)) {
            p = start;
            return false;
        }
        out.kind = RespMessage::Kind::Integer;
        out.integer = v;
        return true;
    }
    case '$': {
        ++p;
        std::string_view len_line;
        if (!read_crlf_line(s, p, len_line)) {
            p = start;
            return false;
        }
        long long len = 0;
        if (!parse_int(len_line, len)) {
            p = start;
            return false;
        }
        if (len == -1) {
            out.kind = RespMessage::Kind::NullBulk;
            return true;
        }
        if (len < 0) {
            p = start;
            return false;
        }
        const auto ulen = static_cast<size_t>(len);
        if (p + ulen + 2 > s.size()) {
            p = start;
            return false;
        }
        out.kind = RespMessage::Kind::BulkString;
        out.text.assign(s.data() + p, ulen);
        p += ulen;
        if (s[p] != '\r' || s[p + 1] != '\n') {
            p = start;
            return false;
        }
        p += 2;
        return true;
    }
    case '*': {
        ++p;
        std::string_view count_line;
        if (!read_crlf_line(s, p, count_line)) {
            p = start;
            return false;
        }
        long long n = 0;
        if (!parse_int(count_line, n) || n < 0) {
            p = start;
            return false;
        }
        out.kind = RespMessage::Kind::Array;
        out.elements.clear();
        out.elements.reserve(static_cast<size_t>(n));
        for (long long i = 0; i < n; ++i) {
            RespMessage child;
            if (!parse_value(s, p, child)) {
                p = start;
                return false;
            }
            out.elements.push_back(std::move(child));
        }
        return true;
    }
    default:
        return false;
    }
}

}  // namespace

std::string resp_simple(std::string_view s) {
    return std::string("+") + std::string(s) + "\r\n";
}

std::string resp_error(std::string_view msg) {
    return std::string("-ERR ") + std::string(msg) + "\r\n";
}

std::string resp_integer(long long n) {
    return std::string(":") + std::to_string(n) + "\r\n";
}

std::string resp_bulk(std::string_view s) {
    return std::string("$") + std::to_string(s.size()) + "\r\n" + std::string(s) + "\r\n";
}

std::string resp_null_bulk() {
    return "$-1\r\n";
}

std::string resp_array(const std::vector<std::string>& items) {
    std::string out = "*" + std::to_string(items.size()) + "\r\n";
    for (const auto& item : items) {
        out += resp_bulk(item);
    }
    return out;
}

bool resp_try_parse_command(std::string_view buf, size_t& consumed, std::vector<std::string>& out) {
    consumed = 0;
    out.clear();
    if (buf.empty() || buf[0] != '*') {
        return false;
    }
    size_t p = 1;
    std::string_view count_line;
    if (!read_crlf_line(buf, p, count_line)) {
        return false;
    }
    long long n = 0;
    if (!parse_int(count_line, n) || n < 0) {
        return false;
    }
    out.reserve(static_cast<size_t>(n));
    for (long long i = 0; i < n; ++i) {
        std::string part;
        if (!parse_bulk_string(buf, p, part)) {
            return false;
        }
        out.push_back(std::move(part));
    }
    consumed = p;
    return true;
}

bool resp_try_parse_message(std::string_view buf, size_t& consumed, RespMessage& out) {
    size_t p = 0;
    if (!parse_value(buf, p, out)) {
        consumed = 0;
        return false;
    }
    consumed = p;
    return true;
}
