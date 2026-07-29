namespace helengine.wiiu.builder.tests;

/// <summary>
/// Defines the compiler-target contract required for generated Wii U shader sources.
/// </summary>
public sealed class WiiUShaderBackendTests {
    /// <summary>
    /// Ensures separately compiled Wii U shader stages expose identical varying names so CafeGLSL links world position, normal, and texture coordinates correctly.
    /// </summary>
    [Fact]
    public void Compile_standard_shader_stages_name_matching_varyings() {
        const string source = "struct VS_IN { float3 Position : POSITION; float3 Normal : NORMAL; float2 TexCoord : TEXCOORD0; }; struct PS_IN { float4 Position : SV_POSITION; float3 WorldPosition : TEXCOORD0; float3 WorldNormal : TEXCOORD1; float2 TextureCoordinate : TEXCOORD2; }; PS_IN VS(VS_IN input) { PS_IN output; output.Position = float4(input.Position, 1.0); output.WorldPosition = input.Position; output.WorldNormal = input.Normal; output.TextureCoordinate = input.TexCoord; return output; } float4 PS(PS_IN input) : SV_Target { return float4(input.WorldPosition + input.WorldNormal + float3(input.TextureCoordinate, 0.0), 1.0); }";
        WiiUGlslShaderBackend backend = new WiiUGlslShaderBackend();
        ShaderDefine[] defines = ShaderPlatformDefines.BuildDefines(
            ShaderCompileTarget.WiiU,
            new ShaderModel(4, 0),
            Array.Empty<ShaderDefine>());
        ShaderCompileOptions options = new ShaderCompileOptions(ShaderBindingPolicies.Default, false, false, false);
        ShaderCompileResult vertexResult = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo("StandardShader.hlsl", source),
                "ForwardStandard.vs",
                "VS",
                ShaderStage.Vertex,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "ForwardStandard",
                defines,
                options),
            new EmptyShaderIncludeResolver());
        ShaderCompileResult pixelResult = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo("StandardShader.hlsl", source),
                "ForwardStandard.ps",
                "PS",
                ShaderStage.Pixel,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "ForwardStandard",
                defines,
                options),
            new EmptyShaderIncludeResolver());

        string vertexGlsl = System.Text.Encoding.UTF8.GetString(vertexResult.Binary.Bytecode);
        string pixelGlsl = System.Text.Encoding.UTF8.GetString(pixelResult.Binary.Bytecode);
        Assert.Contains("out vec3 WorldPosition;", vertexGlsl, StringComparison.Ordinal);
        Assert.Contains("out vec3 WorldNormal;", vertexGlsl, StringComparison.Ordinal);
        Assert.Contains("out vec2 TextureCoordinate;", vertexGlsl, StringComparison.Ordinal);
        Assert.Contains("in vec3 WorldPosition;", pixelGlsl, StringComparison.Ordinal);
        Assert.Contains("in vec3 WorldNormal;", pixelGlsl, StringComparison.Ordinal);
        Assert.Contains("in vec2 TextureCoordinate;", pixelGlsl, StringComparison.Ordinal);
    }

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
    /// Ensures generated GLSL retains deterministic texture names after SPIR-V image and sampler resources are combined.
    /// </summary>
    [Fact]
    public void Compile_pixel_shader_names_standard_shader_texture_resources() {
        WiiUGlslShaderBackend backend = new WiiUGlslShaderBackend();
        ShaderCompileResult result = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo(
                    "StandardShader.hlsl",
                    "Texture2D shadowAtlasTexture : register(t1); SamplerState shadowAtlasSampler : register(s1); TextureCube pointShadowTexture0 : register(t2); TextureCube pointShadowTexture1 : register(t3); TextureCube pointShadowTexture2 : register(t4); TextureCube pointShadowTexture3 : register(t5); SamplerState pointShadowSampler : register(s2); Texture2D DiffuseTexture : register(t0); SamplerState DiffuseTextureSampler : register(s0); Texture2D EmissiveTexture : register(t7); SamplerState EmissiveTextureSampler : register(s7); Texture2D RoughnessTexture : register(t6); SamplerState RoughnessTextureSampler : register(s6); float4 PS(float4 position : SV_POSITION) : SV_Target { return DiffuseTexture.Sample(DiffuseTextureSampler, float2(0.5, 0.5)) + shadowAtlasTexture.Sample(shadowAtlasSampler, float2(0.5, 0.5)) + float4(pointShadowTexture0.Sample(pointShadowSampler, float3(0.0, 0.0, 1.0)).r + pointShadowTexture1.Sample(pointShadowSampler, float3(0.0, 0.0, 1.0)).r + pointShadowTexture2.Sample(pointShadowSampler, float3(0.0, 0.0, 1.0)).r + pointShadowTexture3.Sample(pointShadowSampler, float3(0.0, 0.0, 1.0)).r, 0.0, 0.0, 0.0) + EmissiveTexture.Sample(EmissiveTextureSampler, float2(0.5, 0.5)) + RoughnessTexture.Sample(RoughnessTextureSampler, float2(0.5, 0.5)); }"),
                "ForwardStandardShader.ps",
                "PS",
                ShaderStage.Pixel,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "ForwardStandard",
                ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WiiU, new ShaderModel(4, 0), Array.Empty<ShaderDefine>()),
                new ShaderCompileOptions(ShaderBindingPolicies.Default, false, false, false)),
            new EmptyShaderIncludeResolver());

        string glsl = System.Text.Encoding.UTF8.GetString(result.Binary.Bytecode);
        Assert.True(result.Success);
        Assert.Contains("DiffuseTexture", glsl, StringComparison.Ordinal);
        Assert.Contains("layout(binding = 0) uniform sampler2D DiffuseTexture;", glsl, StringComparison.Ordinal);
        Assert.Contains("layout(binding = 1) uniform sampler2D shadowAtlasTexture;", glsl, StringComparison.Ordinal);
        Assert.Contains("layout(binding = 6) uniform sampler2D RoughnessTexture;", glsl, StringComparison.Ordinal);
        Assert.Contains("layout(binding = 7) uniform sampler2D EmissiveTexture;", glsl, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures request-provided variant defines reach shaderc before Wii U GLSL is generated.
    /// </summary>
    [Fact]
    public void Compile_pixel_shader_applies_variant_define() {
        WiiUGlslShaderBackend backend = new WiiUGlslShaderBackend();
        ShaderCompileResult result = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo("VariantProbe.hlsl", "float4 PS(float4 position : SV_POSITION) : SV_Target {\n#if defined(WIIU_VARIANT_PROBE)\nreturn float4(0.25, 0.5, 0.75, 1.0);\n#else\nreturn float4(0.0, 0.0, 0.0, 1.0);\n#endif\n}"),
                "VariantProbe.ps",
                "PS",
                ShaderStage.Pixel,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "VariantProbe",
                ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WiiU, new ShaderModel(4, 0), [new ShaderDefine("WIIU_VARIANT_PROBE", "1")]),
                new ShaderCompileOptions(ShaderBindingPolicies.Default, false, false, false)),
            new EmptyShaderIncludeResolver());

        string glsl = System.Text.Encoding.UTF8.GetString(result.Binary.Bytecode);
        Assert.Contains("0.25", glsl, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures an output diagnostic can retain the uniform block needed by the Wii U presenter.
    /// </summary>
    [Fact]
    public void Compile_pixel_shader_diagnostic_retains_referenced_uniform_block() {
        WiiUGlslShaderBackend backend = new WiiUGlslShaderBackend();
        ShaderCompileResult result = backend.Compile(
            new ShaderCompileRequest(
                new ShaderSourceInfo("DiagnosticProbe.hlsl", "cbuffer ForwardLightBuffer : register(b0) { float4 lightColor; }; float4 PS(float4 position : SV_POSITION) : SV_Target { float4 litColor = lightColor; return float4(1.0, 0.0, 1.0, 1.0) + (litColor * 0.000001); }"),
                "DiagnosticProbe.ps",
                "PS",
                ShaderStage.Pixel,
                ShaderCompileTarget.WiiU,
                new ShaderModel(4, 0),
                "DiagnosticProbe",
                ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WiiU, new ShaderModel(4, 0), Array.Empty<ShaderDefine>()),
                new ShaderCompileOptions(ShaderBindingPolicies.Default, false, false, false)),
            new EmptyShaderIncludeResolver());

        string glsl = System.Text.Encoding.UTF8.GetString(result.Binary.Bytecode);
        Assert.True(result.Success);
        Assert.Contains("ForwardLightBuffer", glsl, StringComparison.Ordinal);
        Assert.Contains("vec4(1.0, 0.0, 1.0, 1.0)", glsl, StringComparison.Ordinal);
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
