#pragma once

#include <string>
#include <span>

namespace SpexronDPI::Parsers {

class L7Parser {
public:

    static std::string ExtractHostOrSNI(std::span<const uint8_t> payload, uint16_t port);

private:
    static std::string ParseHttpHost(std::span<const uint8_t> payload);
    static std::string ParseTlsSNI(std::span<const uint8_t> payload);
};

} 
