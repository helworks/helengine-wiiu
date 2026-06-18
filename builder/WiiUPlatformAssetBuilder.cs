using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.wiiu.builder;

/// <summary>
/// Implements the Wii U platform asset builder contract consumed by the shared editor build graph.
/// </summary>
public sealed class WiiUPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Native build executor used by the first Wii U editor-build slice.
    /// </summary>
    readonly IWiiUNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Initializes one Wii U builder instance with the current platform metadata.
    /// </summary>
    public WiiUPlatformAssetBuilder()
        : this(new WiiUDockerNativeBuildExecutor()) {
    }

    /// <summary>
    /// Initializes one Wii U builder instance with an explicit native build executor.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used by the Wii U builder workspace.</param>
    public WiiUPlatformAssetBuilder(IWiiUNativeBuildExecutor nativeBuildExecutor) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.wiiu.builder",
            "1.0.0",
            "wiiu",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 2),
            ["wiiu"],
            ["wiiu-default"]);
        Definition = WiiUPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the explicit builder descriptor for the Wii U builder assembly.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets the typed Wii U platform definition exposed to the editor.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// The first Wii U slice does not implement material cooking yet.
    /// </summary>
    /// <param name="request">Material translation request for the Wii U builder.</param>
    /// <returns>No result because Wii U material cooking is not implemented in this slice.</returns>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        throw new InvalidOperationException("The first Wii U slice does not add Wii U material cooking yet.");
    }

    /// <summary>
    /// Executes one Wii U build request through the minimal RPX build workspace.
    /// </summary>
    /// <param name="request">The resolved build request.</param>
    /// <param name="progressReporter">The progress reporter.</param>
    /// <param name="diagnosticReporter">The diagnostic reporter.</param>
    /// <param name="cancellationToken">The cancellation token.</param>
    /// <returns>The final build report.</returns>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        }

        return WiiUBuildWorkspace.BuildAsync(request, progressReporter, diagnosticReporter, cancellationToken, NativeBuildExecutor);
    }
}
