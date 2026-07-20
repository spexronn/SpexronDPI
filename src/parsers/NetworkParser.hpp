#pragma once

#include "core/PacketContext.hpp"

namespace SpexronDPI::Parsers {

class NetworkParser {
public:

    static bool Parse(Core::PacketContext& ctx);
};

} 
