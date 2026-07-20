#pragma once

#include "capture/CaptureWorker.hpp"

namespace SpexronDPI::GUI {

class Dashboard {
public:
    static int Run(Capture::CaptureWorker& worker, bool startHidden = false);
};

} 
