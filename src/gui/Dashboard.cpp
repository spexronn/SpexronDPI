#include "Dashboard.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "telemetry/Statistics.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <functional>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

#pragma execution_character_set("utf-8")

#define WM_TRAYICON (WM_USER + 1)

namespace SpexronDPI::GUI {

// --- Language System ---
enum class Lang { TR, EN, RU, DE, ES };
static Lang g_CurrentLang = Lang::TR;

static const char* L(const char* tr, const char* en, const char* ru, const char* de, const char* es) {
    if (g_CurrentLang == Lang::EN) return en;
    if (g_CurrentLang == Lang::RU) return ru;
    if (g_CurrentLang == Lang::DE) return de;
    if (g_CurrentLang == Lang::ES) return es;
    return tr;
}

static void LoadLanguageFromRegistry() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\SpexronDPI", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char langStr[16];
        DWORD size = sizeof(langStr);
        if (RegQueryValueExA(hKey, "Language", NULL, NULL, (LPBYTE)langStr, &size) == ERROR_SUCCESS) {
            std::string lang = langStr;
            if (lang == "en") g_CurrentLang = Lang::EN;
            else if (lang == "ru") g_CurrentLang = Lang::RU;
            else if (lang == "de") g_CurrentLang = Lang::DE;
            else if (lang == "es") g_CurrentLang = Lang::ES;
            else g_CurrentLang = Lang::TR;
        }
        RegCloseKey(hKey);
    }
}

// Global UI State
static std::atomic<bool> g_ExitRequested = false;
static std::atomic<bool> g_ToggleBypassRequested = false;
static std::atomic<bool> g_ClearCacheRequested = false;
static std::atomic<bool> g_ShowWindowRequested = false;
static HWND g_Hwnd = NULL;
static Capture::CaptureWorker* g_WorkerPtr = nullptr;
static GLFWwindow* g_Window = nullptr;

static WNDPROC g_OriginalWndProc;

static std::wstring Utf8ToWide(const char* utf8Str) {
    if (!utf8Str) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, &wstrTo[0], size_needed);
    return wstrTo;
}

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, 1, Utf8ToWide(L("Kontrol Panelini Aç", "Open Control Panel", "Открыть панель управления", "Systemsteuerung öffnen", "Abrir Panel de Control")).c_str());
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, 2, Utf8ToWide(L("Bypass Etkinleştir / Devre Dışı Bırak", "Enable/Disable Bypass", "Вкл/Выкл обход", "Bypass Aktivieren/Deaktivieren", "Activar/Desactivar Bypass")).c_str());
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, 3, Utf8ToWide(L("Önbelleği Temizle", "Clear Cache", "Очистить кэш", "Cache leeren", "Limpiar caché")).c_str());
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, 4, Utf8ToWide(L("Çıkış", "Exit", "Выход", "Beenden", "Salir")).c_str());
            
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            
            if (cmd == 1) {
                g_ShowWindowRequested = true;
            } else if (cmd == 2) {
                g_ToggleBypassRequested = true;
            } else if (cmd == 3) {
                g_ClearCacheRequested = true;
            } else if (cmd == 4) {
                g_ExitRequested = true;
            }
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            g_ShowWindowRequested = true;
        }
        return 0;
    }
    return CallWindowProc(g_OriginalWndProc, hwnd, msg, wParam, lParam);
}

