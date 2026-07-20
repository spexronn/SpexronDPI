#include "PacketModifier.hpp"
#include "utils/Checksum.hpp"
#include "telemetry/Logger.hpp"
#include <cstring>
#include <unordered_map>
#include <mutex>

#if defined(_MSC_VER) || defined(_WIN32)
#include <winsock2.h>
#endif

namespace SpexronDPI::Engine {

void PacketModifier::RecalculateChecksums(Core::PacketContext& ctx) {
    if (!ctx.ipv4Header) return;

    ctx.ipv4Header->checksum = 0; 

    size_t ipHeaderSize = ctx.ipv4Header->ihl * 4;
    std::span<const uint8_t> ipSpan(ctx.rawData.data(), ipHeaderSize);
    ctx.ipv4Header->checksum = Utils::Checksum::Calculate(ipSpan);

    if (ctx.tcpHeader) {
        ctx.tcpHeader->checksum = 0;

        uint8_t* tcpStart = reinterpret_cast<uint8_t*>(ctx.tcpHeader);
        size_t tcpSize = ctx.packetLength - ipHeaderSize;
        std::span<const uint8_t> tcpSpan(tcpStart, tcpSize);

        ctx.tcpHeader->checksum = Utils::Checksum::CalculateTcpUdp(
            ctx.ipv4Header->srcIp, 
            ctx.ipv4Header->dstIp, 
            ctx.ipv4Header->protocol, 
            tcpSpan
        );
    }
    else if (ctx.udpHeader) {
        ctx.udpHeader->checksum = 0;

        uint8_t* udpStart = reinterpret_cast<uint8_t*>(ctx.udpHeader);
        size_t udpSize = ctx.packetLength - ipHeaderSize;
        std::span<const uint8_t> udpSpan(udpStart, udpSize);

        ctx.udpHeader->checksum = Utils::Checksum::CalculateTcpUdp(
            ctx.ipv4Header->srcIp, 
            ctx.ipv4Header->dstIp, 
            ctx.ipv4Header->protocol, 
            udpSpan
        );
    }

    ctx.isModified = false; 
}

void PacketModifier::ModifyTTL(Core::PacketContext& ctx, uint8_t newTtl) {
    if (ctx.ipv4Header && ctx.ipv4Header->ttl != newTtl) {
        ctx.ipv4Header->ttl = newTtl;
        ctx.isModified = true; 
    }
}

void PacketModifier::SplitTcpSegment(Core::PacketContext& ctx) {
    if (!ctx.tcpHeader || ctx.payload.size() <= 1) return;
    if (!ctx.ipv4Header && !ctx.ipv6Header) return;

    // Başlık boyutlarını hesapla (IP + TCP)

    size_t ipHeaderSize = ctx.ipv4Header ? (ctx.ipv4Header->ihl * 4) : 40; 
    size_t tcpHeaderSize = ctx.tcpHeader->dataOffset * 4;
    size_t headersSize = ipHeaderSize + tcpHeaderSize;

    size_t originalPayloadSize = ctx.payload.size();

    // Payload'ı tam ortadan ikiye bölerek (SNI içeriğini parçalayarak) DPI'ın görmesini engelle
    size_t splitOffset = originalPayloadSize / 2;

    if (splitOffset == 0) return;

    size_t remainingPayloadSize = originalPayloadSize - splitOffset;

    ctx.oldSeqNum = ntohl(ctx.tcpHeader->seqNum);
    ctx.oldAckNum = ntohl(ctx.tcpHeader->ackNum);
    if (ctx.ipv4Header) ctx.oldTtl = ctx.ipv4Header->ttl;

    // 2. parçayı (kalan payload) oluştur ve verileri kopyala
    std::vector<uint8_t> secondPacket(headersSize + remainingPayloadSize);

    std::memcpy(secondPacket.data(), ctx.rawData.data(), headersSize);

    std::memcpy(secondPacket.data() + headersSize, ctx.payload.data() + splitOffset, remainingPayloadSize);

    // 2. parçanın Sequence numarasını ilk parçanın boyutu kadar (splitOffset) ileri al
    auto* tcp2 = reinterpret_cast<Parsers::TcpHeader*>(secondPacket.data() + ipHeaderSize);
    uint32_t newSeq = ctx.oldSeqNum + splitOffset; 
    tcp2->seqNum = htonl(newSeq);

    ctx.newSeqNum = newSeq;
    ctx.newAckNum = ctx.oldAckNum; 
    if (ctx.ipv4Header) ctx.newTtl = ctx.oldTtl; 

    if (ctx.ipv4Header) {
        auto* ip2 = reinterpret_cast<Parsers::IPv4Header*>(secondPacket.data());
        ip2->totalLength = htons(headersSize + remainingPayloadSize);
        ip2->id = htons(ntohs(ctx.ipv4Header->id) + 1); 
    } else {
        // IPv6 boyutunu güncelle

        uint16_t ipv6PayloadLen2 = tcpHeaderSize + remainingPayloadSize;
        secondPacket[4] = (ipv6PayloadLen2 >> 8) & 0xFF;
        secondPacket[5] = ipv6PayloadLen2 & 0xFF;
    }

    tcp2->checksum = 0;

    ctx.injectedPackets.push_back(secondPacket);

    // 1. parçayı (orijinal paketi) splitOffset kadar kalacak şekilde güncelle
    ctx.rawData.resize(headersSize + splitOffset);
    ctx.packetLength = headersSize + splitOffset;

    if (ctx.ipv4Header) {
        ctx.ipv4Header->totalLength = htons(headersSize + splitOffset);
    } else {
        uint16_t ipv6PayloadLen1 = tcpHeaderSize + splitOffset;
        ctx.rawData[4] = (ipv6PayloadLen1 >> 8) & 0xFF;
        ctx.rawData[5] = ipv6PayloadLen1 & 0xFF;
    }

    ctx.payload = std::span<uint8_t>(ctx.rawData.data() + headersSize, splitOffset);

    ctx.isModified = true;

    if (ctx.isTargetSni) {
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "Original Size: " + std::to_string(originalPayloadSize) + " bytes payload"));
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "Split Offset: " + std::to_string(splitOffset)));
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "First Packet: " + std::to_string(splitOffset) + " bytes payload"));
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "Second Packet: " + std::to_string(remainingPayloadSize) + " bytes payload"));
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "SeqNum değişti: " + std::to_string(ctx.oldSeqNum) + " -> " + std::to_string(ctx.newSeqNum)));
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "Sahte paket oluşturuldu (İkinci Segment)"));
    }

    LOG_TRACE("TCP Segmentasyon başarılı: Orijinal paket {} byte payload'a küçültüldü, ikinci paket (enjekte edilecek) {} byte payload içeriyor.", splitOffset, remainingPayloadSize);
}

