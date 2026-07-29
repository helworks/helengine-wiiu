using helengine;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies schema-specific Wii U material cooking and the compatibility contract for legacy payloads.
/// </summary>
public sealed class WiiUMaterialCookerTests {
    /// <summary>
    /// Ensures authored StandardShader surface parameters, colors, flags, and shader dependency survive cooking.
    /// </summary>
    [Fact]
    public void Cook_standard_shader_preserves_authored_parameters() {
        PlatformMaterialCookRequest request = CreateRequest(
            WiiUMaterialSchemaIds.StandardShaderSchemaId,
            new Dictionary<string, string> {
                [WiiUMaterialSchemaIds.TextureIdFieldId] = "cooked/textures/test.hasset",
                [WiiUMaterialSchemaIds.BaseColorFieldId] = "#804020FF",
                [WiiUMaterialSchemaIds.RoughnessFieldId] = "0.25",
                [WiiUMaterialSchemaIds.MetallicFieldId] = "0.75",
                [WiiUMaterialSchemaIds.SpecularFieldId] = "0.60",
                [WiiUMaterialSchemaIds.EmissiveColorFieldId] = "#FFD54A33",
                [WiiUMaterialSchemaIds.DoubleSidedFieldId] = "true",
                [WiiUMaterialSchemaIds.LightingModeFieldId] = "lit"
            });

        PlatformMaterialCookResult result = new WiiUMaterialCooker().Cook(request);
        WiiUStandardShaderMaterialAsset asset = new WiiUStandardShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);

