#include "ConfigManager.hpp"
#include "rules/RuleEngine.hpp"
#include "telemetry/Logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace SpexronDPI::Config {

bool ConfigManager::LoadConfig(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        LOG_WARN("Ayar dosyası bulunamadı, varsayılan ayarlar oluşturuluyor...");

        nlohmann::json defaultJson = {
            {"mode", "global"},
            {"bypass_tactics", {"ttl_spoof", "tcp_fragment"}},
            {"target_domains", nlohmann::json::array()} 
        };

        std::ofstream outFile(filePath);
        outFile << defaultJson.dump(4); 

        for (const auto& domain : defaultJson["target_domains"]) {
            Rules::RuleEngine::AddBlockedDomain(domain.get<std::string>());
        }
        return true;
    }

    try {
        nlohmann::json configJson;
        file >> configJson;

        Rules::RuleEngine::ClearRules();

        if (configJson.contains("mode") && configJson["mode"] == "global") {
            Rules::RuleEngine::SetGlobalMode(true);
            LOG_INFO("Global Mod aktif. Tüm HTTP/HTTPS trafiği filtreleniyor.");
        } else {
            Rules::RuleEngine::SetGlobalMode(false);
            for (const auto& domain : configJson["target_domains"]) {
                Rules::RuleEngine::AddBlockedDomain(domain.get<std::string>());
            }
            LOG_INFO("Ayarlar okundu.");
        }

        return true;
    } 
    catch (const std::exception& e) {
        LOG_ERROR("JSON format hatası: {}", e.what());
        return false;
    }
}

void ConfigManager::Reload() {
    LOG_INFO("Ayarlar yenileniyor...");
    LoadConfig("SpexronDPI_Config.json");
}

} 
