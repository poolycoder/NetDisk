#include "config.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace netdisk {

namespace {

std::string trim(const std::string &value)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

} // namespace

bool KeyValueConfig::load(const std::string &path, std::string &error)
{
    std::ifstream input(path.c_str());
    if (!input) {
        error = "cannot open config file: " + path;
        return false;
    }

    values_.clear();
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            std::ostringstream message;
            message << "invalid config line " << lineNumber << " in " << path;
            error = message.str();
            return false;
        }

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            std::ostringstream message;
            message << "empty key/value at line " << lineNumber << " in " << path;
            error = message.str();
            return false;
        }
        values_[key] = value;
    }
    return true;
}

std::string KeyValueConfig::get(const std::string &key,
                                const std::string &fallback) const
{
    const auto iterator = values_.find(key);
    return iterator == values_.end() ? fallback : iterator->second;
}

int KeyValueConfig::getInt(const std::string &key, int fallback) const
{
    const std::string value = get(key);
    if (value.empty()) {
        return fallback;
    }

    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0') {
        return fallback;
    }
    return static_cast<int>(parsed);
}

unsigned long long KeyValueConfig::getUInt64(const std::string &key,
                                             unsigned long long fallback) const
{
    const std::string value = get(key);
    if (value.empty()) {
        return fallback;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0') {
        return fallback;
    }
    return parsed;
}

} // namespace netdisk
