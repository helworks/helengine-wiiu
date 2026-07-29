using System.Text;

namespace helengine.wiiu.builder;

/// <summary>
/// Serializes and deserializes the versioned little-endian Wii U StandardShader material contract.
/// </summary>
public sealed class WiiUStandardShaderMaterialBinarySerializer {
    /// <summary>
    /// Identifies the initial Wii U StandardShader material payload layout.
    /// </summary>
    public const uint Version = 1u;

    /// <summary>
    /// Gets the stable four-byte signature that identifies Wii U StandardShader material payloads.
    /// </summary>
    public static ReadOnlySpan<byte> Magic => "WUMT"u8;

    /// <summary>
    /// Serializes one validated Wii U StandardShader material asset in the runtime contract's exact field order.
    /// </summary>
    /// <param name="asset">Material asset whose identities and shader values should be encoded.</param>
    /// <returns>Little-endian bytes ready to store as a cooked Wii U material payload.</returns>
    public byte[] Serialize(WiiUStandardShaderMaterialAsset asset) {
        ValidateAsset(asset);

        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.UTF8, leaveOpen: true);
        writer.Write(Magic);
        writer.Write(Version);
        WriteString(writer, asset.MaterialAssetId);
        WriteString(writer, asset.ShaderAssetId);
        WriteString(writer, asset.VertexProgramName);
        WriteString(writer, asset.PixelProgramName);
        WriteString(writer, asset.VariantName);
        WriteString(writer, asset.DiffuseTextureAssetId);
        writer.Write(asset.BaseColorR);
        writer.Write(asset.BaseColorG);
        writer.Write(asset.BaseColorB);
        writer.Write(asset.BaseColorA);
        writer.Write(asset.Roughness);
        writer.Write(asset.Metallic);
        writer.Write(asset.Specular);
        writer.Write(asset.EmissiveColorR);
        writer.Write(asset.EmissiveColorG);
        writer.Write(asset.EmissiveColorB);
        writer.Write(asset.EmissiveColorA);
        writer.Write(asset.Lit ? (byte)1 : (byte)0);
        writer.Write(asset.DoubleSided ? (byte)1 : (byte)0);
        return stream.ToArray();
    }

    /// <summary>
    /// Deserializes one complete Wii U StandardShader material payload and rejects unsupported or malformed input.
    /// </summary>
    /// <param name="bytes">Serialized payload bytes beginning with the Wii U material signature and version.</param>
    /// <returns>The validated material asset represented by the supplied bytes.</returns>
    public WiiUStandardShaderMaterialAsset Deserialize(byte[] bytes) {
        if (bytes == null) {
            throw new ArgumentNullException(nameof(bytes));
        }

        try {
            using MemoryStream stream = new(bytes, writable: false);
            using BinaryReader reader = new(stream, Encoding.UTF8, leaveOpen: true);
            ValidateMagic(reader);

            uint version = reader.ReadUInt32();
            if (version != Version) {
                throw new InvalidOperationException($"Unsupported Wii U StandardShader material payload version '{version}'.");
            }

            WiiUStandardShaderMaterialAsset asset = new() {
                MaterialAssetId = ReadString(reader),
                ShaderAssetId = ReadString(reader),
                VertexProgramName = ReadString(reader),
                PixelProgramName = ReadString(reader),
                VariantName = ReadString(reader),
                DiffuseTextureAssetId = ReadString(reader),
                BaseColorR = reader.ReadByte(),
                BaseColorG = reader.ReadByte(),
                BaseColorB = reader.ReadByte(),
                BaseColorA = reader.ReadByte(),
                Roughness = reader.ReadSingle(),
                Metallic = reader.ReadSingle(),
                Specular = reader.ReadSingle(),
                EmissiveColorR = reader.ReadByte(),
                EmissiveColorG = reader.ReadByte(),
                EmissiveColorB = reader.ReadByte(),
                EmissiveColorA = reader.ReadByte(),
                Lit = ReadBoolean(reader),
                DoubleSided = ReadBoolean(reader)
            };
            if (stream.Position != stream.Length) {
                throw new InvalidOperationException("Wii U StandardShader material payload version one does not permit trailing bytes.");
            }

            ValidateAsset(asset);
            return asset;
        } catch (EndOfStreamException exception) {
            throw new InvalidOperationException("The Wii U StandardShader material payload is truncated.", exception);
        }
    }

    /// <summary>
    /// Writes one required UTF-8 value preceded by its signed 32-bit byte length.
    /// </summary>
    /// <param name="writer">Binary writer positioned at the string field in the payload.</param>
    /// <param name="value">Validated string value to encode without substitution.</param>
    static void WriteString(BinaryWriter writer, string value) {
        byte[] bytes = Encoding.UTF8.GetBytes(value);
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    /// <summary>
    /// Reads one UTF-8 value whose signed 32-bit prefix declares the exact byte count to consume.
    /// </summary>
    /// <param name="reader">Binary reader positioned at a length-prefixed string field.</param>
    /// <returns>The decoded string after its complete declared byte sequence has been read.</returns>
    static string ReadString(BinaryReader reader) {
        int byteCount = reader.ReadInt32();
        if (byteCount < 0) {
            throw new InvalidOperationException("Wii U StandardShader material string lengths cannot be negative.");
        }

        long remainingByteCount = reader.BaseStream.Length - reader.BaseStream.Position;
        if (byteCount > remainingByteCount) {
            throw new InvalidOperationException($"Wii U StandardShader material string length '{byteCount}' exceeds the remaining payload size '{remainingByteCount}'.");
        }

        byte[] bytes = reader.ReadBytes(byteCount);
        if (bytes.Length != byteCount) {
            throw new EndOfStreamException("The Wii U StandardShader material string is truncated.");
        }

        return Encoding.UTF8.GetString(bytes);
    }

    /// <summary>
    /// Reads one canonical Boolean byte and rejects numeric encodings other than zero and one.
    /// </summary>
    /// <param name="reader">Binary reader positioned at a Boolean field.</param>
    /// <returns>False for zero or true for one.</returns>
    static bool ReadBoolean(BinaryReader reader) {
        byte value = reader.ReadByte();
        if (value > 1) {
            throw new InvalidOperationException($"Wii U StandardShader material Boolean byte '{value}' is invalid.");
        }

        return value == 1;
    }

    /// <summary>
    /// Verifies that a payload begins with the complete Wii U StandardShader material signature.
    /// </summary>
    /// <param name="reader">Binary reader positioned at the first payload byte.</param>
    static void ValidateMagic(BinaryReader reader) {
        byte[] magic = reader.ReadBytes(Magic.Length);
        if (magic.Length != Magic.Length) {
            throw new EndOfStreamException("The Wii U StandardShader material signature is truncated.");
        } else if (!magic.AsSpan().SequenceEqual(Magic)) {
            throw new InvalidOperationException("The supplied payload is not a Wii U StandardShader material asset.");
        }
    }

    /// <summary>
    /// Validates every required material identity and normalized scalar before bytes enter or leave the contract boundary.
    /// </summary>
    /// <param name="asset">Material asset whose contract invariants should be enforced.</param>
    static void ValidateAsset(WiiUStandardShaderMaterialAsset asset) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        } else if (string.IsNullOrWhiteSpace(asset.MaterialAssetId)) {
            throw new InvalidOperationException("Wii U StandardShader materials require one material asset id.");
        } else if (string.IsNullOrWhiteSpace(asset.ShaderAssetId)) {
            throw new InvalidOperationException("Wii U StandardShader materials require one shader asset id.");
        } else if (string.IsNullOrWhiteSpace(asset.VertexProgramName)) {
            throw new InvalidOperationException("Wii U StandardShader materials require one vertex-program name.");
        } else if (string.IsNullOrWhiteSpace(asset.PixelProgramName)) {
            throw new InvalidOperationException("Wii U StandardShader materials require one pixel-program name.");
        } else if (string.IsNullOrWhiteSpace(asset.VariantName)) {
            throw new InvalidOperationException("Wii U StandardShader materials require one shader variant name.");
        } else if (asset.DiffuseTextureAssetId == null) {
            throw new InvalidOperationException("Wii U StandardShader materials require a non-null diffuse texture asset id.");
        }

        ValidateScalar(asset.Roughness, nameof(asset.Roughness));
        ValidateScalar(asset.Metallic, nameof(asset.Metallic));
        ValidateScalar(asset.Specular, nameof(asset.Specular));
    }

    /// <summary>
    /// Verifies that one material scalar is finite and belongs to the inclusive normalized range from zero through one.
    /// </summary>
    /// <param name="value">Scalar value to validate.</param>
    /// <param name="propertyName">Material property name used to identify an invalid value.</param>
    static void ValidateScalar(float value, string propertyName) {
        if (!float.IsFinite(value) || value < 0f || value > 1f) {
            throw new InvalidOperationException($"Wii U StandardShader material scalar '{propertyName}' must be finite and between zero and one inclusive.");
        }
    }
}
