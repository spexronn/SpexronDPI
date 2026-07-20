#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace SpexronDPI::Engine {

class HostManager {
public:

    static void ApplyBypass();

    static void RemoveBypass();

private:
    static std::string ResolveDoH(const std::string& domain);
    static std::vector<std::string> targetDomains;
    static const std::string START_MARKER;
    static const std::string END_MARKER;
};

} 
