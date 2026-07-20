#pragma once

#include <string>
#include <vector>
#include "core/PacketContext.hpp"

using WinDivertHandle = void*;

namespace SpexronDPI::Capture {

class WinDivertAdapter {
public:
    WinDivertAdapter();
    ~WinDivertAdapter();

    bool Open(const std::string& filter);
    void Close();

    bool Recv(Core::PacketContext& ctx);

    bool Send(Core::PacketContext& ctx);

private:
    WinDivertHandle m_handle;
};

} 
