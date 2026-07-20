#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <windows.h> 

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#include "telemetry/Logger.hpp"
#include "telemetry/Statistics.hpp"
#include "config/ConfigManager.hpp"
#include "capture/CaptureWorker.hpp"
#include "engine/HostManager.hpp"
#include "gui/Dashboard.hpp"

std::atomic<bool> g_IsRunning = true;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_WARN("Kapatma sinyali alındı.");
        LOG_WARN("SpexronDPI durduruluyor...");
        g_IsRunning = false;
    }
}

int main(int argc, char* argv[]) {
    using namespace SpexronDPI;

    bool startHidden = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--startup") {
            startHidden = true;
        }
    }

    // Single Instance Kontrolü
    HANDLE hMutex = CreateMutexA(NULL, FALSE, "SpexronDPI_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Zaten çalışıyorsa mevcut pencereyi bul ve öne getir
        HWND hwnd = FindWindowA(NULL, "SpexronDPI Kontrol Paneli");
        if (hwnd) {
            ShowWindow(hwnd, SW_RESTORE);
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }
        return 0; // İkinci kopyayı kapat
    }

    try {

        Telemetry::Logger::Init();
        LOG_INFO("================================================");
        LOG_INFO("    SpexronDPI Başlatılıyor (v1.0.0)");
        LOG_INFO("================================================");

        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        if (!Config::ConfigManager::LoadConfig("SpexronDPI_Config.json")) {
            LOG_CRITICAL("Yapılandırma dosyası okunamadı!");
            return -1;
        }

        Engine::HostManager::ApplyBypass();

        Capture::CaptureWorker worker;


        GUI::Dashboard::Run(worker, startHidden);

        worker.Stop();
        Engine::HostManager::RemoveBypass();

        LOG_INFO("Son İstatistikler:");
        Telemetry::Statistics::PrintSummary();
        LOG_INFO("SpexronDPI kapatıldı.");

        return 0;
    } catch (const std::exception& e) {
        std::string errorMsg = "SpexronDPI Beklenmeyen Bir Hata İle Karşılaştı:\n\n" + std::string(e.what()) + "\n\n(Not: Program C:\\Program Files içerisindeyse log dosyası yazabilmesi için Yönetici olarak başlatılması gerekebilir. Ayrıca eksik DLL olup olmadığını kontrol edin.)";
        MessageBoxA(NULL, errorMsg.c_str(), "SpexronDPI - Ciddi Hata", MB_ICONERROR | MB_OK);
        return -1;
    } catch (...) {
        MessageBoxA(NULL, "Bilinmeyen kritik bir hata olustu.", "SpexronDPI - Ciddi Hata", MB_ICONERROR | MB_OK);
        return -1;
    }
}
