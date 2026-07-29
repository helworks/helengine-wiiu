using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies the versioned Wii U StandardShader material payload shared by the builder and native runtime.
/// </summary>
public sealed class WiiUStandardShaderMaterialBinarySerializerTests {
    /// <summary>
    /// Ensures the serializer and deserializer independently conform to the exact version-one little-endian wire layout.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_ConformsToGoldenWireFormat() {
        byte[] expectedPayload = [
            0x57, 0x55, 0x4D, 0x54,
            0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00, 0xC3, 0xA9,
            0x01, 0x00, 0x00, 0x00, 0x73,
            0x01, 0x00, 0x00, 0x00, 0x76,
            0x01, 0x00, 0x00, 0x00, 0x70,
            0x01, 0x00, 0x00, 0x00, 0x78,
            0x00, 0x00, 0x00, 0x00,
            0x01, 0x02, 0x03, 0x04,
            0x00, 0x00, 0x00, 0x3F,
            0x00, 0x00, 0x80, 0x3E,
            0x00, 0x00, 0x80, 0x3F,
            0x05, 0x06, 0x07, 0x08,
            0x01, 0x00
        ];
        WiiUStandardShaderMaterialAsset asset = new() {
            MaterialAssetId = "\u00E9",
            ShaderAssetId = "s",
            VertexProgramName = "v",
            PixelProgramName = "p",
            VariantName = "x",
            DiffuseTextureAssetId = string.Empty,
            BaseColorR = 1,
            BaseColorG = 2,
            BaseColorB = 3,
            BaseColorA = 4,
            Roughness = 0.5f,
            Metallic = 0.25f,
            Specular = 1f,
            EmissiveColorR = 5,
            EmissiveColorG = 6,
            EmissiveColorB = 7,
            EmissiveColorA = 8,
            Lit = true,
            DoubleSided = false
        };
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        byte[] serializedPayload = serializer.Serialize(asset);
        WiiUStandardShaderMaterialAsset decodedAsset = serializer.Deserialize(expectedPayload);

        Assert.Equal(expectedPayload, serializedPayload);
        Assert.Equal("\u00E9", decodedAsset.MaterialAssetId);
        Assert.Equal("s", decodedAsset.ShaderAssetId);
        Assert.Equal("v", decodedAsset.VertexProgramName);
        Assert.Equal("p", decodedAsset.PixelProgramName);
        Assert.Equal("x", decodedAsset.VariantName);
        Assert.Equal(string.Empty, decodedAsset.DiffuseTextureAssetId);
        Assert.Equal(1, decodedAsset.BaseColorR);
        Assert.Equal(2, decodedAsset.BaseColorG);
        Assert.Equal(3, decodedAsset.BaseColorB);
        Assert.Equal(4, decodedAsset.BaseColorA);
        Assert.Equal(0.5f, decodedAsset.Roughness);
        Assert.Equal(0.25f, decodedAsset.Metallic);
        Assert.Equal(1f, decodedAsset.Specular);
        Assert.Equal(5, decodedAsset.EmissiveColorR);
        Assert.Equal(6, decodedAsset.EmissiveColorG);
        Assert.Equal(7, decodedAsset.EmissiveColorB);
        Assert.Equal(8, decodedAsset.EmissiveColorA);
        Assert.True(decodedAsset.Lit);
        Assert.False(decodedAsset.DoubleSided);
    }

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
    /// Ensures a string whose declared byte length exceeds its available bytes is rejected as a truncated field.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsTruncatedLengthPrefixedString() {
        byte[] payload = [
            0x57, 0x55, 0x4D, 0x54,
            0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0x6D
        ];
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(payload));
    }

    /// <summary>
    /// Ensures a hostile maximum string length is rejected from stream bounds before any large allocation is attempted.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsHostileStringLengthBeforeAllocation() {
        byte[] payload = [
            0x57, 0x55, 0x4D, 0x54,
            0x01, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0xFF, 0x7F
        ];
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(payload));
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
    /// Ensures version one rejects trailing data instead of silently accepting an extended or ambiguous layout.
    /// </summary>
    [Fact]
    public void Deserialize_RejectsTrailingBytes() {
        WiiUStandardShaderMaterialBinarySerializer serializer = new();
        byte[] payload = serializer.Serialize(CreateValidAsset());
        byte[] extendedPayload = [.. payload, 0];

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(extendedPayload));
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
