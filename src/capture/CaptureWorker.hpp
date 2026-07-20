#pragma once

#include <thread>
#include <atomic>
#include "WinDivertAdapter.hpp"

namespace SpexronDPI::Capture {

class CaptureWorker {
public:
    CaptureWorker();
    ~CaptureWorker();

    bool Start(const std::string& filter);

    void Stop();

    bool IsRunning() const { return m_isRunning; }

private:
    void RunLoop();

    WinDivertAdapter m_adapter;
    std::thread m_workerThread;
    std::atomic<bool> m_isRunning;
};

} 