const std::vector<uint8_t> PacketModifier::FAKE_CLIENTHELLO_PAYLOAD = {

    0x00, 0x17, 0x00, 0x00,
    0xFF, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x0A, 0x00, 0x0E,
    0x00, 0x0C, 0x00, 0x1D, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x01, 0x00, 0x01, 0x01,
    0x00, 0x0B, 0x00, 0x02, 0x01, 0x00,
    0x00, 0x23, 0x00, 0x00,
    0x00, 0x10, 0x00, 0x0E,
    0x00, 0x0C, 0x02, 0x68, 0x32, 0x08, 0x68, 0x74, 0x74, 0x70, 0x2F, 0x31, 0x2E, 0x31,
    0x00, 0x05, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x22, 0x00, 0x0A, 0x00, 0x08, 0x04, 0x03, 0x05, 0x03, 0x06, 0x03, 0x02, 0x03,
    0x00, 0x33, 0x00, 0x6B, 0x00, 0x69, 0x00, 0x1D, 0x00, 0x20,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0x00, 0x17, 0x00, 0x41, 0x04,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0x00, 0x2B, 0x00, 0x05, 0x04, 0x03, 0x04, 0x03, 0x03,
    0x00, 0x0D, 0x00, 0x18, 0x00, 0x16, 0x04, 0x03, 0x05, 0x03, 0x06, 0x03, 0x08, 0x04, 0x08, 0x05,
    0x08, 0x06, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01, 0x02, 0x03, 0x02, 0x01,
    0x00, 0x2D, 0x00, 0x02, 0x01, 0x01,
    0x00, 0x1C, 0x00, 0x02, 0x40, 0x01,
    0xFE, 0x0D, 0x01, 0x19, 0x00, 0x00, 0x01, 0x00, 0x01, 0xAA, 0x00, 0x20,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0x00, 0xEF,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
};

