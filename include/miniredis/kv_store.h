#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class KVStore {
    std::unordered_map<std::string, std::string> data_;
    mutable std::mutex mu_;

public:
    void set(std::string key, std::string value);
    std::optional<std::string> get(const std::string& key);
    size_t del(const std::vector<std::string>& keys);
    bool exists(const std::string& key);
    void flush();
    std::vector<std::string> keys(const std::string& pattern);
};
