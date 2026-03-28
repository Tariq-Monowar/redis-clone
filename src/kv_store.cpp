#include "miniredis/kv_store.h"

#include <algorithm>
#include <fnmatch.h>

void KVStore::set(std::string key, std::string value) {
    std::lock_guard<std::mutex> lock(mu_);
    data_[std::move(key)] = std::move(value);
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

size_t KVStore::del(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lock(mu_);
    size_t removed = 0;
    for (const auto& k : keys) {
        if (data_.erase(k) > 0) {
            ++removed;
        }
    }
    return removed;
}

bool KVStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    return data_.find(key) != data_.end();
}

void KVStore::flush() {
    std::lock_guard<std::mutex> lock(mu_);
    data_.clear();
}

std::vector<std::string> KVStore::keys(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> out;
    for (const auto& kv : data_) {
        if (fnmatch(pattern.c_str(), kv.first.c_str(), 0) == 0) {
            out.push_back(kv.first);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
