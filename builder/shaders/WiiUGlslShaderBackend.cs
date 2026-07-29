using Silk.NET.Shaderc;
using Silk.NET.SPIRV.Cross;
using System.Runtime.InteropServices;
using ShadercCompiler = Silk.NET.Shaderc.Compiler;
using SpirvCrossCompiler = Silk.NET.SPIRV.Cross.Compiler;

namespace helengine.wiiu.builder;

/// <summary>
/// Compiles shared HLSL shader sources into desktop GLSL accepted by the Wii U CafeGLSL toolchain.
/// </summary>
public sealed class WiiUGlslShaderBackend : IShaderBackend {
    /// <summary>
    /// Default name assigned to shader requests that do not provide a variant name.
    /// </summary>
    const string DefaultVariantName = "default";

    /// <summary>
    /// Describes the shader models and stages implemented by the Wii U backend.
    /// </summary>
    readonly ShaderBackendCapabilities CapabilitiesData;

    /// <summary>
    /// Initializes the Wii U shader compiler capabilities.
    /// </summary>
    public WiiUGlslShaderBackend() {
        ShaderModel shaderModel = new ShaderModel(4, 0);
        CapabilitiesData = new ShaderBackendCapabilities(shaderModel, shaderModel, new[] { ShaderStage.Vertex, ShaderStage.Pixel }, false);
    }

    /// <summary>
    /// Gets the platform shader target emitted by this backend.
    /// </summary>
    public ShaderCompileTarget Target {
        get {
            return ShaderCompileTarget.WiiU;
        }
    }

    /// <summary>
    /// Gets the shader capabilities supported by the Wii U target.
    /// </summary>
    public ShaderBackendCapabilities Capabilities {
        get {
            return CapabilitiesData;
        }
    }

    /// <summary>
    /// Compiles a shared HLSL program through SPIR-V into GLSL source.
    /// </summary>
    /// <param name="request">Shader source, stage, platform defines, and binding policy.</param>
    /// <param name="includeResolver">Resolver used for HLSL include references.</param>
    /// <returns>GLSL source and program binding metadata.</returns>
    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        if (includeResolver == null) {
            throw new ArgumentNullException(nameof(includeResolver));
        }

