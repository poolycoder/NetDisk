#include "protocol.h"

#include <arpa/inet.h>
#include <cstring>

namespace netdisk {

namespace {

std::uint64_t hostToNetwork64(std::uint64_t value)
{
    const std::uint32_t high = htonl(static_cast<std::uint32_t>(value >> 32));
    const std::uint32_t low = htonl(static_cast<std::uint32_t>(value & 0xffffffffU));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

std::uint64_t networkToHost64(std::uint64_t value)
{
    const std::uint32_t high = ntohl(static_cast<std::uint32_t>(value & 0xffffffffU));
    const std::uint32_t low = ntohl(static_cast<std::uint32_t>(value >> 32));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

} // namespace

bool sendFrame(const Connection &connection, const void *data, std::uint32_t length,
               std::string &error)
{
    if (length > kMaxFrameSize) {
        error = "frame is too large";
        return false;
    }
    const std::uint32_t networkLength = htonl(length);
    return sendAll(connection, &networkLength, sizeof(networkLength), error) &&
           (length == 0 || sendAll(connection, data, length, error));
}

bool sendFrame(const Connection &connection, const std::string &text,
               std::string &error)
{
    return sendFrame(connection, text.data(), static_cast<std::uint32_t>(text.size()),
                     error);
}

bool sendFrame(const Connection &connection, const std::vector<unsigned char> &data,
               std::string &error)
{
    return sendFrame(connection, data.empty() ? nullptr : data.data(),
                     static_cast<std::uint32_t>(data.size()), error);
}

bool receiveFrame(const Connection &connection, std::vector<unsigned char> &data,
                  std::string &error, std::uint32_t maxSize)
{
    std::uint32_t networkLength = 0;
    if (!receiveAll(connection, &networkLength, sizeof(networkLength), error)) {
        return false;
    }
    const std::uint32_t length = ntohl(networkLength);
    if (length > maxSize || length > kMaxFrameSize) {
        error = "incoming frame exceeds configured limit";
        return false;
    }
    data.assign(length, 0);
    return length == 0 || receiveAll(connection, data.data(), length, error);
}

bool receiveText(const Connection &connection, std::string &text,
                 std::string &error, std::uint32_t maxSize)
{
    std::vector<unsigned char> data;
    if (!receiveFrame(connection, data, error, maxSize)) {
        return false;
    }
    text.assign(data.begin(), data.end());
    return true;
}

bool sendUInt64(const Connection &connection, std::uint64_t value,
                std::string &error)
{
    const std::uint64_t networkValue = hostToNetwork64(value);
    return sendFrame(connection, &networkValue, sizeof(networkValue), error);
}

bool receiveUInt64(const Connection &connection, std::uint64_t &value,
                   std::string &error)
{
    std::vector<unsigned char> data;
    if (!receiveFrame(connection, data, error, sizeof(std::uint64_t)) ||
        data.size() != sizeof(std::uint64_t)) {
        error = "invalid uint64 frame";
        return false;
    }
    std::uint64_t networkValue = 0;
    std::memcpy(&networkValue, data.data(), sizeof(networkValue));
    value = networkToHost64(networkValue);
    return true;
}

} // namespace netdisk
