#pragma once

#include "connection.h"

#include <cstdint>
#include <string>
#include <vector>

namespace netdisk {

constexpr std::uint32_t kMaxFrameSize = 4U * 1024U * 1024U;
constexpr std::uint32_t kFileChunkSize = 64U * 1024U;

bool sendFrame(const Connection &connection, const void *data, std::uint32_t length,
               std::string &error);
bool sendFrame(const Connection &connection, const std::string &text,
               std::string &error);
bool sendFrame(const Connection &connection, const std::vector<unsigned char> &data,
               std::string &error);
bool receiveFrame(const Connection &connection, std::vector<unsigned char> &data,
                  std::string &error, std::uint32_t maxSize = kMaxFrameSize);
bool receiveText(const Connection &connection, std::string &text,
                 std::string &error, std::uint32_t maxSize = kMaxFrameSize);
bool sendUInt64(const Connection &connection, std::uint64_t value,
                std::string &error);
bool receiveUInt64(const Connection &connection, std::uint64_t &value,
                   std::string &error);

} // namespace netdisk