static HICON CreateStateIcon(bool isActive) {
    int width = 32;
    int height = 32;
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    uint32_t* pixels = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    HBITMAP hMask = CreateBitmap(width, height, 1, 1, NULL);
    
    for (int i = 0; i < width * height; ++i) pixels[i] = 0;
    
    uint32_t color = isActive ? 0xFF00D8FF : 0xFFFFFFFF;
    
    int bodyX = 6, bodyY = 16, bodyW = 20, bodyH = 14;
    int shX = 10, shY = 7, shW = 12, shH = 9;
    
    for (int y = shY; y < bodyY; ++y) {
        for (int x = shX; x < shX + shW; ++x) {
            if (x >= shX && x < shX + 3) pixels[y * width + x] = color;
            if (x >= shX + shW - 3 && x < shX + shW) pixels[y * width + x] = color;
            if (y >= shY && y < shY + 3) {
                if ((y == shY && (x == shX || x == shX + shW - 1))) continue;
                pixels[y * width + x] = color;
            }
        }
    }
    
    for (int y = bodyY; y < bodyY + bodyH; ++y) {
        for (int x = bodyX; x < bodyX + bodyW; ++x) {
            if ((y == bodyY || y == bodyY + bodyH - 1) && (x == bodyX || x == bodyX + bodyW - 1)) continue;
            pixels[y * width + x] = color;
        }
    }
    
    for (int y = bodyY + 3; y <= bodyY + 6; ++y) {
        for (int x = 14; x <= 17; ++x) {
            float dx = x - 15.5f;
            float dy = y - 4.5f - bodyY;
            if (dx*dx + dy*dy <= 4.5f) {
                pixels[y * width + x] = 0;
            }
        }
    }
    for (int y = bodyY + 6; y <= bodyY + 9; ++y) {
        for (int x = 15; x <= 16; ++x) {
            pixels[y * width + x] = 0;
        }
    }
    
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = hMask;
    ii.hbmColor = hBitmap;
    HICON hIcon = CreateIconIndirect(&ii);
    
    DeleteObject(hBitmap);
    DeleteObject(hMask);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    return hIcon;
}

static void AddTrayIcon(HWND hwnd) {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    
    nid.hIcon = CreateStateIcon(false);
    
    strcpy_s(nid.szTip, "SpexronDPI");
    Shell_NotifyIconA(NIM_ADD, &nid);
    DestroyIcon(nid.hIcon);
}

static void UpdateTrayIcon(HWND hwnd, bool isActive) {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON;
    nid.hIcon = CreateStateIcon(isActive);
    
    Shell_NotifyIconA(NIM_MODIFY, &nid);
    DestroyIcon(nid.hIcon);
}

static void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconA(NIM_DELETE, &nid);
}

static void EnableFluentDesign(HWND hwnd) {
    int value = 2; // DWMSBT_MAINWINDOW (Mica)
    DwmSetWindowAttribute(hwnd, 38, &value, sizeof(value));
    
    int dark = 1;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    
    int corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
}

static std::string GetWindowsVersion() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
      char productName[256];
      DWORD productNameSize = sizeof(productName);
      if (RegQueryValueExA(hKey, "ProductName", NULL, NULL, (LPBYTE)productName,
                           &productNameSize) == ERROR_SUCCESS) {
        char displayVersion[256];
        DWORD displayVersionSize = sizeof(displayVersion);
        std::string versionStr = productName;
        if (RegQueryValueExA(hKey, "DisplayVersion", NULL, NULL,
                             (LPBYTE)displayVersion,
                             &displayVersionSize) == ERROR_SUCCESS) {
          versionStr += " " + std::string(displayVersion);
        }
        RegCloseKey(hKey);
        return versionStr;
      }
      RegCloseKey(hKey);
    }
    return L("Bilinmeyen Windows Sürümü", "Unknown Windows Version", "Неизвестная версия Windows", "Unbekannte Windows-Version", "Versión de Windows Desconocida");
}

static void SaveBypassState(bool enabled) {
  HKEY hKey;
  if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\SpexronDPI", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
    DWORD value = enabled ? 1 : 0;
    RegSetValueExA(hKey, "BypassEnabled", 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
    RegCloseKey(hKey);
  }
}

static bool LoadBypassState() {
  HKEY hKey;
  DWORD value = 1; // Default to true
  if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\SpexronDPI", 0, NULL, 0, KEY_READ, NULL, &hKey, NULL) == ERROR_SUCCESS) {
    DWORD size = sizeof(value);
    RegQueryValueExA(hKey, "BypassEnabled", NULL, NULL, (LPBYTE)&value, &size);
    RegCloseKey(hKey);
  }
  return value != 0;
}

static void SetupModernStyle() {
  ImGuiStyle &style = ImGui::GetStyle();

  style.WindowRounding = 0.0f;
  style.FrameRounding = 8.0f;
  style.PopupRounding = 8.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 8.0f;
  style.TabRounding = 8.0f;
  style.ChildRounding = 12.0f;

  style.WindowPadding = ImVec2(25, 25);
  style.FramePadding = ImVec2(15, 10);
  style.ItemSpacing = ImVec2(20, 20);

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.055f, 0.07f, 1.00f);
  colors[ImGuiCol_Border] = ImVec4(0.133f, 0.145f, 0.176f, 1.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.086f, 0.094f, 0.118f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.086f, 0.094f, 0.118f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
  colors[ImGuiCol_Text] = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.57f, 0.61f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.133f, 0.145f, 0.176f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.086f, 0.094f, 0.118f, 1.00f);
}

