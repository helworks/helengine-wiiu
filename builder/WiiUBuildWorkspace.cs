using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

namespace helengine.wiiu.builder;

/// <summary>
/// Owns the minimal Wii U builder workspace operations for the first editor-driven RPX slice.
/// </summary>
public static class WiiUBuildWorkspace {
    /// <summary>
    /// Executes one Wii U build request by invoking the native build and copying the produced RPX into the requested output root.
    /// </summary>
    /// <param name="request">Resolved Wii U build request to process.</param>
    /// <param name="progressReporter">Progress reporter that receives streaming updates.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter that receives streamed failures.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the build cooperatively.</param>
    /// <param name="nativeBuildExecutor">Native build executor used to produce the RPX.</param>
    /// <returns>The final build report.</returns>
    public static async Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken,
        IWiiUNativeBuildExecutor nativeBuildExecutor) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        } else if (nativeBuildExecutor == null) {
            throw new ArgumentNullException(nameof(nativeBuildExecutor));
        }

        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(request.WorkingRoot);

        progressReporter.Report(new PlatformBuildProgressUpdate("Build Native Executable", WiiUBuilderPaths.RpxFileName, 1, 2, "Building native Wii U RPX."));
        string producedArtifactPath = await nativeBuildExecutor.BuildAsync(request, diagnosticReporter, cancellationToken);
        string destinationArtifactPath = Path.Combine(request.OutputRoot, WiiUBuilderPaths.RpxFileName);
        File.Copy(producedArtifactPath, destinationArtifactPath, true);
        progressReporter.Report(new PlatformBuildProgressUpdate("Stage Native Artifact", WiiUBuilderPaths.RpxFileName, 2, 2, "Copied the native Wii U RPX into the output root."));

        PlatformBuildItemOutcome[] sceneOutcomes = BuildSuccessfulSceneOutcomes(request);
        PlatformBuildItemOutcome[] looseAssetOutcomes = BuildSuccessfulLooseAssetOutcomes(request);
        return new PlatformBuildReport(true, Array.Empty<PlatformBuildDiagnostic>(), sceneOutcomes, looseAssetOutcomes);
    }

    /// <summary>
    /// Builds the successful scene outcomes for the supplied request.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <returns>Successful outcomes for all scenes in the request manifest.</returns>
    static PlatformBuildItemOutcome[] BuildSuccessfulSceneOutcomes(PlatformBuildRequest request) {
        return request.Manifest.Scenes
            .Select(scene => new PlatformBuildItemOutcome(scene.SceneId, PlatformBuildItemOutcomeKind.Succeeded))
            .ToArray();
    }

    /// <summary>
    /// Builds the successful loose-asset outcomes for the supplied request.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <returns>Successful outcomes for all loose assets in the request manifest.</returns>
    static PlatformBuildItemOutcome[] BuildSuccessfulLooseAssetOutcomes(PlatformBuildRequest request) {
        return request.Manifest.LooseAssets
            .Select(asset => new PlatformBuildItemOutcome(asset.AssetId, PlatformBuildItemOutcomeKind.Succeeded))
            .ToArray();
    }
}