        ValidateRequest(request);
        byte[] spirv = CompileSpirv(request);
        byte[] glsl = CompileGlsl(spirv, request);
        ShaderProgramDefinition programDefinition = BuildProgramDefinition(request);
        ShaderCompiledBinary binary = new ShaderCompiledBinary(request.Target, request.Stage, request.EntryPoint, request.Variant, glsl);
        return new ShaderCompileResult(request, programDefinition, binary, Array.Empty<ShaderCompileDiagnostic>(), true);
    }

    /// <summary>
    /// Validates that the request is compatible with the Wii U shader target.
    /// </summary>
    /// <param name="request">Request to validate.</param>
    void ValidateRequest(ShaderCompileRequest request) {
        if (request.Target != ShaderCompileTarget.WiiU) {
            throw new InvalidOperationException("WiiUGlslShaderBackend only supports Wii U targets.");
        }

        if (request.Stage != ShaderStage.Vertex && request.Stage != ShaderStage.Pixel) {
            throw new InvalidOperationException("The Wii U backend supports vertex and pixel shaders only.");
        }

        if (request.ShaderModel.Major != 4 || request.ShaderModel.Minor != 0) {
            throw new InvalidOperationException("The Wii U backend requires shader model 4.0.");
        }
    }

    /// <summary>
    /// Compiles HLSL into SPIR-V using shaderc.
    /// </summary>
    /// <param name="request">Shader compilation request.</param>
    /// <returns>SPIR-V binary data.</returns>
    unsafe byte[] CompileSpirv(ShaderCompileRequest request) {
        Shaderc shaderc = Shaderc.GetApi();
        ShadercCompiler* compiler = shaderc.CompilerInitialize();
        CompileOptions* options = shaderc.CompileOptionsInitialize();
        if (compiler == null || options == null) {
            if (options != null) {
                shaderc.CompileOptionsRelease(options);
            }

            if (compiler != null) {
                shaderc.CompilerRelease(compiler);
            }

            throw new InvalidOperationException("Failed to initialize the HLSL compiler.");
        }

        try {
            ConfigureCompileOptions(shaderc, options, request);
            string sourcePath = string.IsNullOrWhiteSpace(request.Source.Path) ? "shader.hlsl" : request.Source.Path;
            CompilationResult* result = shaderc.CompileIntoSpv(compiler, request.Source.Source, (nuint)request.Source.Source.Length, GetShaderKind(request.Stage), sourcePath, request.EntryPoint, options);
            if (result == null) {
                throw new InvalidOperationException("The HLSL compiler returned no result.");
            }

            try {
                if (shaderc.ResultGetCompilationStatus(result) != CompilationStatus.Success) {
                    throw new InvalidOperationException($"HLSL compilation failed: {shaderc.ResultGetErrorMessageS(result)}");
                }

                int byteLength = checked((int)shaderc.ResultGetLength(result));
                if (byteLength == 0) {
                    throw new InvalidOperationException("HLSL compilation produced no SPIR-V data.");
                }

                byte[] spirv = new byte[byteLength];
                Marshal.Copy((IntPtr)shaderc.ResultGetBytes(result), spirv, 0, byteLength);
                return spirv;
            } finally {
                shaderc.ResultRelease(result);
            }
        } finally {
            shaderc.CompileOptionsRelease(options);
            shaderc.CompilerRelease(compiler);
        }
    }

    /// <summary>
    /// Converts SPIR-V data to GLSL source with SPIRV-Cross.
    /// </summary>
    /// <param name="spirv">SPIR-V binary data.</param>
    /// <param name="request">Request that determines the GLSL entry point.</param>
    /// <returns>UTF-8 GLSL source data.</returns>
    unsafe byte[] CompileGlsl(byte[] spirv, ShaderCompileRequest request) {
        if (spirv.Length % sizeof(uint) != 0) {
            throw new InvalidOperationException("SPIR-V bytecode is not aligned to 32-bit words.");
        }

        Cross cross = new Cross(Cross.CreateDefaultContext([ResolveSpirvCrossLibraryPath()]));
        Context* context = null;
        ParsedIr* parsedIr = null;
        SpirvCrossCompiler* compiler = null;
        fixed (byte* byteData = spirv) {
            uint* wordData = (uint*)byteData;
            cross.ContextCreate(&context);
            if (context == null) {
                throw new InvalidOperationException("Failed to initialize the SPIRV-Cross context.");
            }

            try {
                cross.ContextParseSpirv(context, wordData, (nuint)(spirv.Length / sizeof(uint)), &parsedIr);
                cross.ContextCreateCompiler(context, Backend.Glsl, parsedIr, CaptureMode.TakeOwnership, &compiler);
                if (compiler == null) {
                    throw new InvalidOperationException("Failed to initialize the GLSL compiler.");
                }

                CompilerOptions* options = null;
                cross.CompilerCreateCompilerOptions(compiler, &options);
                if (options == null) {
                    throw new InvalidOperationException("Failed to initialize GLSL compiler options.");
                }

                cross.CompilerOptionsSetUint(options, CompilerOption.GlslVersion, 330);
                cross.CompilerOptionsSetBool(options, CompilerOption.GlslES, 0);
                cross.CompilerInstallCompilerOptions(compiler, options);
                cross.CompilerSetEntryPoint(compiler, request.EntryPoint, GetExecutionModel(request.Stage));
                NameRuntimeResources(cross, compiler, request.Stage);
                cross.CompilerBuildCombinedImageSamplers(compiler);
                NameCombinedImageSamplerResources(cross, compiler);
                byte* source = null;
                cross.CompilerCompile(compiler, &source);
                if (source == null) {
                    throw new InvalidOperationException($"SPIRV-Cross failed to produce GLSL source: {cross.ContextGetLastErrorStringS(context)}");
                }

                string glsl = Marshal.PtrToStringUTF8((IntPtr)source);
                if (string.IsNullOrWhiteSpace(glsl)) {
                    throw new InvalidOperationException("SPIRV-Cross produced empty GLSL source.");
                }

                return System.Text.Encoding.UTF8.GetBytes(glsl);
            } finally {
                cross.ContextDestroy(context);
            }
        }
    }

    /// <summary>
    /// Configures shaderc for portable HLSL-to-SPIR-V compilation.
    /// </summary>
    /// <param name="shaderc">Shaderc API instance.</param>
    /// <param name="options">Options to configure.</param>
    /// <param name="request">Request containing compiler options and platform defines.</param>
    unsafe void ConfigureCompileOptions(Shaderc shaderc, CompileOptions* options, ShaderCompileRequest request) {
        shaderc.CompileOptionsSetTargetEnv(options, TargetEnv.Vulkan, (uint)EnvVersion.Vulkan12);
        shaderc.CompileOptionsSetSourceLanguage(options, SourceLanguage.Hlsl);
        shaderc.CompileOptionsSetHlslIoMapping(options, true);
        shaderc.CompileOptionsSetAutoMapLocations(options, true);
        shaderc.CompileOptionsSetAutoBindUniforms(options, true);
        shaderc.CompileOptionsSetPreserveBindings(options, true);
        shaderc.CompileOptionsSetOptimizationLevel(options, request.Options.Optimize ? OptimizationLevel.Performance : OptimizationLevel.Zero);

        IReadOnlyList<ShaderDefine> defines = request.Defines;
        for (int i = 0; i < defines.Count; i++) {
            ShaderDefine define = defines[i];
            string name = define.Name ?? string.Empty;
            string value = define.Value ?? string.Empty;
            shaderc.CompileOptionsAddMacroDefinition(options, name, (nuint)name.Length, value, (nuint)value.Length);
        }
    }

    /// <summary>
    /// Converts engine shader stages into shaderc stage identifiers.
    /// </summary>
    /// <param name="stage">Engine shader stage.</param>
    /// <returns>Corresponding shaderc stage.</returns>
    ShaderKind GetShaderKind(ShaderStage stage) {
        return stage == ShaderStage.Vertex ? ShaderKind.VertexShader : ShaderKind.FragmentShader;
    }

    /// <summary>
    /// Converts engine shader stages into SPIR-V execution models.
    /// </summary>
    /// <param name="stage">Engine shader stage.</param>
    /// <returns>Corresponding SPIR-V execution model.</returns>
    Silk.NET.SPIRV.ExecutionModel GetExecutionModel(ShaderStage stage) {
        return stage == ShaderStage.Vertex ? Silk.NET.SPIRV.ExecutionModel.Vertex : Silk.NET.SPIRV.ExecutionModel.Fragment;
    }

    /// <summary>
    /// Builds package metadata from the authored HLSL source and requested variant.
    /// </summary>
    /// <param name="request">Request used to create the metadata.</param>
    /// <returns>Program definition for asset serialization.</returns>
    ShaderProgramDefinition BuildProgramDefinition(ShaderCompileRequest request) {
        ShaderBinding[] bindings = HlslShaderBindingParser.ParseBindings(request.Source.Source, request.Options.BindingPolicy, request.Defines);
        string variantName = string.IsNullOrWhiteSpace(request.Variant) ? DefaultVariantName : request.Variant;
        string[] defineValues = new string[request.Defines.Count];
        for (int i = 0; i < request.Defines.Count; i++) {
            ShaderDefine define = request.Defines[i];
            defineValues[i] = string.IsNullOrWhiteSpace(define.Value) ? define.Name : string.Concat(define.Name, "=", define.Value);
        }

        return new ShaderProgramDefinition(request.ProgramName, request.Stage, request.EntryPoint, bindings, Array.Empty<ShaderVertexElement>(), Array.Empty<ShaderVertexElement>(), new[] { new ShaderVariant(variantName, defineValues) });
    }

    /// <summary>
    /// Assigns stable names to the reflected resources that the GX2 presenter binds at runtime.
    /// </summary>
    /// <param name="cross">SPIRV-Cross API used to inspect the compiled program.</param>
    /// <param name="compiler">SPIRV-Cross compiler containing the parsed program.</param>
    /// <param name="stage">Stage whose vertex-input resources should be named.</param>
    unsafe void NameRuntimeResources(Cross cross, SpirvCrossCompiler* compiler, ShaderStage stage) {
        Resources* resources = null;
        cross.CompilerCreateShaderResources(compiler, &resources);
        if (resources == null) {
            throw new InvalidOperationException("SPIRV-Cross did not expose shader resources for the Wii U runtime contract.");
        }

        NameUniformBufferResources(cross, compiler, resources);
        if (stage == ShaderStage.Vertex) {
            NameVertexInputResources(cross, compiler, resources);
            NameStageInterfaceResources(cross, compiler, resources, ResourceType.StageOutput);
        } else {
            NameStageInterfaceResources(cross, compiler, resources, ResourceType.StageInput);
        }
    }

    /// <summary>
    /// Assigns stable texture names after SPIRV-Cross combines separate HLSL textures and samplers into GLSL sampler resources.
    /// </summary>
    /// <param name="cross">SPIRV-Cross API used to inspect the combined GLSL resources.</param>
    /// <param name="compiler">SPIRV-Cross compiler containing the combined program.</param>
    unsafe void NameCombinedImageSamplerResources(Cross cross, SpirvCrossCompiler* compiler) {
        CombinedImageSampler* combinedImageSamplers = null;
        nuint combinedImageSamplerCount = 0;
        cross.CompilerGetCombinedImageSamplers(compiler, &combinedImageSamplers, &combinedImageSamplerCount);
        for (nuint samplerIndex = 0; samplerIndex < combinedImageSamplerCount; samplerIndex++) {
            CombinedImageSampler combinedImageSampler = combinedImageSamplers[samplerIndex];
            uint binding = cross.CompilerGetDecoration(compiler, combinedImageSampler.ImageId, Silk.NET.SPIRV.Decoration.Binding);
            cross.CompilerSetDecoration(compiler, combinedImageSampler.CombinedId, Silk.NET.SPIRV.Decoration.Binding, binding);
            string resourceName = GetTextureResourceName(binding);
            if (!string.IsNullOrWhiteSpace(resourceName)) {
                cross.CompilerSetName(compiler, combinedImageSampler.CombinedId, resourceName);
            }
        }
    }

    /// <summary>
    /// Assigns deterministic names to the shared StandardShader uniform buffers by their HLSL binding slots.
    /// </summary>
    /// <param name="cross">SPIRV-Cross API used to inspect the compiled program.</param>
    /// <param name="compiler">SPIRV-Cross compiler containing the parsed program.</param>
    /// <param name="resources">Reflected shader resources.</param>
    unsafe void NameUniformBufferResources(Cross cross, SpirvCrossCompiler* compiler, Resources* resources) {
        ReflectedResource* reflectedResources = null;
        nuint resourceCount = 0;
        cross.ResourcesGetResourceListForType(resources, ResourceType.UniformBuffer, &reflectedResources, &resourceCount);
        for (nuint resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
            ReflectedResource resource = reflectedResources[resourceIndex];
            uint binding = cross.CompilerGetDecoration(compiler, resource.Id, Silk.NET.SPIRV.Decoration.Binding);
            string resourceName = GetUniformBufferName(binding);
            if (!string.IsNullOrWhiteSpace(resourceName)) {
                cross.CompilerSetName(compiler, resource.Id, resourceName);
            }
        }
    }

    /// <summary>
    /// Assigns deterministic names to StandardShader vertex inputs by their engine vertex-layout locations.
    /// </summary>
    /// <param name="cross">SPIRV-Cross API used to inspect the compiled program.</param>
    /// <param name="compiler">SPIRV-Cross compiler containing the parsed program.</param>
    /// <param name="resources">Reflected shader resources.</param>
    unsafe void NameVertexInputResources(Cross cross, SpirvCrossCompiler* compiler, Resources* resources) {
        ReflectedResource* reflectedResources = null;
        nuint resourceCount = 0;
        cross.ResourcesGetResourceListForType(resources, ResourceType.StageInput, &reflectedResources, &resourceCount);
        for (nuint resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
            ReflectedResource resource = reflectedResources[resourceIndex];
            uint location = cross.CompilerGetDecoration(compiler, resource.Id, Silk.NET.SPIRV.Decoration.Location);
            string resourceName = GetVertexInputName(location);
            if (!string.IsNullOrWhiteSpace(resourceName)) {
                cross.CompilerSetName(compiler, resource.Id, resourceName);
            }
        }
    }

    /// <summary>
    /// Assigns matching names to separately compiled vertex outputs and pixel inputs so the CafeGLSL linker connects StandardShader varyings by location.
    /// </summary>
    /// <param name="cross">SPIRV-Cross API used to inspect the compiled program.</param>
    /// <param name="compiler">SPIRV-Cross compiler containing the parsed program.</param>
    /// <param name="resources">Reflected shader resources.</param>
    /// <param name="resourceType">Vertex-output or pixel-input resource type whose names should be stabilized.</param>
    unsafe void NameStageInterfaceResources(Cross cross, SpirvCrossCompiler* compiler, Resources* resources, ResourceType resourceType) {
        ReflectedResource* reflectedResources = null;
        nuint resourceCount = 0;
        cross.ResourcesGetResourceListForType(resources, resourceType, &reflectedResources, &resourceCount);
        for (nuint resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
            ReflectedResource resource = reflectedResources[resourceIndex];
            uint location = cross.CompilerGetDecoration(compiler, resource.Id, Silk.NET.SPIRV.Decoration.Location);
            string resourceName = GetStageInterfaceName(location);
            if (!string.IsNullOrWhiteSpace(resourceName)) {
                cross.CompilerSetName(compiler, resource.Id, resourceName);
            }
        }
    }

    /// <summary>
    /// Resolves the runtime uniform-block name for one StandardShader HLSL binding slot.
    /// </summary>
    /// <param name="binding">HLSL constant-buffer binding slot.</param>
    /// <returns>Stable GX2 uniform-block name, or an empty string for a resource outside the runtime contract.</returns>
    string GetUniformBufferName(uint binding) {
        return binding switch {
            0U => "TransformBuffer",
            1U => "ForwardLightBuffer",
            2U => "ShadowBuffer",
            3U => "BaseColorBuffer",
            4U => "RoughnessBuffer",
            5U => "MetallicBuffer",
            6U => "SpecularBuffer",
            7U => "EmissiveBuffer",
            _ => string.Empty
        };
    }

    /// <summary>
    /// Resolves the runtime texture name for one shared StandardShader texture binding slot.
    /// </summary>
    /// <param name="binding">HLSL texture binding slot.</param>
    /// <returns>Stable GX2 sampler name, or an empty string for a resource outside the runtime contract.</returns>
    string GetTextureResourceName(uint binding) {
        return binding switch {
            0U => "DiffuseTexture",
            1U => "shadowAtlasTexture",
            2U => "pointShadowTexture0",
            3U => "pointShadowTexture1",
            4U => "pointShadowTexture2",
            5U => "pointShadowTexture3",
            6U => "RoughnessTexture",
            7U => "EmissiveTexture",
            _ => string.Empty
        };
    }

    /// <summary>
    /// Resolves the runtime vertex-attribute name for one StandardShader input location.
    /// </summary>
    /// <param name="location">Vertex-input location emitted by shaderc.</param>
    /// <returns>Stable GX2 attribute name, or an empty string for a resource outside the runtime contract.</returns>
    string GetVertexInputName(uint location) {
        return location switch {
            0U => "Position",
            1U => "Normal",
            2U => "TexCoord",
            _ => string.Empty
        };
    }

    /// <summary>
    /// Resolves the stable cross-stage varying name for one StandardShader interface location.
    /// </summary>
    /// <param name="location">Shared vertex-output and pixel-input location emitted by shaderc.</param>
    /// <returns>Stable varying name, or an empty string for a location outside the StandardShader interface.</returns>
    string GetStageInterfaceName(uint location) {
        return location switch {
            0U => "WorldPosition",
            1U => "WorldNormal",
            2U => "TextureCoordinate",
            _ => string.Empty
        };
    }

    /// <summary>
    /// Resolves the packaged SPIRV-Cross native library for the current Windows builder process.
    /// </summary>
    /// <returns>Absolute path to the native SPIRV-Cross library.</returns>
    string ResolveSpirvCrossLibraryPath() {
        string assemblyDirectoryPath = Path.GetDirectoryName(typeof(WiiUGlslShaderBackend).Assembly.Location)
            ?? throw new InvalidOperationException("Unable to resolve the Wii U builder assembly directory.");
        string libraryPath = Path.Combine(assemblyDirectoryPath, "runtimes", "win-x64", "native", "spirv-cross.dll");
        if (!File.Exists(libraryPath)) {
            throw new FileNotFoundException("The packaged SPIRV-Cross native library was not found.", libraryPath);
        }

        return libraryPath;
    }
}
