#include "RuleEngine.hpp"

namespace SpexronDPI::Rules {

bool RuleEngine::s_globalMode = true; 
std::unordered_set<std::string> RuleEngine::s_blockedDomains;

void RuleEngine::SetGlobalMode(bool enable) {
    s_globalMode = enable;
}

void RuleEngine::AddBlockedDomain(const std::string& domain) {
    s_blockedDomains.insert(domain);
}

void RuleEngine::ClearRules() {
    s_blockedDomains.clear();
}

bool RuleEngine::IsTargetDomain(const std::string& domain) {

    if (s_globalMode) {
        return true; 
    }

    if (domain.empty()) return false;

    return s_blockedDomains.find(domain) != s_blockedDomains.end();
}

} 
