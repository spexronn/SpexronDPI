#pragma once

#include <cstdint>
#include <span>

namespace SpexronDPI::Utils {

class Checksum {
public:

    static uint16_t Calculate(std::span<const uint8_t> data);

    static uint16_t CalculateTcpUdp(uint32_t srcIp, uint32_t dstIp, uint8_t protocol, std::span<const uint8_t> tcpUdpData);
};

} 
