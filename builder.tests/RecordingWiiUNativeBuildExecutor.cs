using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Records the produced packaged artifact paths for the Wii U builder artifact-flow tests.
/// </summary>
public sealed class RecordingWiiUNativeBuildExecutor : IWiiUNativeBuildExecutor {
    /// <summary>
    /// Gets the last produced native RPX artifact path.
    /// </summary>
    public string LastProducedArtifactPath { get; private set; } = string.Empty;

    /// <summary>
    /// Gets the last produced native WUHB bundle path.
    /// </summary>
    public string LastProducedBundlePath { get; private set; } = string.Empty;

    /// <summary>
    /// Writes fake native Wii U packaged artifacts into the intermediate root and returns their paths.
    /// </summary>
    /// <param name="request">Build request under test.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter passed through the builder contract.</param>
    /// <param name="cancellationToken">Cancellation token passed through the builder contract.</param>
    /// <returns>Absolute paths to the fake RPX and WUHB artifacts.</returns>
    public WiiUNativeBuildResult Build(
        PlatformBuildRequest request,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        string artifactPath = Path.Combine(request.WorkingRoot, "native-build", "helengine_wiiu.rpx");
        string bundlePath = Path.Combine(request.WorkingRoot, "native-build", "helengine_wiiu.wuhb");
        Directory.CreateDirectory(Path.GetDirectoryName(artifactPath) ?? throw new InvalidOperationException("Artifact directory path could not be resolved."));
        File.WriteAllText(artifactPath, "fake-rpx");
        File.WriteAllText(bundlePath, "fake-wuhb");
        LastProducedArtifactPath = artifactPath;
        LastProducedBundlePath = bundlePath;
        return new WiiUNativeBuildResult(artifactPath, bundlePath);
    }
}
