#include "CaptureWorker.hpp"
#include "telemetry/Logger.hpp"
#include "parsers/NetworkParser.hpp"
#include "engine/PacketProcessor.hpp"

namespace SpexronDPI::Capture {

CaptureWorker::CaptureWorker() : m_isRunning(false) {}

CaptureWorker::~CaptureWorker() {
    Stop();
}

bool CaptureWorker::Start(const std::string& filter) {
    if (m_isRunning) return false;

    if (!m_adapter.Open(filter)) {
        return false;
    }

    m_isRunning = true;
    m_workerThread = std::thread(&CaptureWorker::RunLoop, this);

    return true;
}

void CaptureWorker::Stop() {
    if (m_isRunning) {
        m_isRunning = false;
        m_adapter.Close(); 

        if (m_workerThread.joinable()) {
            m_workerThread.join(); 
        }
        LOG_INFO("CaptureWorker Thread başarıyla durduruldu.");
    }
}

void CaptureWorker::RunLoop() {
    while (m_isRunning) {
        Core::PacketContext ctx;

        if (m_adapter.Recv(ctx)) {

            if (Parsers::NetworkParser::Parse(ctx)) {

                Engine::PacketProcessor::Process(ctx);

                for (auto& extraData : ctx.injectedPackets) {
                    Core::PacketContext extraCtx;
                    extraCtx.rawData = extraData;
                    extraCtx.packetLength = extraData.size();
                    extraCtx.divertAddress = ctx.divertAddress; 
                    extraCtx.isModified = true; 

                    extraCtx.packetId = Core::PacketContext::GlobalPacketIdCounter++;
                    extraCtx.parentPacketId = ctx.packetId;

                    Parsers::NetworkParser::Parse(extraCtx);
                    LOG_TRACE("{}", extraCtx.GetTraceLog("Inject", "TCP Segmentation", "Segment Injected"));

                    m_adapter.Send(extraCtx);
                }

                if (!ctx.shouldDrop) {
                    m_adapter.Send(ctx);
                }
            } else {

                m_adapter.Send(ctx);
            }
        }
    }
}

} 
