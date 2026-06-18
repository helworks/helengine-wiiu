using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Records the produced RPX path for the first builder artifact-flow tests.
/// </summary>
public sealed class RecordingWiiUNativeBuildExecutor : IWiiUNativeBuildExecutor {
    /// <summary>
    /// Gets the last produced native artifact path.
    /// </summary>
    public string LastProducedArtifactPath { get; private set; } = string.Empty;

    /// <summary>
    /// Writes one fake RPX into the intermediate root and returns that path.
    /// </summary>
    /// <param name="request">Build request under test.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter passed through the builder contract.</param>
    /// <param name="cancellationToken">Cancellation token passed through the builder contract.</param>
    /// <returns>Absolute path to the fake RPX artifact.</returns>
    public Task<string> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        string artifactPath = Path.Combine(request.WorkingRoot, "native-build", "helengine_wiiu.rpx");
        Directory.CreateDirectory(Path.GetDirectoryName(artifactPath) ?? throw new InvalidOperationException("Artifact directory path could not be resolved."));
        File.WriteAllText(artifactPath, "fake-rpx");
        LastProducedArtifactPath = artifactPath;
        return Task.FromResult(artifactPath);
    }
}
