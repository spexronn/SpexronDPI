#include "Statistics.hpp"
#include "Logger.hpp"

namespace SpexronDPI::Telemetry {

std::atomic<uint64_t> Statistics::s_totalPackets{0};
std::atomic<uint64_t> Statistics::s_bypassedPackets{0};
std::atomic<uint64_t> Statistics::s_droppedPackets{0};

void Statistics::IncrementTotalPackets() { 
    s_totalPackets.fetch_add(1, std::memory_order_relaxed); 
}

void Statistics::IncrementBypassedPackets() { 
    s_bypassedPackets.fetch_add(1, std::memory_order_relaxed); 
}

void Statistics::IncrementDroppedPackets() { 
    s_droppedPackets.fetch_add(1, std::memory_order_relaxed); 
}

void Statistics::PrintSummary() {
    LOG_INFO("--- İstatistikler ---");
    LOG_INFO("İşlenen Toplam Paket: {}", s_totalPackets.load(std::memory_order_relaxed));
    LOG_INFO("Filtrelenen (Bypass): {}", s_bypassedPackets.load(std::memory_order_relaxed));
    LOG_INFO("Düşürülen: {}", s_droppedPackets.load(std::memory_order_relaxed));
    LOG_INFO("---------------------");
}

} 
