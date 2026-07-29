using helengine.baseplatform.Manifest;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.wiiu.builder;

/// <summary>
/// Converts editor-resolved HLSL source into the GLSL files consumed by the Wii U native shader build.
/// </summary>
public sealed class WiiUShaderArtifactCooker {
    /// <summary>
    /// Compiles every requested program pair from source into stable GLSL artifact files.
    /// </summary>
    /// <param name="request">Cook request containing shader source and selected program pairs.</param>
    /// <returns>Declarations for every generated GLSL file.</returns>
    public PlatformShaderArtifactCookResult Cook(PlatformShaderArtifactCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (!string.Equals(request.PlatformId, "wiiu", StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"Wii U cannot cook shaders for platform '{request.PlatformId}'.");
        } else if (request.ShaderSources.Count == 0 && request.ShaderDependencies.Count != 0) {
            throw new InvalidOperationException("Wii U shader cooking requires resolved authored source text.");
        }

        List<PlatformCookedArtifactDeclaration> declarations = [];
        for (int index = 0; index < request.ShaderDependencies.Count; index++) {
            helengine.baseplatform.Results.PlatformShaderDependency dependency = request.ShaderDependencies[index];
            if (!dependency.HasProgramPair) {
                throw new InvalidOperationException($"Wii U shader dependency '{dependency.ShaderAssetId}' does not select a program pair.");
            }

            PlatformShaderArtifactCookSource source = FindSource(request.ShaderSources, dependency.ShaderAssetId);
            CookDependencyVariants(request, source, dependency, declarations);
        }

        return new PlatformShaderArtifactCookResult(declarations);
    }

    /// <summary>
    /// Expands the shared StandardShader source into every Wii U forward and directional-shadow pass.
    /// </summary>
    /// <param name="request">Cook output context.</param>
    /// <param name="source">Shared authored HLSL source.</param>
    /// <param name="dependency">Material-selected program names.</param>
    /// <param name="declarations">Destination declarations.</param>
    void CookDependencyVariants(PlatformShaderArtifactCookRequest request, PlatformShaderArtifactCookSource source, helengine.baseplatform.Results.PlatformShaderDependency dependency, List<PlatformCookedArtifactDeclaration> declarations) {
        string[] variants = string.Equals(dependency.ShaderAssetId, "ForwardStandardShader", StringComparison.Ordinal)
            ? ["ForwardStandard", "ForwardStandardShadowed", "ShadowDepth"]
            : [dependency.VariantName];
        for (int index = 0; index < variants.Length; index++) {
            helengine.baseplatform.Results.PlatformShaderDependency variantDependency = new(dependency.ShaderAssetId, dependency.VertexProgramName, dependency.PixelProgramName, variants[index]);
            CookStage(request, source, variantDependency, ShaderStage.Vertex, variantDependency.VertexProgramName, "VS", declarations);
            string pixelEntryPoint = string.Equals(variantDependency.VariantName, "ShadowDepth", StringComparison.Ordinal) ? "ShadowDepthPS" : "PS";
            CookStage(request, source, variantDependency, ShaderStage.Pixel, variantDependency.PixelProgramName, pixelEntryPoint, declarations);
        }
    }

    /// <summary>
    /// Locates the unique resolved source associated with one dependency.
    /// </summary>
    /// <param name="sources">Resolved sources supplied by the editor.</param>
    /// <param name="shaderAssetId">Shader asset identity to locate.</param>
    /// <returns>Matching authored shader source.</returns>
    PlatformShaderArtifactCookSource FindSource(IReadOnlyList<PlatformShaderArtifactCookSource> sources, string shaderAssetId) {
        for (int index = 0; index < sources.Count; index++) {
            if (string.Equals(sources[index].ShaderAssetId, shaderAssetId, StringComparison.Ordinal)) {
                return sources[index];
            }
        }

        throw new InvalidOperationException($"Wii U shader source for '{shaderAssetId}' was not supplied.");
    }

    /// <summary>
    /// Compiles and writes one program stage.
    /// </summary>
    /// <param name="request">Cook output context.</param>
    /// <param name="source">Shared authored HLSL source.</param>
    /// <param name="dependency">Material-selected program pair.</param>
    /// <param name="stage">Stage being compiled.</param>
    /// <param name="programName">Material program identity.</param>
    /// <param name="entryPoint">HLSL entry point.</param>
    /// <param name="declarations">Destination declarations.</param>
    void CookStage(PlatformShaderArtifactCookRequest request, PlatformShaderArtifactCookSource source, helengine.baseplatform.Results.PlatformShaderDependency dependency, ShaderStage stage, string programName, string entryPoint, List<PlatformCookedArtifactDeclaration> declarations) {
        ShaderDefine[] defines = ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WiiU, new ShaderModel(4, 0), BuildVariantDefines(dependency.VariantName));
        ShaderCompileRequest compileRequest = new(new ShaderSourceInfo(source.ShaderAssetId + ".hlsl", source.SourceText), programName, entryPoint, stage, ShaderCompileTarget.WiiU, new ShaderModel(4, 0), dependency.ShaderAssetId, defines, new ShaderCompileOptions(ShaderBindingPolicies.Default, false, true, false));
        ShaderCompileResult result = new WiiUGlslShaderBackend().Compile(compileRequest, new WiiUShaderIncludeResolver());
        string relativePath = $"shaders/{dependency.VariantName}.{(stage == ShaderStage.Vertex ? "vs" : "ps")}";
        string fullPath = Path.Combine(request.CookRootPath, relativePath.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath) ?? throw new InvalidOperationException("Unable to create Wii U shader output directory."));
        File.WriteAllBytes(fullPath, result.Binary.Bytecode);
        declarations.Add(new PlatformCookedArtifactDeclaration(relativePath, programName, "shader", dependency.VariantName));
    }

    /// <summary>
    /// Creates compiler defines required by the selected shared StandardShader variant.
    /// </summary>
    /// <param name="variantName">Selected variant name.</param>
    /// <returns>Variant-specific compile definitions.</returns>
    ShaderDefine[] BuildVariantDefines(string variantName) {
        if (string.Equals(variantName, "ForwardStandard", StringComparison.Ordinal)) {
            return [new ShaderDefine("HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY", "1")];
        } else if (string.Equals(variantName, "ForwardStandardShadowed", StringComparison.Ordinal)) {
            return [
                new ShaderDefine("HELENGINE_STANDARD_SHADOWED", "1"),
                new ShaderDefine("HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY", "1")
            ];
        } else if (string.Equals(variantName, "ShadowDepth", StringComparison.Ordinal)) {
            return [new ShaderDefine("HELENGINE_STANDARD_SHADOW_DEPTH", "1")];
        }

        throw new InvalidOperationException($"Wii U does not support StandardShader variant '{variantName}'.");
    }
}
