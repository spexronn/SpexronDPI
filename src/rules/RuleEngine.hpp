#pragma once
#include <string>
#include <unordered_set>

namespace SpexronDPI::Rules {

class RuleEngine {
public:

    static void SetGlobalMode(bool enable);

    static void AddBlockedDomain(const std::string& domain);
    static void ClearRules();

    static bool IsTargetDomain(const std::string& domain);

private:
    static bool s_globalMode;

    static std::unordered_set<std::string> s_blockedDomains;
};

} 
