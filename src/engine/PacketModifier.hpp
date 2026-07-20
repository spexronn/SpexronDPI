#pragma once

#include "core/PacketContext.hpp"

namespace SpexronDPI::Engine {

class PacketModifier {
public:

    static void ModifyTTL(Core::PacketContext& ctx, uint8_t newTtl);

    static void SplitTcpSegment(Core::PacketContext& ctx);

    static void InjectFakeClientHello(Core::PacketContext& ctx);

    static const std::vector<uint8_t> FAKE_CLIENTHELLO_PAYLOAD;

    static void RedirectDns(Core::PacketContext& ctx);
    static void RestoreDns(Core::PacketContext& ctx);

    static void RecalculateChecksums(Core::PacketContext& ctx);
};

} 
