#include "HostManager.hpp"
#include "telemetry/Logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <wininet.h>
#include <iostream>

#pragma comment(lib, "wininet.lib")

using json = nlohmann::json;

namespace SpexronDPI::Engine {

std::vector<std::string> HostManager::targetDomains = {
    "discord.com",
    "gateway.discord.gg",
    "discordapp.com",
    "discordapp.net",
    "cdn.discordapp.com",
    "media.discordapp.net",
    "router.discordapp.net",
    "discord.gg",
    "dl.discordapp.net",
    "updates.discord.com",
    "status.discord.com",
    "discord.media",
    "discordcdn.com"
};

const std::string HostManager::START_MARKER = "# --- SPEXRONDPI DISCORD BYPASS START ---";
const std::string HostManager::END_MARKER = "# --- SPEXRONDPI DISCORD BYPASS END ---";

std::string HostManager::ResolveDoH(const std::string& domain) {
    std::string resultIp = "";
    HINTERNET hInternet = InternetOpenA("SpexronDPI", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    std::string url = "https://cloudflare-dns.com/dns-query?name=" + domain + "&type=A";
    const char* headers = "accept: application/dns-json\r\n";

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers, -1, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (hUrl) {
        char buffer[4096];
        DWORD bytesRead;
        std::string responseStr;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            responseStr += buffer;
        }

        try {
            auto j = json::parse(responseStr);
            if (j.contains("Answer") && j["Answer"].is_array() && j["Answer"].size() > 0) {

                for (const auto& answer : j["Answer"]) {
                    if (answer.contains("type") && answer["type"] == 1) { 
                        resultIp = answer["data"].get<std::string>();
                        break;
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("DoH JSON Parsing failed for {}: {}", domain, e.what());
        }

        InternetCloseHandle(hUrl);
    } else {
        LOG_ERROR("InternetOpenUrlA failed for DoH query: {}", domain);
    }
    InternetCloseHandle(hInternet);
    return resultIp;
}

void HostManager::ApplyBypass() {
    LOG_INFO("Cloudflare DoH API kullanilarak Discord IP'leri cozumleniyor...");
    std::string hostsPath = "C:\\Windows\\System32\\drivers\\etc\\hosts";

    RemoveBypass();

    std::vector<std::string> newEntries;
    for (const auto& domain : targetDomains) {
        std::string ip = ResolveDoH(domain);
        if (!ip.empty()) {
            newEntries.push_back(ip + " " + domain);
            LOG_INFO("Cozumlendi: {} -> {}", domain, ip);
        } else {
            LOG_WARN("Cozumlenemedi: {}", domain);
        }
    }

    if (newEntries.empty()) {
        LOG_WARN("Hicbir Discord IP'si cozumlenemedi, Hosts dosyasi guncellenmedi.");
        return;
    }

    std::ofstream hostsFile(hostsPath, std::ios::app);
    if (hostsFile.is_open()) {
        hostsFile << "\n" << START_MARKER << "\n";
        for (const auto& entry : newEntries) {
            hostsFile << entry << "\n";
        }
        hostsFile << END_MARKER << "\n";
        hostsFile.close();
        LOG_INFO("Discord IP'leri başarıyla Windows Hosts dosyasına eklendi (DNS Bypass aktif!).");
    } else {
        LOG_ERROR("Hosts dosyası açılamadı! Lütfen programı Yönetici olarak çalıştırdığınızdan emin olun.");
    }
}

void HostManager::RemoveBypass() {
    std::string hostsPath = "C:\\Windows\\System32\\drivers\\etc\\hosts";
    std::ifstream inFile(hostsPath);
    if (!inFile.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    bool inBlock = false;

    while (std::getline(inFile, line)) {
        if (line.find(START_MARKER) != std::string::npos) {
            inBlock = true;
            continue;
        }
        if (line.find(END_MARKER) != std::string::npos) {
            inBlock = false;
            continue;
        }
        if (!inBlock) {
            lines.push_back(line);
        }
    }
    inFile.close();

    std::ofstream outFile(hostsPath, std::ios::trunc);
    if (outFile.is_open()) {
        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        outFile.close();
        LOG_INFO("SpexronDPI Discord Bypass kayitlari Hosts dosyasindan temizlendi.");
    }
}

} 
