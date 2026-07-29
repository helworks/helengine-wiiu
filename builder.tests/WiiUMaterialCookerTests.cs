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
            CreateRequest(WiiUMaterialSchemaIds.StandardTexturedSchemaId, new Dictionary<string, string>()));

        Asset decoded = global::helengine.files.AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes);

        Assert.IsType<PlatformMaterialAsset>(decoded);
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
