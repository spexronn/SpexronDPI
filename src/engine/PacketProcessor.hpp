#pragma once

#include "core/PacketContext.hpp"

namespace SpexronDPI::Engine {

class PacketProcessor {
public:

    static void Process(Core::PacketContext& ctx);
};

} 
