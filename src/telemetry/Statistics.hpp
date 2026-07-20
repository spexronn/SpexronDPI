#pragma once
#include <atomic>
#include <cstdint>

namespace SpexronDPI::Telemetry {

class Statistics {
public:
    static void IncrementTotalPackets();
    static void IncrementBypassedPackets();
    static void IncrementDroppedPackets();
    static void PrintSummary();

    static uint64_t GetTotalPackets() { return s_totalPackets.load(); }
    static uint64_t GetBypassedPackets() { return s_bypassedPackets.load(); }
    static uint64_t GetDroppedPackets() { return s_droppedPackets.load(); }

private:
    static std::atomic<uint64_t> s_totalPackets;
    static std::atomic<uint64_t> s_bypassedPackets;
    static std::atomic<uint64_t> s_droppedPackets;
};

} 