void PacketModifier::InjectFakeClientHello(Core::PacketContext& ctx) {
    if (!ctx.ipv4Header || !ctx.tcpHeader) return;

    size_t ipHeaderSize = ctx.ipv4Header->ihl * 4;
    size_t tcpHeaderSize = ctx.tcpHeader->dataOffset * 4;
    size_t headersSize = ipHeaderSize + tcpHeaderSize;

    std::vector<uint8_t> fakePacket(headersSize + FAKE_CLIENTHELLO_PAYLOAD.size());
    std::memcpy(fakePacket.data(), ctx.rawData.data(), headersSize);
    std::memcpy(fakePacket.data() + headersSize, FAKE_CLIENTHELLO_PAYLOAD.data(), FAKE_CLIENTHELLO_PAYLOAD.size());

    Parsers::IPv4Header* fakeIp = reinterpret_cast<Parsers::IPv4Header*>(fakePacket.data());
    Parsers::TcpHeader* fakeTcp = reinterpret_cast<Parsers::TcpHeader*>(fakePacket.data() + ipHeaderSize);

    fakeIp->totalLength = htons(static_cast<uint16_t>(fakePacket.size()));

    // DPI'ı yanıltmak için çok düşük bir TTL (örn: 8) ayarla.
    // Böylece bu sahte paket ISP'nin DPI cihazına ulaşır ama gerçek sunucuya (hedefe) varmadan ölür.
    fakeIp->ttl = 8;

    // Sequence drift (SEQ - 10000) kullanmak yerine, orijinal paketle TAMAMEN AYNI SEQ/ACK kullanıyoruz.
    // DPI cihazı bu sequence numarasını görünce, "Tamam, sahte SNI ile bağlantı kuruldu" der.
    // Ardından gelen GERÇEK paketleri "Retransmission" veya "Zaten izin verilmiş bağlantı" sanıp atlar!
    // fakeTcp->ackNum = ...
    // fakeTcp->seqNum = ...

    ctx.injectedPackets.push_back(fakePacket);

    LOG_INFO("{}", ctx.GetTraceLog("DPI Desync", "Fake ClientHello", "523 Byte sahte TLS ClientHello, TTL=8 ve orijinal SEQ/ACK ile enjekte edildi."));
}

static std::unordered_map<uint16_t, uint32_t> dnsNatTable; 
static std::mutex dnsNatMutex;

void PacketModifier::RedirectDns(Core::PacketContext& ctx) {
    if (!ctx.ipv4Header || !ctx.udpHeader) return;

    if (ntohs(ctx.udpHeader->dstPort) != 53) return;

    uint32_t originalDstIp = ctx.ipv4Header->dstIp;
    uint16_t srcPort = ntohs(ctx.udpHeader->srcPort);

    {
        std::lock_guard<std::mutex> lock(dnsNatMutex);
        dnsNatTable[srcPort] = originalDstIp;
    }

    ctx.ipv4Header->dstIp = inet_addr("94.140.14.14"); 
    ctx.udpHeader->dstPort = htons(5353);

    ctx.isModified = true;
    LOG_DEBUG("{}", ctx.GetTraceLog("DNS NAT", "N/A", "DNS isteği AdGuard DNS'e (94.140.14.14:5353) yönlendirildi"));
}

void PacketModifier::RestoreDns(Core::PacketContext& ctx) {
    if (!ctx.ipv4Header || !ctx.udpHeader) return;

    if (ntohs(ctx.udpHeader->dstPort) != 5353 && ntohs(ctx.udpHeader->srcPort) != 5353) return;

    uint16_t dstPort = ntohs(ctx.udpHeader->dstPort);
    uint32_t originalSrcIp = 0;

    {
        std::lock_guard<std::mutex> lock(dnsNatMutex);
        auto it = dnsNatTable.find(dstPort);
        if (it != dnsNatTable.end()) {
            originalSrcIp = it->second;

        }
    }

    if (originalSrcIp != 0) {
        ctx.ipv4Header->srcIp = originalSrcIp; 
        ctx.udpHeader->srcPort = htons(53);    

        ctx.isModified = true;
        LOG_DEBUG("{}", ctx.GetTraceLog("DNS NAT", "N/A", "AdGuard DNS'ten gelen DNS yanıtı, orijinal IP'ye çevrildi"));
    }
}

} 
