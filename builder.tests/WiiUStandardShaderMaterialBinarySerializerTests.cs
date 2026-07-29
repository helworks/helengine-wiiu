using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies the versioned Wii U StandardShader material payload shared by the builder and native runtime.
/// </summary>
public sealed class WiiUStandardShaderMaterialBinarySerializerTests {
    /// <summary>
    /// Ensures every authored material value survives serialization and deserialization without substitution or loss.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_PreservesCompleteMaterialPayload() {
        WiiUStandardShaderMaterialAsset asset = CreateValidAsset();
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        byte[] payload = serializer.Serialize(asset);
        WiiUStandardShaderMaterialAsset decodedAsset = serializer.Deserialize(payload);

        Assert.Equal("materials/brick", decodedAsset.MaterialAssetId);
        Assert.Equal("ForwardStandardShader", decodedAsset.ShaderAssetId);
        Assert.Equal("ForwardStandardShader.vs", decodedAsset.VertexProgramName);
        Assert.Equal("ForwardStandardShader.ps", decodedAsset.PixelProgramName);
        Assert.Equal("ForwardStandard", decodedAsset.VariantName);
        Assert.Equal("textures/brick_diffuse", decodedAsset.DiffuseTextureAssetId);
        Assert.Equal(17, decodedAsset.BaseColorR);
        Assert.Equal(34, decodedAsset.BaseColorG);
        Assert.Equal(51, decodedAsset.BaseColorB);
        Assert.Equal(68, decodedAsset.BaseColorA);
        Assert.Equal(0.25f, decodedAsset.Roughness);
        Assert.Equal(0.5f, decodedAsset.Metallic);
        Assert.Equal(0.75f, decodedAsset.Specular);
        Assert.Equal(85, decodedAsset.EmissiveColorR);
        Assert.Equal(102, decodedAsset.EmissiveColorG);
        Assert.Equal(119, decodedAsset.EmissiveColorB);
        Assert.Equal(136, decodedAsset.EmissiveColorA);
        Assert.True(decodedAsset.Lit);
        Assert.False(decodedAsset.DoubleSided);
    }

    /// <summary>
    /// Ensures payload versions outside the builder-owned contract are rejected before material fields are decoded.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsUnsupportedVersion() {
        WiiUStandardShaderMaterialBinarySerializer serializer = new();
        byte[] payload = serializer.Serialize(CreateValidAsset());
        payload[4] = 2;

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(payload));
    }

    /// <summary>
    /// Ensures an otherwise valid payload cannot silently deserialize when its final field is missing.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsTruncatedPayload() {
        WiiUStandardShaderMaterialBinarySerializer serializer = new();
        byte[] payload = serializer.Serialize(CreateValidAsset());
        byte[] truncatedPayload = payload[..^1];

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(truncatedPayload));
    }

    /// <summary>
    /// Ensures data without the Wii U material signature is not interpreted as a StandardShader material.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsNonmatchingMagic() {
        WiiUStandardShaderMaterialBinarySerializer serializer = new();
        byte[] payload = serializer.Serialize(CreateValidAsset());
        payload[0] = (byte)'X';

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(payload));
    }

    /// <summary>
    /// Ensures boolean fields accept only their canonical zero and one byte encodings.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsInvalidBooleanByte() {
        WiiUStandardShaderMaterialBinarySerializer serializer = new();
        byte[] payload = serializer.Serialize(CreateValidAsset());
        payload[^1] = 2;

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(payload));
    }

    /// <summary>
    /// Ensures serialization refuses materials that do not identify the shader asset they require.
    /// </summary>
    [Fact]
    public void Serialize_RejectsMissingShaderIdentity() {
        WiiUStandardShaderMaterialAsset asset = CreateValidAsset();
        asset.ShaderAssetId = string.Empty;
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        Assert.Throws<InvalidOperationException>(() => serializer.Serialize(asset));
    }

    /// <summary>
    /// Creates a complete valid asset used as the real serialization input for each contract test.
    /// </summary>
    /// <returns>A populated material asset whose values exercise every serialized field.</returns>
    static WiiUStandardShaderMaterialAsset CreateValidAsset() {
        return new WiiUStandardShaderMaterialAsset {
            MaterialAssetId = "materials/brick",
            ShaderAssetId = "ForwardStandardShader",
            VertexProgramName = "ForwardStandardShader.vs",
            PixelProgramName = "ForwardStandardShader.ps",
            VariantName = "ForwardStandard",
            DiffuseTextureAssetId = "textures/brick_diffuse",
            BaseColorR = 17,
            BaseColorG = 34,
            BaseColorB = 51,
            BaseColorA = 68,
            Roughness = 0.25f,
            Metallic = 0.5f,
            Specular = 0.75f,
            EmissiveColorR = 85,
            EmissiveColorG = 102,
            EmissiveColorB = 119,
            EmissiveColorA = 136,
            Lit = true,
            DoubleSided = false
        };
    }
}
