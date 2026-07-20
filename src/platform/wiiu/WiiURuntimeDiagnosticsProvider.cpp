#include "platform/wiiu/WiiURuntimeDiagnosticsProvider.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "RuntimeMemoryDiagnosticsSnapshot.hpp"

#include <cstdarg>
#include <cstdio>

namespace helengine::wiiu {
    namespace {
        constexpr const char* RuntimeTracePaths[] = {
            "sd:/wiiu_runtime_trace.txt",
            "wiiu_runtime_trace.txt"
        };
    }

    /// Creates one Wii U runtime diagnostics provider with no additional state beyond the trace sinks.
    WiiURuntimeDiagnosticsProvider::WiiURuntimeDiagnosticsProvider() {
    }

    /// Releases the Wii U runtime diagnostics provider after the generated core no longer references it.
    WiiURuntimeDiagnosticsProvider::~WiiURuntimeDiagnosticsProvider() {
    }

    /// Captures an empty runtime memory snapshot so the generated diagnostics service can keep its existing contract without fabricating host-specific counters.
    RuntimeMemoryDiagnosticsSnapshot* WiiURuntimeDiagnosticsProvider::CaptureSnapshot() {
        return new RuntimeMemoryDiagnosticsSnapshot();
    }

    /// Appends the current generated-core update stage into the persistent Wii U runtime trace.
    void WiiURuntimeDiagnosticsProvider::ReportUpdateStage(std::string stage) {
#if defined(HELENGINE_WIIU_RUNTIME_STAGE_TRACE)
        AppendRuntimeTrace("[WiiUFile] Managed update stage: %s\n", stage.c_str());
#else
        (void)stage;
#endif
    }

    /// Appends one host-readable Wii U runtime trace line to every supported trace sink.
    void WiiURuntimeDiagnosticsProvider::AppendRuntimeTrace(const char* format, ...) {
        char buffer[2048];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);

        for (const char* runtimeTracePath : RuntimeTracePaths) {
            std::FILE* file = std::fopen(runtimeTracePath, "ab");
            if (file == nullptr) {
                continue;
            }

            std::fputs(buffer, file);
            std::fflush(file);
            std::fclose(file);
        }
    }
}

#endif
