#pragma once

#include <vector>
#include <span>
#include <string>
#include <cstdio>
#include <cstring>
#include <atomic>
#include "parsers/PacketHeaders.hpp"

namespace SpexronDPI::Core {

struct PacketContext {

    std::vector<uint8_t> rawData; 

    size_t packetLength = 0;      

    Parsers::IPv4Header* ipv4Header = nullptr;

    void* ipv6Header = nullptr;
    Parsers::TcpHeader* tcpHeader = nullptr;
    Parsers::UdpHeader* udpHeader = nullptr;

    std::span<uint8_t> payload;

    std::vector<uint8_t> divertAddress;

    bool isOutbound = true;  

    bool isModified = false;

    bool shouldDrop = false;

    std::vector<std::vector<uint8_t>> injectedPackets;

    inline static std::atomic<uint64_t> GlobalPacketIdCounter{1};
    uint64_t packetId = 0;
    uint64_t parentPacketId = 0; 

    bool isImpostor = false; 
    bool isTargetSni = false; 
    uint32_t oldSeqNum = 0, newSeqNum = 0;
    uint32_t oldAckNum = 0, newAckNum = 0;
    uint8_t oldTtl = 0, newTtl = 0;

    const char* dropFile = nullptr;
    const char* dropFunc = nullptr;
    int dropLine = 0;
    const char* dropCondition = nullptr;

    inline std::string GetTraceLog(const std::string& action, const std::string& rule = "N/A", const std::string& result = "N/A") const {
        char buf[512];
        uint8_t src[4] = {0};
        uint8_t dst[4] = {0};

        if (ipv4Header) {
            std::memcpy(src, &ipv4Header->srcIp, 4);
            std::memcpy(dst, &ipv4Header->dstIp, 4);
        }

        const char* protoStr = "UNKNOWN";
        uint16_t sPort = 0;
        uint16_t dPort = 0;

        if (tcpHeader) {
            protoStr = "TCP";
            sPort = (reinterpret_cast<const uint8_t*>(&tcpHeader->srcPort)[0] << 8) | reinterpret_cast<const uint8_t*>(&tcpHeader->srcPort)[1];
            dPort = (reinterpret_cast<const uint8_t*>(&tcpHeader->dstPort)[0] << 8) | reinterpret_cast<const uint8_t*>(&tcpHeader->dstPort)[1];
            if (dPort == 443 || sPort == 443) protoStr = "TCP (HTTPS)";
        } else if (udpHeader) {
            protoStr = "UDP";
            sPort = (reinterpret_cast<const uint8_t*>(&udpHeader->srcPort)[0] << 8) | reinterpret_cast<const uint8_t*>(&udpHeader->srcPort)[1];
            dPort = (reinterpret_cast<const uint8_t*>(&udpHeader->dstPort)[0] << 8) | reinterpret_cast<const uint8_t*>(&udpHeader->dstPort)[1];
            if (dPort == 443 || sPort == 443) protoStr = "UDP (QUIC)";
        }

        std::string parentInfo = "";
        if (parentPacketId > 0) {
            parentInfo = " Parent:" + std::to_string(parentPacketId);
        }

        std::snprintf(buf, sizeof(buf), "[ID:%llu%s] [%s] [%s] %s | %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u | Len:%zu | Rule:%s | Result:%s",
            (unsigned long long)packetId,
            parentInfo.c_str(),
            action.c_str(),
            isOutbound ? "OUT" : "IN ",
            protoStr,
            src[0], src[1], src[2], src[3], sPort,
            dst[0], dst[1], dst[2], dst[3], dPort,
            packetLength,
            rule.c_str(),
            result.c_str()
        );
        return std::string(buf);
    }

    std::span<uint8_t> GetRawSpan() {
        return {rawData.data(), packetLength};
    }
};

} 

#define DROP_PACKET(ctx, reason) \
    do { \
        (ctx).shouldDrop = true; \
        (ctx).dropFile = __FILE__; \
        (ctx).dropFunc = __func__; \
        (ctx).dropLine = __LINE__; \
        (ctx).dropCondition = (reason); \
    } while (0)
