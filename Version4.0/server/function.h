#pragma once

#include <cstdint>
#include <string>

int tcpInit(const std::string &bindIp, std::uint16_t port, std::string &error);
