// Windows / Linux: no system capture yet (route the output into an input device instead).
#include "SystemAudio.h"

namespace cosmos
{
struct SystemAudio::Impl {};
SystemAudio::SystemAudio() {}
SystemAudio::~SystemAudio() {}
bool SystemAudio::isSupported() { return false; }
bool SystemAudio::isRunning() const { return false; }
void SystemAudio::stop() {}
std::string SystemAudio::start (Callback) { return "System audio capture is only available on macOS 14.2 or newer for now."; }
}
