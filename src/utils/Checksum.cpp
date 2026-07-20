#include "Checksum.hpp"

#if defined(_MSC_VER) || defined(_WIN32)
#include <winsock2.h> 
#endif

namespace SpexronDPI::Utils {

uint16_t Checksum::Calculate(std::span<const uint8_t> data) {
    uint32_t sum = 0;
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data.data());
    size_t length = data.size();

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length == 1) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

uint16_t Checksum::CalculateTcpUdp(uint32_t srcIp, uint32_t dstIp, uint8_t protocol, std::span<const uint8_t> tcpUdpData) {
    uint32_t sum = 0;

    sum += (srcIp & 0xFFFF) + (srcIp >> 16);
    sum += (dstIp & 0xFFFF) + (dstIp >> 16);
    sum += htons(static_cast<uint16_t>(protocol)); 
    sum += htons(static_cast<uint16_t>(tcpUdpData.size()));

    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(tcpUdpData.data());
    size_t length = tcpUdpData.size();

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length == 1) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

} 
