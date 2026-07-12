#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <string>

#include "IRuntimeDiagnosticsProvider.hpp"
#include "IRuntimeUpdateStageDiagnosticsProvider.hpp"

class RuntimeMemoryDiagnosticsSnapshot;

namespace helengine::wiiu {
    /// Bridges generated-core runtime diagnostics callbacks into the persistent Wii U trace file so host-side crash repros can be correlated with managed update stages.
    class WiiURuntimeDiagnosticsProvider final : public IRuntimeDiagnosticsProvider, public IRuntimeUpdateStageDiagnosticsProvider {
    public:
        /// Creates one Wii U runtime diagnostics provider with no additional state beyond the trace sinks.
        WiiURuntimeDiagnosticsProvider();

        /// Releases the Wii U runtime diagnostics provider after the generated core no longer references it.
        ~WiiURuntimeDiagnosticsProvider();

        /// Captures an empty runtime memory snapshot so the generated diagnostics service can keep its existing contract without fabricating host-specific counters.
        RuntimeMemoryDiagnosticsSnapshot* CaptureSnapshot() override;

        /// Appends the current generated-core update stage into the persistent Wii U runtime trace.
        void ReportUpdateStage(std::string stage) override;

    private:
        /// Appends one host-readable Wii U runtime trace line to every supported trace sink.
        void AppendRuntimeTrace(const char* format, ...);
    };
}

#endif
