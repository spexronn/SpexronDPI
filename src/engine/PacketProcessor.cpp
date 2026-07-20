#include "PacketProcessor.hpp"
#include "PacketModifier.hpp"
#include "parsers/L7Parser.hpp"
#include "rules/RuleEngine.hpp"
#include "telemetry/Logger.hpp"
#include "telemetry/Statistics.hpp"

#if defined(_MSC_VER) || defined(_WIN32)
#include <winsock2.h>
#endif

namespace SpexronDPI::Engine {

void PacketProcessor::Process(Core::PacketContext& ctx) {

    if (!ctx.isOutbound) {
        // Gelen DNS yanıtlarını kontrol et
        if (ctx.udpHeader && ntohs(ctx.udpHeader->srcPort) == 53) {

            if (ctx.payload.size() >= 4) {
                // Bilinen DNS Spoofing imzalarını ara
                for (size_t i = 0; i <= ctx.payload.size() - 4; ++i) {
                    if (ctx.payload[i] == 195 && ctx.payload[i+1] == 175 && 
                        (ctx.payload[i+2] == 254 || ctx.payload[i+2] == 255) && 
                        ctx.payload[i+3] == 2) {

                        LOG_INFO("{}", ctx.GetTraceLog("DNS Spoofing", "N/A", "DNS Spoofing IP'si bulundu, sahte DNS paketi düşürüldü!"));
                        DROP_PACKET(ctx, "Sahte DNS Yanıtı Düşürüldü");
                        return;
                    }
                }
            }
        } else if (ctx.udpHeader && ntohs(ctx.udpHeader->srcPort) == 5353) {
            // AdGuard DNS'ten dönen alternatif port (5353) yanıtını orijinal portuna (53) çevir
            PacketModifier::RestoreDns(ctx);
            return;
        }
        return; 
    }

    if (ctx.udpHeader) {
        uint16_t dstPort = ntohs(ctx.udpHeader->dstPort);
        
        // QUIC (UDP 443) trafiğini engelleyerek istemciyi TCP'ye düşür (TCP Fallback)
        if (dstPort == 443 && Rules::RuleEngine::IsTargetDomain("")) { 
            DROP_PACKET(ctx, "QUIC (UDP 443) paketi TCP Fallback için düşürüldü");
            return;
        }

        // Standart DNS (53) isteklerini AdGuard sansürsüz portuna (5353) yönlendir
        if (dstPort == 53) {
            PacketModifier::RedirectDns(ctx);
            return;
        }
    }

    if (ctx.tcpHeader) {
        uint16_t dstPort = ntohs(ctx.tcpHeader->dstPort); 

        if (dstPort == 80 || dstPort == 443) {

            std::string domain = "";
            // Payload içinden Host veya SNI (Server Name Indication) çıkar
            if (ctx.payload.size() > 0) {
                domain = Parsers::L7Parser::ExtractHostOrSNI(ctx.payload, dstPort);
                if (!domain.empty()) {
                    if (domain == "updates.discord.com") {
                        ctx.isTargetSni = true;
                        LOG_INFO("{}", ctx.GetTraceLog("Detect", "Target SNI", "updates.discord.com eşleşti!"));
                    }
                    LOG_TRACE("{}", ctx.GetTraceLog("L7 Analyze", "N/A", "Extracted SNI/Host: " + domain));
                }
            }

            if (Rules::RuleEngine::IsTargetDomain(domain) || Rules::RuleEngine::IsTargetDomain("")) {
                if (ctx.isTargetSni) {
                    LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "Rule matched. Neden = SNI (updates.discord.com) bulundu."));
                }
                // SYN paketlerini atla, müdahaleyi L7 aşamasında (ClientHello) yapacağız
                bool isSyn = (ctx.tcpHeader->flags & 0x02) != 0;
                if (isSyn) {

                } 
                else if (ctx.payload.size() > 5) {
                    // TLS ClientHello (0x16 0x03) veya HTTP GET (GET ) kontrolü
                    bool isClientHello = (dstPort == 443 && ctx.payload[0] == 0x16 && ctx.payload[1] == 0x03);
                    bool isHttp = (dstPort == 80 && ctx.payload[0] == 'G' && ctx.payload[1] == 'E' && ctx.payload[2] == 'T');

                    if (isClientHello || isHttp) {
                        if (ctx.isTargetSni && isClientHello) {
                            LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "TLS ClientHello tespit edildi. Bypass taktikleri uygulanacak."));
                        }

                        // Taktik 1: DPI'ı yanıltmak için sahte ClientHello ve Sequence/Ack drift (-10000/-66000) gönder
                        if (isClientHello) {
                            PacketModifier::InjectFakeClientHello(ctx);
                        }

                        // Taktik 2: Orijinal paketi ortadan bölerek (TCP Segmentation) SNI okumasını engelle
                        PacketModifier::SplitTcpSegment(ctx);
                    }
                }

                if (ctx.isModified || !ctx.injectedPackets.empty()) {
                    Telemetry::Statistics::IncrementBypassedPackets();
                }
            }
        }
    }
}

} 
