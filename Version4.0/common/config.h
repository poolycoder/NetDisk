#pragma once

#include <string>
#include <unordered_map>

namespace netdisk {

class KeyValueConfig {
public:
    bool load(const std::string &path, std::string &error);
    std::string get(const std::string &key, const std::string &fallback = "") const;
    int getInt(const std::string &key, int fallback) const;
    unsigned long long getUInt64(const std::string &key,
                                 unsigned long long fallback) const;

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace netdisk
