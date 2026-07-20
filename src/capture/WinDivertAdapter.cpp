#include "WinDivertAdapter.hpp"
#include "telemetry/Logger.hpp"

#if defined(_MSC_VER) || defined(_WIN32)
#include <windows.h>
#include <windivert.h>

#endif

namespace SpexronDPI::Capture {

constexpr size_t MAX_PACKET_SIZE = 65535;

WinDivertAdapter::WinDivertAdapter() : m_handle(INVALID_HANDLE_VALUE) {}

WinDivertAdapter::~WinDivertAdapter() {
    Close();
}

bool WinDivertAdapter::Open(const std::string& filter) {
    LOG_INFO("Filtre uygulanıyor: {}", filter);

    m_handle = WinDivertOpen(filter.c_str(), WINDIVERT_LAYER_NETWORK, 0, 0);

    if (m_handle == INVALID_HANDLE_VALUE) {
        LOG_CRITICAL("Sürücü hatası: Yönetici izinleri eksik.");
        return false;
    }

    return true;
}

void WinDivertAdapter::Close() {
    if (m_handle != INVALID_HANDLE_VALUE) {
        WinDivertClose(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
        LOG_INFO("Sürücü durduruldu.");
    }
}

bool WinDivertAdapter::Recv(Core::PacketContext& ctx) {
    if (m_handle == INVALID_HANDLE_VALUE) return false;

    ctx.rawData.resize(MAX_PACKET_SIZE);

    uint32_t readLength = 0;
    WINDIVERT_ADDRESS divertAddr = {};

    bool success = WinDivertRecv(m_handle, ctx.rawData.data(), static_cast<uint32_t>(ctx.rawData.size()), &readLength, &divertAddr);

    if (success && readLength > 0) {
        ctx.packetLength = readLength;
        ctx.packetId = Core::PacketContext::GlobalPacketIdCounter++;
        ctx.isOutbound = divertAddr.Outbound;
        ctx.isImpostor = divertAddr.Impostor;

        if (ctx.isImpostor) {
            LOG_WARN("{}", ctx.GetTraceLog("Recv", "N/A", "Packet seen again (Impostor bit set)"));
        } else {
            LOG_TRACE("{}", ctx.GetTraceLog("Recv", "N/A", "Paket Yakalandi"));
        }

        ctx.divertAddress.resize(sizeof(WINDIVERT_ADDRESS));
        std::memcpy(ctx.divertAddress.data(), &divertAddr, sizeof(WINDIVERT_ADDRESS));

        return true;
    }

    return false;
}

bool WinDivertAdapter::Send(Core::PacketContext& ctx) {
    if (m_handle == INVALID_HANDLE_VALUE) return false;

    if (ctx.shouldDrop) {
        std::string dropMsg = "Drop at " + std::string(ctx.dropFile ? ctx.dropFile : "Unknown") + 
                              ":" + std::to_string(ctx.dropLine) + 
                              " (" + std::string(ctx.dropCondition ? ctx.dropCondition : "No Reason") + ")";

        if (ctx.isTargetSni) {
            LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", dropMsg));
        } else {
            LOG_TRACE("{}", ctx.GetTraceLog("Send", "N/A", dropMsg));
        }
        return true; 
    }

    uint32_t sendLength = 0;
    WINDIVERT_ADDRESS divertAddr = {};

    if (ctx.divertAddress.size() == sizeof(WINDIVERT_ADDRESS)) {
        std::memcpy(&divertAddr, ctx.divertAddress.data(), sizeof(WINDIVERT_ADDRESS));
    } else {
        divertAddr.Outbound = ctx.isOutbound;
    }

    divertAddr.Impostor = 1; 

    if (ctx.isModified) {

        divertAddr.IPChecksum = 0;
        divertAddr.TCPChecksum = 0;
        divertAddr.UDPChecksum = 0;

        WinDivertHelperCalcChecksums(ctx.rawData.data(), static_cast<uint32_t>(ctx.packetLength), nullptr, 0);

        if (ctx.isTargetSni) {
            LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "Checksum recalculated"));
        }
    }

    if (ctx.isTargetSni) {
        LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", ctx.parentPacketId > 0 ? "WinDivertSend(fake) called" : "WinDivertSend(original) called"));
    } else {
        LOG_TRACE("{}", ctx.GetTraceLog("Send", "N/A", ctx.isModified ? "Modified Send" : "Pass-Through Send"));
    }

    bool sendResult = WinDivertSend(m_handle, ctx.rawData.data(), static_cast<uint32_t>(ctx.packetLength), &sendLength, &divertAddr);

    if (ctx.isTargetSni) {
        if (sendResult) {
            LOG_INFO("{}", ctx.GetTraceLog("Decision Tree", "N/A", "WinDivertSend = SUCCESS"));
        } else {
            LOG_ERROR("{}", ctx.GetTraceLog("Decision Tree", "N/A", "WinDivertSend = FAILED (Error: " + std::to_string(GetLastError()) + ")"));
        }
    }

    return sendResult;
}

} 
