#pragma once

#include <cstddef>
#include <string>

namespace netdisk {

bool randomHex(std::size_t byteCount, std::string &hex, std::string &error);
bool randomToken(std::string &token, std::string &error);
bool hashPassword(const std::string &password, std::string &encoded,
                  std::string &error);
bool verifyPassword(const std::string &password, const std::string &encoded,
                    bool &matched, std::string &error);
bool sha256File(const std::string &path, std::string &hex, std::string &error);

} // namespace netdisk
