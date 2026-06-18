using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Records streamed diagnostics for Wii U builder tests.
/// </summary>
public sealed class RecordingDiagnosticReporter : IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Gets the recorded diagnostics.
    /// </summary>
    public List<PlatformBuildDiagnostic> Diagnostics { get; } = [];

    /// <summary>
    /// Records one diagnostic emitted by the builder.
    /// </summary>
    /// <param name="diagnostic">The emitted diagnostic.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
        Diagnostics.Add(diagnostic);
    }
}
