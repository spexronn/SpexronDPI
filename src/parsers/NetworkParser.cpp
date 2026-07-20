#include "NetworkParser.hpp"
#include "telemetry/Logger.hpp"

#if defined(_MSC_VER) || defined(_WIN32)
#include <winsock2.h>
#endif

namespace SpexronDPI::Parsers {

bool NetworkParser::Parse(Core::PacketContext& ctx) {

    if (ctx.packetLength < sizeof(IPv4Header)) {
        LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (Too small for IP)"));
        return false; 
    }

    uint8_t* rawPtr = ctx.rawData.data();

    uint8_t version = (rawPtr[0] >> 4) & 0x0F;

    if (version == 4) { 
        ctx.ipv4Header = reinterpret_cast<IPv4Header*>(rawPtr);

        size_t ipHeaderSize = ctx.ipv4Header->ihl * 4;

        if (ctx.packetLength < ipHeaderSize) {
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (Packet smaller than IP header)"));
            return false;
        }

        uint16_t totalLength = ntohs(ctx.ipv4Header->totalLength);
        if (totalLength < ipHeaderSize) {
            LOG_DEBUG("IPv4 Underflow tespiti: totalLength ({}) < ipHeaderSize ({})", totalLength, ipHeaderSize);
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv4 Underflow)"));
            return false;
        }
        if (ctx.packetLength < totalLength) {
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (Packet truncated)"));
            return false;
        }
        size_t actualPacketLength = totalLength;

        uint8_t protocol = ctx.ipv4Header->protocol;
        uint8_t* l4Ptr = rawPtr + ipHeaderSize;
        size_t l4RemainingSize = actualPacketLength - ipHeaderSize;

        bool isFragment = (ntohs(ctx.ipv4Header->fragOffset) & 0x1FFF) != 0;
        if (isFragment) {
            LOG_TRACE("IPv4 Fragmented paket tespit edildi, passthrough yapiliyor.");
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Pass-Through (IPv4 Fragment)"));
            return true; 
        }

        LOG_TRACE("IPv4 Paketi: protocol={}", protocol);

        if (protocol == 6) { 
            if (l4RemainingSize < sizeof(TcpHeader)) {
                LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (TCP Header too small)"));
                return false;
            }

            ctx.tcpHeader = reinterpret_cast<TcpHeader*>(l4Ptr);

            size_t tcpHeaderSize = ctx.tcpHeader->dataOffset * 4;
            if (l4RemainingSize < tcpHeaderSize) {
                LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (TCP Data Offset mismatch)"));
                return false;
            }

            uint8_t* payloadPtr = l4Ptr + tcpHeaderSize;
            size_t payloadSize = l4RemainingSize - tcpHeaderSize;

            if (payloadSize > 0) {
                ctx.payload = std::span<uint8_t>(payloadPtr, payloadSize);
            }
        } 
        else if (protocol == 17) { 
            if (l4RemainingSize < sizeof(UdpHeader)) {
                LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (UDP Header too small)"));
                return false;
            }

            ctx.udpHeader = reinterpret_cast<UdpHeader*>(l4Ptr);

            size_t udpHeaderSize = sizeof(UdpHeader);
            uint8_t* payloadPtr = l4Ptr + udpHeaderSize;
            size_t payloadSize = l4RemainingSize - udpHeaderSize;

            if (payloadSize > 0) {
                ctx.payload = std::span<uint8_t>(payloadPtr, payloadSize);
            }
        }

    } 
    else if (version == 6) {

        if (ctx.packetLength < 40) {
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv6 too small)"));
            return false;
        }

        ctx.ipv6Header = rawPtr;
        uint8_t nextHeader = rawPtr[6]; 
        size_t ipHeaderSize = 40;

        uint16_t payloadLength = (rawPtr[4] << 8) | rawPtr[5];
        size_t actualPacketLength = 40 + payloadLength;
        if (actualPacketLength < ipHeaderSize) {
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv6 Underflow)"));
            return false;
        }
        if (ctx.packetLength < actualPacketLength) {
            LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv6 Truncated)"));
            return false;
        }

        uint8_t* l4Ptr = rawPtr + ipHeaderSize;
        size_t l4RemainingSize = actualPacketLength - ipHeaderSize;

        if (nextHeader == 6) { 
            if (l4RemainingSize < 20) {
                LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv6 TCP too small)"));
                return false;
            }
            ctx.tcpHeader = reinterpret_cast<TcpHeader*>(l4Ptr);
            size_t tcpHeaderSize = ctx.tcpHeader->dataOffset * 4;
            if (l4RemainingSize < tcpHeaderSize) {
                LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv6 TCP Offset mismatch)"));
                return false;
            }

            uint8_t* payloadPtr = l4Ptr + tcpHeaderSize;
            size_t payloadSize = l4RemainingSize - tcpHeaderSize;
            if (payloadSize > 0) ctx.payload = std::span<uint8_t>(payloadPtr, payloadSize);
        } else if (nextHeader == 17) { 
            if (l4RemainingSize < 8) {
                LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (IPv6 UDP too small)"));
                return false;
            }
            ctx.udpHeader = reinterpret_cast<UdpHeader*>(l4Ptr);
            size_t udpHeaderSize = 8;
            uint8_t* payloadPtr = l4Ptr + udpHeaderSize;
            size_t payloadSize = l4RemainingSize - udpHeaderSize;
            if (payloadSize > 0) ctx.payload = std::span<uint8_t>(payloadPtr, payloadSize);
        }
    } 
    else {
        LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Drop (Unknown Version)"));
        return false; 
    }

    LOG_TRACE("{}", ctx.GetTraceLog("Parse", "N/A", "Success"));
    return true; 
}

} 