        Assert.Equal(0.25f, asset.Roughness);
        Assert.Equal(0.75f, asset.Metallic);
        Assert.Equal(0.60f, asset.Specular);
        Assert.Equal((byte)255, asset.EmissiveColorR);
        Assert.Equal((byte)213, asset.EmissiveColorG);
        Assert.Equal((byte)74, asset.EmissiveColorB);
        Assert.Equal((byte)51, asset.EmissiveColorA);
        Assert.True(asset.Lit);
        Assert.True(asset.DoubleSided);
        Assert.Equal("ForwardStandardShader", Assert.Single(result.ReferencedShaderAssetIds));
    }

    /// <summary>
    /// Ensures omitted StandardShader surface fields use the same defaults as the Windows material path.
    /// </summary>
    [Fact]
    public void Cook_standard_shader_uses_windows_compatible_defaults() {
        PlatformMaterialCookResult result = new WiiUMaterialCooker().Cook(
            CreateRequest(WiiUMaterialSchemaIds.StandardShaderSchemaId, new Dictionary<string, string>()));
        WiiUStandardShaderMaterialAsset asset = new WiiUStandardShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);

        Assert.Equal(0.4f, asset.Roughness);
        Assert.Equal(0f, asset.Metallic);
        Assert.Equal(0.5f, asset.Specular);
        Assert.Equal((byte)255, asset.EmissiveColorR);
        Assert.Equal((byte)255, asset.EmissiveColorG);
        Assert.Equal((byte)255, asset.EmissiveColorB);
        Assert.Equal((byte)0, asset.EmissiveColorA);
    }

    /// <summary>
    /// Ensures authored StandardShader scalars below the supported range are clamped to zero in the cooked payload.
    /// </summary>
    [Fact]
    public void Cook_standard_shader_clamps_below_zero_scalars_to_zero() {
        PlatformMaterialCookResult result = new WiiUMaterialCooker().Cook(
            CreateRequest(
                WiiUMaterialSchemaIds.StandardShaderSchemaId,
                new Dictionary<string, string> {
                    [WiiUMaterialSchemaIds.RoughnessFieldId] = "-0.25",
                    [WiiUMaterialSchemaIds.MetallicFieldId] = "-1.0",
                    [WiiUMaterialSchemaIds.SpecularFieldId] = "-42.0"
                }));
        WiiUStandardShaderMaterialAsset asset = new WiiUStandardShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);

        Assert.Equal(0f, asset.Roughness);
        Assert.Equal(0f, asset.Metallic);
        Assert.Equal(0f, asset.Specular);
    }

    /// <summary>
    /// Ensures authored StandardShader scalars above the supported range are clamped to one in the cooked payload.
    /// </summary>
    [Fact]
    public void Cook_standard_shader_clamps_above_one_scalars_to_one() {
        PlatformMaterialCookResult result = new WiiUMaterialCooker().Cook(
            CreateRequest(
                WiiUMaterialSchemaIds.StandardShaderSchemaId,
                new Dictionary<string, string> {
                    [WiiUMaterialSchemaIds.RoughnessFieldId] = "1.25",
                    [WiiUMaterialSchemaIds.MetallicFieldId] = "2.0",
                    [WiiUMaterialSchemaIds.SpecularFieldId] = "42.0"
                }));
        WiiUStandardShaderMaterialAsset asset = new WiiUStandardShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);

        Assert.Equal(1f, asset.Roughness);
        Assert.Equal(1f, asset.Metallic);
        Assert.Equal(1f, asset.Specular);
    }

    /// <summary>
    /// Ensures malformed or non-finite StandardShader parameters fail material cooking instead of entering the runtime payload.
    /// </summary>
    /// <param name="fieldId">Authored field whose invalid value should be rejected.</param>
    /// <param name="value">Invalid authored value supplied to the cooker.</param>
    [Theory]
    [InlineData("roughness", "not-a-number")]
    [InlineData("metallic", "NaN")]
    [InlineData("specular", "Infinity")]
    [InlineData("emissive-color", "orange")]
    public void Cook_standard_shader_rejects_invalid_authored_parameters(string fieldId, string value) {
        Dictionary<string, string> fields = new() { [fieldId] = value };

        Assert.Throws<InvalidOperationException>(() => new WiiUMaterialCooker().Cook(
            CreateRequest(WiiUMaterialSchemaIds.StandardShaderSchemaId, fields)));
    }

    /// <summary>
    /// Ensures the legacy Wii U schema remains encoded as the shared platform material asset contract.
    /// </summary>
    [Fact]
    public void Cook_legacy_wiiu_schema_keeps_platform_material_asset_payload() {
        PlatformMaterialCookResult result = new WiiUMaterialCooker().Cook(
            CreateRequest(
                WiiUMaterialSchemaIds.StandardTexturedSchemaId,
                new Dictionary<string, string> {
                    [WiiUMaterialSchemaIds.TextureRelativePathFieldId] = "cooked/textures/legacy-test.hasset",
                    [WiiUMaterialSchemaIds.DoubleSidedFieldId] = "true",
                    [WiiUMaterialSchemaIds.VertexColorModeFieldId] = "ignore",
                    [WiiUMaterialSchemaIds.BaseColorFieldId] = "#10203040",
                    [WiiUMaterialSchemaIds.LightingModeFieldId] = "unlit"
                }));

        Asset decoded = global::helengine.files.AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes);
        PlatformMaterialAsset asset = Assert.IsType<PlatformMaterialAsset>(decoded);

        Assert.Equal("wiiu-material-test", asset.Id);
        Assert.Equal("wiiu-default", asset.RendererFamilyId);
        Assert.Equal("cooked/textures/legacy-test.hasset", asset.TextureRelativePath);
        Assert.True(asset.DoubleSided);
        Assert.False(asset.UseVertexColor);
        Assert.False(asset.Lit);
        Assert.Equal((byte)16, asset.BaseColorR);
        Assert.Equal((byte)32, asset.BaseColorG);
        Assert.Equal((byte)48, asset.BaseColorB);
        Assert.Equal((byte)64, asset.BaseColorA);
        Assert.Equal("ForwardStandardShader", Assert.Single(result.ReferencedShaderAssetIds));
    }

    /// <summary>
    /// Creates a valid Wii U material request for the supplied schema and authored field values.
    /// </summary>
    /// <param name="schemaId">Material schema that selects the cooker payload contract.</param>
    /// <param name="fields">Authored material field values presented to the cooker.</param>
    /// <returns>A request configured for the default Wii U graphics and cooking profiles.</returns>
    static PlatformMaterialCookRequest CreateRequest(
        string schemaId,
        IReadOnlyDictionary<string, string> fields) {
        return new PlatformMaterialCookRequest(
            "wiiu-material-test",
            "Materials/tests/wiiu-material-test.helmat",
            "wiiu",
            "wiiu-default",
            "wiiu-default",
            schemaId,
            fields);
    }
}
