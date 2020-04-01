#pragma once

#include "SML\util\Logging.h"
#include <iostream>

using namespace std;
using namespace SML::Logging;

namespace ImGui {
    class ImGuiLogger
    {
    public:
        static void LogInfo(const char message[]);
    private:
        static const char prefix[];
    };

    const char ImGuiLogger::prefix[] = "[ImGuiMod] ";

    void ImGuiLogger::LogInfo(const char message[]) {

        std::string buffer(ImGuiLogger::prefix);
        buffer.append(message);
        buffer.append("\n");

        cout << buffer.c_str();
        info(buffer.c_str());
    }
}