using helengine.baseplatform.Builders;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

namespace helengine.wiiu.builder;

/// <summary>
/// Owns the minimal Wii U builder workspace operations for the first editor-driven RPX slice.
/// </summary>
public static class WiiUBuildWorkspace {
    /// <summary>
    /// Stable output-directory name that receives the staged packaged content tree.
    /// </summary>
    const string ContentDirectoryName = "content";

    /// <summary>
    /// Executes one Wii U build request by invoking the native build and staging the produced packaged artifacts into the requested output root.
    /// </summary>
    /// <param name="request">Resolved Wii U build request to process.</param>
    /// <param name="progressReporter">Progress reporter that receives streaming updates.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter that receives streamed failures.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the build cooperatively.</param>
    /// <param name="nativeBuildExecutor">Native build executor used to produce the RPX.</param>
    /// <returns>The final build report.</returns>
    public static PlatformBuildReport Build(
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
        if (CanWriteRuntimeSceneManifest(request.Manifest)) {
            string generatedCoreRootPath = WiiUBuilderPaths.ResolveGeneratedCoreRootPath(request);
            Directory.CreateDirectory(generatedCoreRootPath);
            new WiiURuntimeSceneManifestWriter().Write(generatedCoreRootPath, request.Manifest);
            progressReporter.Report(new PlatformBuildProgressUpdate("Generate Runtime Manifest", "wiiu-runtime-scene-manifest", 1, 4, "Generated Wii U packaged runtime scene manifest."));
        }

        progressReporter.Report(new PlatformBuildProgressUpdate("Build Native Executable", WiiUBuilderPaths.WuhbFileName, 2, 4, "Building native Wii U packaged artifacts."));
        WiiUNativeBuildResult nativeBuildResult = nativeBuildExecutor.Build(request, diagnosticReporter, cancellationToken);
        StageNativeArtifacts(request, nativeBuildResult);
        progressReporter.Report(new PlatformBuildProgressUpdate("Stage Native Artifacts", WiiUBuilderPaths.WuhbFileName, 3, 4, "Copied the native Wii U RPX and WUHB into the output root."));

        if (CanStagePackagedContent(request.Manifest)) {
            string packageSourceRootPath = WiiUBuilderPaths.ResolvePackageSourceRootPath(request);
            string stagedContentRootPath = Path.Combine(request.OutputRoot, ContentDirectoryName);
            new WiiUPackagedContentStager().Stage(request.Manifest, packageSourceRootPath, stagedContentRootPath);
        }

        progressReporter.Report(new PlatformBuildProgressUpdate("Stage Packaged Content", ContentDirectoryName, 4, 4, "Staged the packaged Wii U content tree into the output root."));

        PlatformBuildItemOutcome[] sceneOutcomes = BuildSuccessfulSceneOutcomes(request);
        PlatformBuildItemOutcome[] looseAssetOutcomes = BuildSuccessfulLooseAssetOutcomes(request);
        return new PlatformBuildReport(true, Array.Empty<PlatformBuildDiagnostic>(), sceneOutcomes, looseAssetOutcomes);
    }

    /// <summary>
    /// Returns whether the supplied manifest contains enough packaged scene metadata to emit the Wii U runtime manifest.
    /// </summary>
    /// <param name="manifest">Resolved build manifest.</param>
    /// <returns>True when the runtime scene manifest can be emitted; otherwise false.</returns>
    static bool CanWriteRuntimeSceneManifest(PlatformBuildManifest manifest) {
        return manifest != null
            && !string.IsNullOrWhiteSpace(manifest.StartupSceneId)
            && manifest.Scenes != null
            && manifest.Scenes.Length > 0;
    }

    /// <summary>
    /// Returns whether the supplied manifest references any packaged-content payloads that should be staged into the output root.
    /// </summary>
    /// <param name="manifest">Resolved build manifest.</param>
    /// <returns>True when packaged-content payloads should be staged; otherwise false.</returns>
    static bool CanStagePackagedContent(PlatformBuildManifest manifest) {
        if (manifest == null) {
            return false;
        }

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            if (manifest.Scenes[index].PayloadReferences.Length > 0) {
                return true;
            }
        }

        if (manifest.LooseAssets.Length > 0) {
            return true;
        }

        return (manifest.PlatformCookWorkItems ?? []).Length > 0;
    }

    /// <summary>
    /// Copies the native Wii U artifacts into the output root using the stable packaged artifact file names.
    /// </summary>
    /// <param name="request">Resolved build request whose output root receives the staged artifacts.</param>
    /// <param name="nativeBuildResult">Native build result that identifies the produced artifact paths.</param>
    static void StageNativeArtifacts(PlatformBuildRequest request, WiiUNativeBuildResult nativeBuildResult) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (nativeBuildResult == null) {
            throw new ArgumentNullException(nameof(nativeBuildResult));
        }

        string destinationRpxPath = Path.Combine(request.OutputRoot, WiiUBuilderPaths.RpxFileName);
        string destinationWuhbPath = Path.Combine(request.OutputRoot, WiiUBuilderPaths.WuhbFileName);
        File.Copy(nativeBuildResult.RpxPath, destinationRpxPath, true);
        File.Copy(nativeBuildResult.WuhbPath, destinationWuhbPath, true);
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
