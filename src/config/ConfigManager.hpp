#pragma once
#include <string>

namespace SpexronDPI::Config {

class ConfigManager {
public:

    static bool LoadConfig(const std::string& filePath);

    static void Reload();
};

} 
