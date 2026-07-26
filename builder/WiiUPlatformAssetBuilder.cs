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
public sealed class WiiUPlatformAssetBuilder : IPlatformAssetBuilder, IPlatformShaderArtifactBuilder {
    /// <summary>
    /// Native build executor used by the first Wii U editor-build slice.
    /// </summary>
    readonly IWiiUNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Material cooker that translates authored Wii U material schemas into platform-owned cooked payloads.
    /// </summary>
    readonly WiiUMaterialCooker MaterialCooker;

    /// <summary>
    /// Compiles shared shader sources into Wii U GLSL artifacts.
    /// </summary>
    readonly WiiUShaderArtifactCooker ShaderArtifactCooker;

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
        MaterialCooker = new WiiUMaterialCooker();
        ShaderArtifactCooker = new WiiUShaderArtifactCooker();
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
    /// Translates one Wii U material schema request into the current cooked payload contract.
    /// </summary>
    /// <param name="request">Material translation request for the Wii U builder.</param>
    /// <returns>Minimal cooked material payload plus referenced shader dependencies.</returns>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        return MaterialCooker.Cook(request);
    }

    /// <summary>
    /// Compiles the shader source supplied by the editor into Wii U GLSL artifacts.
    /// </summary>
    /// <param name="request">Cook-root, dependency, and authored-source data.</param>
    /// <returns>Declarations for the generated GLSL artifacts.</returns>
    public PlatformShaderArtifactCookResult CookShaderArtifacts(PlatformShaderArtifactCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        return ShaderArtifactCooker.Cook(request);
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

        return Task.FromResult(WiiUBuildWorkspace.Build(request, progressReporter, diagnosticReporter, cancellationToken, NativeBuildExecutor));
    }
}
