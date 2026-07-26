namespace helengine.wiiu.builder.tests;

/// <summary>
/// Defines the compiler-target contract required for generated Wii U shader sources.
/// </summary>
public sealed class WiiUShaderBackendTests {
    /// <summary>
    /// Ensures the shared shader model exposes Wii U as a first-class compilation target.
    /// </summary>
    [Fact]
    public void ShaderCompileTarget_includes_wii_u() {
        Assert.Equal("WiiU", ShaderCompileTarget.WiiU.ToString());
    }

    /// <summary>
    /// Ensures Wii U shader artifacts use the platform id expected by the asset and native build pipelines.
    /// </summary>
    [Fact]
    public void ShaderTargetNames_returns_wii_u_platform_id() {
        Assert.Equal("wiiu", ShaderTargetNames.GetTargetName(ShaderCompileTarget.WiiU));
    }

    /// <summary>
    /// Ensures shared StandardShader compilation can specialize source for the Wii U platform.
    /// </summary>
    [Fact]
    public void ShaderPlatformDefines_adds_wii_u_api_define() {
        ShaderDefine[] defines = ShaderPlatformDefines.BuildDefines(
            ShaderCompileTarget.WiiU,
            new ShaderModel(4, 0),
            Array.Empty<ShaderDefine>());

        Assert.Contains(defines, define => define.Name == "HEL_API_WIIU" && define.Value == "1");
    }

    /// <summary>
    /// Ensures the Wii U backend lowers shared HLSL through SPIR-V into GLSL source for CafeGLSL.
    /// </summary>
    [Fact]
    public void Compile_vertex_shader_emits_glsl_source() {
        WiiUGlslShaderBackend backend = new WiiUGlslShaderBackend();
        ShaderCompileResult result = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo("StandardShader.hlsl", "struct VertexOutput { float4 Position : SV_POSITION; }; VertexOutput VS(float3 position : POSITION) { VertexOutput output; output.Position = float4(position, 1.0); return output; }"),
                "ForwardStandardShader.vs",
                "VS",
                ShaderStage.Vertex,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "ForwardStandard",
                ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WiiU, new ShaderModel(4, 0), Array.Empty<ShaderDefine>()),
                new ShaderCompileOptions(ShaderBindingPolicies.Default, false, false, false)),
            new EmptyShaderIncludeResolver());

        string glsl = System.Text.Encoding.UTF8.GetString(result.Binary.Bytecode);
        Assert.True(result.Success);
        Assert.StartsWith("#version", glsl, StringComparison.Ordinal);
        Assert.Contains("void main", glsl, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures generated GLSL retains the stable resource names consumed by the GX2 StandardShader binding path.
    /// </summary>
    [Fact]
    public void Compile_vertex_shader_names_standard_shader_resources() {
        WiiUGlslShaderBackend backend = new WiiUGlslShaderBackend();
        ShaderCompileResult result = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo(
                    "StandardShader.hlsl",
                    "cbuffer TransformBuffer : register(b0) { float4x4 WorldViewProjection; }; struct VertexOutput { float4 Position : SV_POSITION; }; VertexOutput VS(float3 position : POSITION) { VertexOutput output; output.Position = mul(WorldViewProjection, float4(position, 1.0)); return output; }"),
                "ForwardStandardShader.vs",
                "VS",
                ShaderStage.Vertex,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "ForwardStandard",
                ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WiiU, new ShaderModel(4, 0), Array.Empty<ShaderDefine>()),
                new ShaderCompileOptions(ShaderBindingPolicies.Default, false, false, false)),
            new EmptyShaderIncludeResolver());

        string glsl = System.Text.Encoding.UTF8.GetString(result.Binary.Bytecode);
        Assert.Contains("TransformBuffer", glsl, StringComparison.Ordinal);
        Assert.Contains("Position", glsl, StringComparison.Ordinal);
    }

    /// <summary>
    /// Rejects includes because this focused compiler test uses self-contained shader source.
    /// </summary>
    sealed class EmptyShaderIncludeResolver : IShaderIncludeResolver {
        /// <summary>
        /// Fails if the compiler unexpectedly requests an include for a self-contained source.
        /// </summary>
        /// <param name="requestingFile">Source file that requested an include.</param>
        /// <param name="includePath">Referenced include path.</param>
        /// <returns>Never returns because no include is available.</returns>
        public ShaderIncludeResult Resolve(string requestingFile, string includePath) {
            throw new InvalidOperationException("The test shader does not use includes.");
        }
    }
}