static void DrawCardBase(const char* id, const char* icon, ImVec4 iconBgColor, ImVec4 iconColor, std::function<void()> contentFn) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.086f, 0.094f, 0.118f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.133f, 0.145f, 0.176f, 1.0f));
    
    ImGui::BeginChild(id, ImVec2(0, 110), true, ImGuiWindowFlags_NoScrollbar);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    
    float circleRadius = 24.0f;
    ImVec2 circleCenter = ImVec2(p.x + 40.0f, p.y + 55.0f);
    drawList->AddCircleFilled(circleCenter, circleRadius, ImGui::ColorConvertFloat4ToU32(iconBgColor));
    
    ImVec2 textSize = ImGui::CalcTextSize(icon);
    drawList->AddText(ImVec2(circleCenter.x - textSize.x / 2.0f, circleCenter.y - textSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(iconColor), icon);
    
    ImGui::SetCursorPos(ImVec2(85.0f, 22.0f));
    ImGui::BeginGroup();
    contentFn();
    ImGui::EndGroup();
    
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

int Dashboard::Run(Capture::CaptureWorker &worker, bool startHidden) {
  if (!glfwInit())
    return -1;
    
  LoadLanguageFromRegistry();

  g_WorkerPtr = &worker;

  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
  
  if (startHidden) {
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }

  GLFWwindow *window = glfwCreateWindow(800, 600, "SpexronDPI", NULL, NULL);
  if (window == NULL)
    return -1;
    
  g_Window = window;
  g_Hwnd = glfwGetWin32Window(window);
  
  EnableFluentDesign(g_Hwnd);
  
  AddTrayIcon(g_Hwnd);
  g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)TrayWndProc);

  glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
    glfwHideWindow(w);
    glfwSetWindowShouldClose(w, GLFW_FALSE);
  });

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  
  ImFontConfig config;
  config.MergeMode = false;
  static const ImWchar all_ranges[] = {
      0x0020, 0x00FF, // Basic Latin + Latin Supplement (EN, DE, ES, TR)
      0x0100, 0x017F, // Latin Extended-A (TR: Ğğ, Şş, İı vb)
      0x0400, 0x04FF, // Cyrillic (RU)
      0,
  };
  io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &config, all_ranges);

  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  static const ImWchar icon_ranges[] = { 0xE700, 0xF8FF, 0 };
  io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segmdl2.ttf", 16.0f, &icons_config, icon_ranges);

  ImGui::StyleColorsDark();
  SetupModernStyle();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  
  if (LoadBypassState() && !worker.IsRunning()) {
      worker.Start("!loopback and !impostor and ((outbound and (ip or ipv6)) or (inbound and udp and (udp.SrcPort == 53 or udp.SrcPort == 5353)))");
  }

  bool showCacheClearedMsg = false;
  float cacheMsgTimer = 0.0f;

  while (!glfwWindowShouldClose(window)) {
    if (g_ExitRequested) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (g_ShowWindowRequested) {
        glfwShowWindow(window);
        glfwRequestWindowAttention(window);
        g_ShowWindowRequested = false;
    }
    if (g_ToggleBypassRequested) {
        if (worker.IsRunning()) {
            worker.Stop();
            SaveBypassState(false);
        } else {
            worker.Start("!loopback and !impostor and ((outbound and (ip or ipv6)) or (inbound and udp and (udp.SrcPort == 53 or udp.SrcPort == 5353)))");
            SaveBypassState(true);
        }
        g_ToggleBypassRequested = false;
    }
    if (g_ClearCacheRequested) {
        system("ipconfig /flushdns");
        showCacheClearedMsg = true;
        cacheMsgTimer = 3.0f;
        g_ClearCacheRequested = false;
    }

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("SpexronDPI_Main", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove);

    bool isRunning = worker.IsRunning();

    static bool firstFrame = true;
    static bool lastIsRunning = false;
    if (firstFrame || isRunning != lastIsRunning) {
        UpdateTrayIcon(g_Hwnd, isRunning);
        lastIsRunning = isRunning;
        firstFrame = false;
    }

    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 160, 20));
    ImGui::SetNextItemWidth(140);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    const char* langPreview = "";
    if (g_CurrentLang == Lang::TR) langPreview = (const char*)u8"\uE774  Türkçe";
    else if (g_CurrentLang == Lang::EN) langPreview = (const char*)u8"\uE774  English";
    else if (g_CurrentLang == Lang::RU) langPreview = (const char*)u8"\uE774  Русский";
    else if (g_CurrentLang == Lang::DE) langPreview = (const char*)u8"\uE774  Deutsch";
    else if (g_CurrentLang == Lang::ES) langPreview = (const char*)u8"\uE774  Español";

    if (ImGui::BeginCombo("##Lang", langPreview)) {
        if (ImGui::Selectable((const char*)u8"\uE774  Türkçe")) g_CurrentLang = Lang::TR;
        if (ImGui::Selectable((const char*)u8"\uE774  English")) g_CurrentLang = Lang::EN;
        if (ImGui::Selectable((const char*)u8"\uE774  Русский")) g_CurrentLang = Lang::RU;
        if (ImGui::Selectable((const char*)u8"\uE774  Deutsch")) g_CurrentLang = Lang::DE;
        if (ImGui::Selectable((const char*)u8"\uE774  Español")) g_CurrentLang = Lang::ES;
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(20, 75));

    if (ImGui::BeginTable("InfoCards", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        
        ImGui::TableSetColumnIndex(0);
        double uptime = glfwGetTime();
        int hours = static_cast<int>(uptime) / 3600;
        int mins = (static_cast<int>(uptime) % 3600) / 60;
        int secs = static_cast<int>(uptime) % 60;
        char uptimeStr[64];
        snprintf(uptimeStr, sizeof(uptimeStr), "%02d:%02d:%02d", hours, mins, secs);
        DrawCardBase("Uptime", (const char*)u8"\uE121", ImVec4(0.1f, 0.14f, 0.22f, 1.0f), ImVec4(0.39f, 0.59f, 0.94f, 1.0f), [&]() {
            ImGui::TextColored(ImVec4(0.85f, 0.87f, 0.91f, 1.0f), "%s", L("Çalışma Süresi", "Uptime", "Время работы", "Betriebszeit", "Tiempo de Actividad"));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0f);
            ImGui::SetWindowFontScale(1.8f);
            ImGui::TextColored(ImVec4(0.94f, 0.94f, 0.96f, 1.0f), "%s", uptimeStr);
            ImGui::SetWindowFontScale(1.0f);
        });

        ImGui::TableSetColumnIndex(1);
        static std::string winVer = GetWindowsVersion();
        DrawCardBase("WinVer", (const char*)u8"\uE7B3", ImVec4(0.12f, 0.18f, 0.29f, 1.0f), ImVec4(0.47f, 0.71f, 1.0f, 1.0f), [&]() {
            ImGui::TextColored(ImVec4(0.85f, 0.87f, 0.91f, 1.0f), "%s", L("Windows Sürümü", "Windows Version", "Версия Windows", "Windows-Version", "Versión de Windows"));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
            ImGui::SetWindowFontScale(1.2f);
            ImGui::TextColored(ImVec4(0.35f, 0.59f, 0.94f, 1.0f), "%s", winVer.c_str());
            ImGui::SetWindowFontScale(1.0f);
        });

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        
        const char *statusStr = isRunning ? L("Etkin", "Enabled", "Вкл", "Aktiviert", "Activado") : L("Devre Dışı", "Disabled", "Выкл", "Deaktiviert", "Desactivado");
        ImVec4 statusColor = isRunning ? ImVec4(0.27f, 0.71f, 0.39f, 1.0f) : ImVec4(0.86f, 0.31f, 0.35f, 1.0f);
        
        DrawCardBase("WinDivert", (const char*)u8"\uE811", ImVec4(0.1f, 0.18f, 0.14f, 1.0f), ImVec4(0.27f, 0.71f, 0.39f, 1.0f), [&]() {
            ImGui::TextColored(ImVec4(0.85f, 0.87f, 0.91f, 1.0f), "WinDivert");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
            ImGui::SetWindowFontScale(1.2f);
            ImGui::TextColored(ImVec4(0.94f, 0.94f, 0.96f, 1.0f), "WinDivert 2.2.2");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
            ImGui::TextColored(statusColor, "%s", statusStr);
        });

        ImGui::TableSetColumnIndex(1);
        DrawCardBase("DPIBypass", (const char*)u8"\uE71A", ImVec4(0.1f, 0.18f, 0.14f, 1.0f), ImVec4(0.27f, 0.71f, 0.39f, 1.0f), [&]() {
            ImGui::TextColored(ImVec4(0.85f, 0.87f, 0.91f, 1.0f), "DPI Bypass");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
            ImGui::SetWindowFontScale(1.2f);
            ImGui::TextColored(statusColor, "%s", statusStr);
            ImGui::SetWindowFontScale(1.0f);
        });

        ImGui::EndTable();
    }

    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isRunning ? ImVec4(0.08f, 0.14f, 0.1f, 1.0f) : ImVec4(0.14f, 0.08f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, isRunning ? ImVec4(0.14f, 0.27f, 0.18f, 1.0f) : ImVec4(0.27f, 0.14f, 0.18f, 1.0f));
    ImGui::BeginChild("StatusBanner", ImVec2(0, 50), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(20, 16));
    ImGui::TextColored(isRunning ? ImVec4(0.31f, 0.78f, 0.47f, 1.0f) : ImVec4(0.86f, 0.31f, 0.35f, 1.0f), "%s", (const char*)u8"\uE83D");
    ImGui::SameLine(0, 15);
    ImGui::TextColored(ImVec4(0.86f, 0.86f, 0.88f, 1.0f), "%s", L("Durum:", "Status:", "Статус:", "Status:", "Estado:"));
    ImGui::SameLine(0, 5);
    const char* bannerStatus = isRunning ? L("Bypass Etkin", "Bypass Enabled", "Обход активен", "Bypass Aktiviert", "Bypass Activado") : L("Bypass Devre Dışı", "Bypass Disabled", "Обход отключен", "Bypass Deaktiviert", "Bypass Desactivado");
    ImGui::TextColored(isRunning ? ImVec4(0.31f, 0.78f, 0.47f, 1.0f) : ImVec4(0.86f, 0.31f, 0.35f, 1.0f), "%s", bannerStatus);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    ImGui::Spacing();

    ImGui::Spacing();

    float btnWidth = (ImGui::GetContentRegionAvail().x - 40.0f) / 3.0f;
    float startY = ImGui::GetCursorPosY();

    // 1. Önbelleği Temizle (Left)
    ImGui::SetCursorPos(ImVec2(20, startY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.086f, 0.094f, 0.118f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.07f, 0.09f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.133f, 0.145f, 0.176f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (ImGui::Button("##ClearCache", ImVec2(btnWidth, 50))) {
        system("ipconfig /flushdns");
        showCacheClearedMsg = true;
        cacheMsgTimer = 3.0f;
    }
    
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();
    ImVec2 btnCenter = ImVec2((btnMin.x + btnMax.x) / 2.0f, (btnMin.y + btnMax.y) / 2.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const char* clearText = L("Önbelleği Temizle", "Clear Cache", "Очистить кэш", "Cache leeren", "Limpiar caché");
    ImVec2 textSize = ImGui::CalcTextSize(clearText);
    const char* trashIcon = (const char*)u8"\uE74D";
    ImVec2 iconSize = ImGui::CalcTextSize(trashIcon);
    float totalWidth = iconSize.x + 10.0f + textSize.x;
    drawList->AddText(ImVec2(btnCenter.x - totalWidth / 2.0f, btnCenter.y - iconSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.86f, 0.86f, 0.88f, 1.0f)), trashIcon);
    drawList->AddText(ImVec2(btnCenter.x - totalWidth / 2.0f + iconSize.x + 10.0f, btnCenter.y - textSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.86f, 0.86f, 0.88f, 1.0f)), clearText);

    // 2. Dosya Konumunu Aç (Middle)
    ImGui::SetCursorPos(ImVec2(20 + btnWidth + 20, startY));
    if (ImGui::Button("##OpenLocation", ImVec2(btnWidth, 50))) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string cmd = "/select,\"" + std::string(path) + "\"";
        ShellExecuteA(NULL, "open", "explorer.exe", cmd.c_str(), NULL, SW_SHOWNORMAL);
    }
    
    btnMin = ImGui::GetItemRectMin();
    btnMax = ImGui::GetItemRectMax();
    btnCenter = ImVec2((btnMin.x + btnMax.x) / 2.0f, (btnMin.y + btnMax.y) / 2.0f);
    const char* openFolderText = L("Dosya Konumunu Aç", "Open Location", "Расположение", "Dateipfad", "Abrir ubicación");
    textSize = ImGui::CalcTextSize(openFolderText);
    const char* folderIcon = (const char*)u8"\uE8D5"; // uE8D5 is a better Folder icon than uE838 which might be a document
    iconSize = ImGui::CalcTextSize(folderIcon);
    totalWidth = iconSize.x + 10.0f + textSize.x;
    drawList->AddText(ImVec2(btnCenter.x - totalWidth / 2.0f, btnCenter.y - iconSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.86f, 0.86f, 0.88f, 1.0f)), folderIcon);
    drawList->AddText(ImVec2(btnCenter.x - totalWidth / 2.0f + iconSize.x + 10.0f, btnCenter.y - textSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(0.86f, 0.86f, 0.88f, 1.0f)), openFolderText);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // 3. Toggle Bypass Button (Right)
    ImGui::SetCursorPos(ImVec2(20 + btnWidth * 2 + 40, startY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (isRunning) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.08f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.05f, 0.07f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.59f, 0.16f, 0.2f, 1.0f));
        if (ImGui::Button("##ToggleBypass", ImVec2(btnWidth, 50))) {
            worker.Stop();
            SaveBypassState(false);
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.14f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.20f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.10f, 0.07f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.14f, 0.27f, 0.18f, 1.0f));
        if (ImGui::Button("##ToggleBypass", ImVec2(btnWidth, 50))) {
            worker.Start("!loopback and !impostor and ((outbound and (ip or ipv6)) or (inbound and udp and (udp.SrcPort == 53 or udp.SrcPort == 5353)))");
            SaveBypassState(true);
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    btnMin = ImGui::GetItemRectMin();
    btnMax = ImGui::GetItemRectMax();
    btnCenter = ImVec2((btnMin.x + btnMax.x) / 2.0f, (btnMin.y + btnMax.y) / 2.0f);
    const char* toggleText = isRunning ? L("Devre Dışı Bırak", "Disable", "Отключить", "Deaktivieren", "Desactivar") : L("Etkinleştir", "Enable", "Включить", "Aktivieren", "Activar");
    textSize = ImGui::CalcTextSize(toggleText);
    const char* powerIcon = (const char*)u8"\uE7E8";
    iconSize = ImGui::CalcTextSize(powerIcon);
    totalWidth = iconSize.x + 10.0f + textSize.x;
    ImVec4 rightBtnContentColor = isRunning ? ImVec4(0.86f, 0.31f, 0.35f, 1.0f) : ImVec4(0.31f, 0.78f, 0.47f, 1.0f);
    drawList->AddText(ImVec2(btnCenter.x - totalWidth / 2.0f, btnCenter.y - iconSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(rightBtnContentColor), powerIcon);
    drawList->AddText(ImVec2(btnCenter.x - totalWidth / 2.0f + iconSize.x + 10.0f, btnCenter.y - textSize.y / 2.0f), ImGui::ColorConvertFloat4ToU32(rightBtnContentColor), toggleText);

    if (showCacheClearedMsg) {
        cacheMsgTimer -= ImGui::GetIO().DeltaTime;
        if (cacheMsgTimer <= 0.0f) {
            showCacheClearedMsg = false;
        } else {
            ImGui::SetCursorPos(ImVec2(20, startY + 60.0f));
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", L("Ağ önbellekleri başarıyla temizlendi!", "Network cache cleared successfully!", "Сетевые кэши успешно очищены!", "Netzwerk-Cache erfolgreich geleert!", "¡Caché de red limpiada con éxito!"));
        }
    }

    ImGui::SetCursorPos(ImVec2(20, 575));
    ImGui::TextColored(ImVec4(0.94f, 0.78f, 0.2f, 1.0f), "SpexronDPI v1.0.0");

    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  SetWindowLongPtr(g_Hwnd, GWLP_WNDPROC, (LONG_PTR)g_OriginalWndProc);
  
  RemoveTrayIcon(g_Hwnd);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}

} // namespace SpexronDPI::GUI
