#include "L7Parser.hpp"
#include <string_view>

namespace SpexronDPI::Parsers {

std::string L7Parser::ExtractHostOrSNI(std::span<const uint8_t> payload, uint16_t port) {
    if (payload.empty()) return "";

    if (port == 80) { 
        return ParseHttpHost(payload);
    } else if (port == 443) { 
        return ParseTlsSNI(payload);
    }

    return "";
}

std::string L7Parser::ParseHttpHost(std::span<const uint8_t> payload) {

    std::string_view data(reinterpret_cast<const char*>(payload.data()), payload.size());

    size_t hostPos = data.find("Host: ");
    if (hostPos == std::string_view::npos) return "";

    hostPos += 6; 
    size_t endPos = data.find("\r\n", hostPos); 

    if (endPos == std::string_view::npos) return "";

    return std::string(data.substr(hostPos, endPos - hostPos));
}

std::string L7Parser::ParseTlsSNI(std::span<const uint8_t> payload) {

    if (payload.size() < 43) return ""; 

    if (payload[0] != 0x16 || payload[5] != 0x01) return "";

    size_t pos = 43; 

    if (pos >= payload.size()) return "";
    uint8_t sessionIdLen = payload[pos];
    pos += 1 + sessionIdLen;

    if (pos + 2 > payload.size()) return "";
    uint16_t cipherSuitesLen = (payload[pos] << 8) | payload[pos + 1];
    pos += 2 + cipherSuitesLen;

    if (pos >= payload.size()) return "";
    uint8_t compressionMethodsLen = payload[pos];
    pos += 1 + compressionMethodsLen;

    if (pos + 2 > payload.size()) return "";
    uint16_t extensionsLen = (payload[pos] << 8) | payload[pos + 1];
    pos += 2;

    size_t endPos = pos + extensionsLen;
    if (endPos > payload.size()) endPos = payload.size();

    while (pos + 4 <= endPos) {
        uint16_t extType = (payload[pos] << 8) | payload[pos + 1];
        uint16_t extLen = (payload[pos + 2] << 8) | payload[pos + 3];
        pos += 4;

        if (extType == 0x0000) { 
            if (pos + 5 <= endPos) {
                uint8_t sniType = payload[pos + 2];
                uint16_t sniLen = (payload[pos + 3] << 8) | payload[pos + 4];
                pos += 5;

                if (sniType == 0x00 && pos + sniLen <= endPos) { 
                    return std::string((char*)&payload[pos], sniLen);
                }
            }
            break;
        } else {
            pos += extLen;
        }
    }
    return "";
}

} 
