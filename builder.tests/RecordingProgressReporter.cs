using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Records streamed progress updates for Wii U builder tests.
/// </summary>
public sealed class RecordingProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>
    /// Gets the recorded progress updates.
    /// </summary>
    public List<PlatformBuildProgressUpdate> Updates { get; } = [];

    /// <summary>
    /// Records one progress update emitted by the builder.
    /// </summary>
    /// <param name="update">The emitted progress update.</param>
    public void Report(PlatformBuildProgressUpdate update) {
        Updates.Add(update);
    }
}
