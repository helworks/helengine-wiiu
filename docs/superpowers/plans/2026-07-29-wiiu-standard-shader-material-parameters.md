# Wii U StandardShader Material Parameters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Carry authored roughness, metallic, specular, and emissive values through Wii U material cooking and runtime upload so unshadowed DemoDisc `cube_test` matches Windows.

**Architecture:** The `standard-shader` schema receives a Wii U-owned, versioned little-endian payload modeled after Vita's compiled-shader material contract. A handwritten native reader detects that format before the existing `PlatformMaterialAsset` fallback, constructs a parameter-complete `WiiURuntimeMaterial`, and the GX2 presenter uploads those values into the already-reflected StandardShader blocks. The legacy `wiiu-standard-textured` payload, all shadow behavior, and additional material textures remain unchanged.

**Tech Stack:** C# 13/.NET 9 builder and xUnit tests, C++20 handwritten Wii U runtime, GX2/WHB, Docker/devkitPro authoritative build, Cemu manual validation.

**Execution note:** Work directly on `main`, as Helena requested. Preserve existing untracked diagnostic/build files and stage only named source, test, and documentation files.

---

### Task 1: Define and prove the Wii U StandardShader binary material contract

**Files:**
- Create: `builder/WiiUStandardShaderMaterialAsset.cs`
- Create: `builder/WiiUStandardShaderMaterialBinarySerializer.cs`
- Create: `builder.tests/WiiUStandardShaderMaterialBinarySerializerTests.cs`

- [ ] **Step 1: Write the failing serializer tests**

Create `WiiUStandardShaderMaterialBinarySerializerTests` with substantive XML comments and these six facts:

```csharp
/// <summary>
/// Verifies the versioned Wii U StandardShader material binary contract.
/// </summary>
public sealed class WiiUStandardShaderMaterialBinarySerializerTests {
    /// <summary>
    /// Ensures every authored material field survives one complete binary round trip.
    /// </summary>
    [Fact]
    public void Serialize_and_deserialize_round_trip_every_material_field() {
        WiiUStandardShaderMaterialAsset asset = new() {
            MaterialAssetId = "materials/cube.hasset",
            ShaderAssetId = "ForwardStandardShader",
            VertexProgramName = "ForwardStandardShader.vs",
            PixelProgramName = "ForwardStandardShader.ps",
            VariantName = "ForwardStandard",
            DiffuseTextureAssetId = "cooked/textures/cube.hasset",
            BaseColorR = 128,
            BaseColorG = 64,
            BaseColorB = 32,
            BaseColorA = 255,
            Roughness = 0.4f,
            Metallic = 0.2f,
            Specular = 0.7f,
            EmissiveColorR = 255,
            EmissiveColorG = 213,
            EmissiveColorB = 74,
            EmissiveColorA = 51,
            Lit = true,
            DoubleSided = false
        };
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        WiiUStandardShaderMaterialAsset decoded = serializer.Deserialize(serializer.Serialize(asset));

        Assert.Equal(asset.MaterialAssetId, decoded.MaterialAssetId);
        Assert.Equal(asset.ShaderAssetId, decoded.ShaderAssetId);
        Assert.Equal(asset.VertexProgramName, decoded.VertexProgramName);
        Assert.Equal(asset.PixelProgramName, decoded.PixelProgramName);
        Assert.Equal(asset.VariantName, decoded.VariantName);
        Assert.Equal(asset.DiffuseTextureAssetId, decoded.DiffuseTextureAssetId);
        Assert.Equal(asset.BaseColorR, decoded.BaseColorR);
        Assert.Equal(asset.BaseColorG, decoded.BaseColorG);
        Assert.Equal(asset.BaseColorB, decoded.BaseColorB);
        Assert.Equal(asset.BaseColorA, decoded.BaseColorA);
        Assert.Equal(asset.Roughness, decoded.Roughness);
        Assert.Equal(asset.Metallic, decoded.Metallic);
        Assert.Equal(asset.Specular, decoded.Specular);
        Assert.Equal(asset.EmissiveColorR, decoded.EmissiveColorR);
        Assert.Equal(asset.EmissiveColorG, decoded.EmissiveColorG);
        Assert.Equal(asset.EmissiveColorB, decoded.EmissiveColorB);
        Assert.Equal(asset.EmissiveColorA, decoded.EmissiveColorA);
        Assert.Equal(asset.Lit, decoded.Lit);
        Assert.Equal(asset.DoubleSided, decoded.DoubleSided);
    }

    /// <summary>
    /// Ensures an unsupported payload version cannot be interpreted as the current contract.
    /// </summary>
    [Fact]
    public void Deserialize_rejects_an_unsupported_version() {
        byte[] bytes = [.. WiiUStandardShaderMaterialBinarySerializer.Magic.ToArray(), 99, 0, 0, 0];
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));

        Assert.Contains("version", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Ensures truncated payloads fail instead of producing partially initialized material state.
    /// </summary>
    [Fact]
    public void Deserialize_rejects_a_truncated_payload() {
        byte[] bytes = [.. WiiUStandardShaderMaterialBinarySerializer.Magic.ToArray(), 1, 0, 0, 0, 4, 0];
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));
    }

    /// <summary>
    /// Ensures a non-Wii-U payload cannot enter the StandardShader material reader.
    /// </summary>
    [Fact]
    public void Deserialize_rejects_a_nonmatching_magic_prefix() {
        byte[] bytes = [(byte)'X', (byte)'U', (byte)'M', (byte)'T', 1, 0, 0, 0];
        WiiUStandardShaderMaterialBinarySerializer serializer = new();

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));
    }

    /// <summary>
    /// Ensures invalid Boolean encodings are rejected instead of being treated as true.
    /// </summary>
    [Fact]
    public void Deserialize_rejects_an_invalid_boolean_encoding() {
        WiiUStandardShaderMaterialAsset asset = new() {
            MaterialAssetId = "materials/cube.hasset",
            ShaderAssetId = "ForwardStandardShader",
            VertexProgramName = "ForwardStandardShader.vs",
            PixelProgramName = "ForwardStandardShader.ps",
            VariantName = "ForwardStandard",
            Roughness = 0.4f,
            Metallic = 0.0f,
            Specular = 0.5f
        };
        WiiUStandardShaderMaterialBinarySerializer serializer = new();
        byte[] bytes = serializer.Serialize(asset);
        bytes[^1] = 2;

        Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));
    }

    /// <summary>
    /// Ensures required shader identities cannot be omitted from a serialized material.
    /// </summary>
    [Fact]
    public void Serialize_rejects_a_missing_shader_identity() {
        WiiUStandardShaderMaterialAsset asset = new() {
            MaterialAssetId = "materials/cube.hasset",
            ShaderAssetId = string.Empty,
            VertexProgramName = "ForwardStandardShader.vs",
            PixelProgramName = "ForwardStandardShader.ps",
            VariantName = "ForwardStandard",
            Roughness = 0.4f,
            Metallic = 0.0f,
            Specular = 0.5f
        };

        Assert.Throws<InvalidOperationException>(() => new WiiUStandardShaderMaterialBinarySerializer().Serialize(asset));
    }
}
```

- [ ] **Step 2: Run the serializer tests and verify RED**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~WiiUStandardShaderMaterialBinarySerializerTests" -v minimal
```

Expected: compilation fails because `WiiUStandardShaderMaterialAsset` and `WiiUStandardShaderMaterialBinarySerializer` do not exist.

- [ ] **Step 3: Add the builder-owned material record**

Create one sealed class in `WiiUStandardShaderMaterialAsset.cs`. Add a substantive XML comment to the class and every property. Use these exact properties and types:

```csharp
public sealed class WiiUStandardShaderMaterialAsset {
    public string MaterialAssetId { get; set; } = string.Empty;
    public string ShaderAssetId { get; set; } = string.Empty;
    public string VertexProgramName { get; set; } = string.Empty;
    public string PixelProgramName { get; set; } = string.Empty;
    public string VariantName { get; set; } = string.Empty;
    public string DiffuseTextureAssetId { get; set; } = string.Empty;
    public byte BaseColorR { get; set; }
    public byte BaseColorG { get; set; }
    public byte BaseColorB { get; set; }
    public byte BaseColorA { get; set; }
    public float Roughness { get; set; }
    public float Metallic { get; set; }
    public float Specular { get; set; }
    public byte EmissiveColorR { get; set; }
    public byte EmissiveColorG { get; set; }
    public byte EmissiveColorB { get; set; }
    public byte EmissiveColorA { get; set; }
    public bool Lit { get; set; }
    public bool DoubleSided { get; set; }
}
```

- [ ] **Step 4: Implement the versioned serializer**

Create `WiiUStandardShaderMaterialBinarySerializer` with `using System.Text;`, magic `"WUMT"u8`, version `1u`, and the exact field order shown below. Add substantive XML comments to the class, constants, property, and every method.

```csharp
public sealed class WiiUStandardShaderMaterialBinarySerializer {
    public static ReadOnlySpan<byte> Magic => "WUMT"u8;
    public const uint Version = 1u;

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
        writer.Write(asset.Lit);
        writer.Write(asset.DoubleSided);
        return stream.ToArray();
    }

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
            ValidateAsset(asset);
            return asset;
        } catch (EndOfStreamException exception) {
            throw new InvalidOperationException("Wii U StandardShader material payload is truncated.", exception);
        }
    }
}
```

Add these class-level static helpers, each with substantive XML comments:

```csharp
static void WriteString(BinaryWriter writer, string value) {
    byte[] bytes = Encoding.UTF8.GetBytes(value);
    writer.Write(bytes.Length);
    writer.Write(bytes);
}

static string ReadString(BinaryReader reader) {
    int byteCount = reader.ReadInt32();
    if (byteCount < 0) {
        throw new InvalidOperationException("Wii U StandardShader material string lengths cannot be negative.");
    }

    byte[] bytes = reader.ReadBytes(byteCount);
    if (bytes.Length != byteCount) {
        throw new InvalidOperationException("Wii U StandardShader material payload is truncated inside a string.");
    }

    return Encoding.UTF8.GetString(bytes);
}

static bool ReadBoolean(BinaryReader reader) {
    byte encoded = reader.ReadByte();
    if (encoded > 1) {
        throw new InvalidOperationException($"Wii U StandardShader material Boolean encoding '{encoded}' is invalid.");
    }

    return encoded != 0;
}

static void ValidateMagic(BinaryReader reader) {
    byte[] magic = reader.ReadBytes(Magic.Length);
    if (magic.Length != Magic.Length || !magic.AsSpan().SequenceEqual(Magic)) {
        throw new InvalidOperationException("Payload is not a Wii U StandardShader material.");
    }
}

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
        throw new InvalidOperationException("Wii U StandardShader diffuse texture identity cannot be null.");
    }

    ValidateScalar(asset.Roughness, nameof(asset.Roughness));
    ValidateScalar(asset.Metallic, nameof(asset.Metallic));
    ValidateScalar(asset.Specular, nameof(asset.Specular));
}

static void ValidateScalar(float value, string fieldName) {
    if (!float.IsFinite(value) || value < 0f || value > 1f) {
        throw new InvalidOperationException($"Wii U StandardShader material field '{fieldName}' must be finite and between zero and one.");
    }
}
```

- [ ] **Step 5: Run the serializer tests and verify GREEN**

Run the command from Step 2.

Expected: all `WiiUStandardShaderMaterialBinarySerializerTests` pass.

- [ ] **Step 6: Commit the binary contract**

```powershell
rtk git add -- builder/WiiUStandardShaderMaterialAsset.cs builder/WiiUStandardShaderMaterialBinarySerializer.cs builder.tests/WiiUStandardShaderMaterialBinarySerializerTests.cs
rtk git commit -m "feat: define Wii U StandardShader material payload"
```

### Task 2: Cook authored StandardShader parameters and preserve the legacy schema

**Files:**
- Modify: `builder/WiiUMaterialSchemaIds.cs`
- Modify: `builder/WiiUMaterialCooker.cs`
- Create: `builder.tests/WiiUMaterialCookerTests.cs`
- Modify: `builder.tests/WiiUPlatformAssetBuilderTests.cs:52-78`

- [ ] **Step 1: Write failing cooker tests**

Create `WiiUMaterialCookerTests` with one class-level XML comment and substantive comments on every fact. Cover these exact cases:

```csharp
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

[Fact]
public void Cook_legacy_wiiu_schema_keeps_platform_material_asset_payload() {
    PlatformMaterialCookResult result = new WiiUMaterialCooker().Cook(
        CreateRequest(WiiUMaterialSchemaIds.StandardTexturedSchemaId, new Dictionary<string, string>()));

    Asset decoded = AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes);
    Assert.IsType<PlatformMaterialAsset>(decoded);
}
```

Add this class-level helper with substantive XML comments; do not create a local helper function:

```csharp
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
```

- [ ] **Step 2: Run cooker tests and verify RED**

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~WiiUMaterialCookerTests|FullyQualifiedName~CookMaterial_WithStandardShaderSchema" -v minimal
```

Expected: compilation fails because the scalar field-id constants do not exist, and the old builder test still expects the generic payload.

- [ ] **Step 3: Add shared StandardShader field identifiers**

Add substantive XML comments and these public constants to `WiiUMaterialSchemaIds`:

```csharp
public const string RoughnessFieldId = "roughness";
public const string MetallicFieldId = "metallic";
public const string SpecularFieldId = "specular";
public const string EmissiveColorFieldId = "emissive-color";
```

- [ ] **Step 4: Split StandardShader and legacy cooking paths**

In `WiiUMaterialCooker.Cook`, keep validation and shader dependency creation common, then branch on the schema:

```csharp
PlatformShaderDependency dependency = new(
    StandardShaderAssetId,
    StandardVertexProgramName,
    StandardPixelProgramName,
    StandardVariantName);
if (string.Equals(request.SchemaId, WiiUMaterialSchemaIds.StandardShaderSchemaId, StringComparison.OrdinalIgnoreCase)) {
    return PlatformMaterialCookResult.CreateWithDependencies(
        new WiiUStandardShaderMaterialBinarySerializer().Serialize(CreateStandardShaderMaterialAsset(request)),
        [dependency]);
}

return PlatformMaterialCookResult.CreateWithDependencies(
    global::helengine.files.AssetSerializer.SerializeToBytes(CreateLegacyMaterialAsset(request)),
    [dependency]);
```

Move the current `PlatformMaterialAsset` initialization into `CreateLegacyMaterialAsset`. Add `CreateStandardShaderMaterialAsset`; before constructing the record, call `ResolveBaseColor` and `ResolveEmissiveColor` into the byte variables shown below, then use the fixed shader identities and assign all fields:

```csharp
WiiUStandardShaderMaterialAsset cookedAsset = new() {
    MaterialAssetId = request.MaterialAssetId,
    ShaderAssetId = StandardShaderAssetId,
    VertexProgramName = StandardVertexProgramName,
    PixelProgramName = StandardPixelProgramName,
    VariantName = StandardVariantName,
    DiffuseTextureAssetId = ResolveTextureRelativePath(request.FieldValues),
    BaseColorR = baseColorRed,
    BaseColorG = baseColorGreen,
    BaseColorB = baseColorBlue,
    BaseColorA = baseColorAlpha,
    Roughness = ResolveScalar(request.FieldValues, WiiUMaterialSchemaIds.RoughnessFieldId, StandardMaterialRoughnessDefaults.DefaultRoughness),
    Metallic = ResolveScalar(request.FieldValues, WiiUMaterialSchemaIds.MetallicFieldId, StandardMaterialMetallicDefaults.DefaultMetallic),
    Specular = ResolveScalar(request.FieldValues, WiiUMaterialSchemaIds.SpecularFieldId, StandardMaterialSpecularDefaults.DefaultSpecular),
    EmissiveColorR = emissiveRed,
    EmissiveColorG = emissiveGreen,
    EmissiveColorB = emissiveBlue,
    EmissiveColorA = emissiveAlpha,
    Lit = ResolveLightingMode(request.FieldValues),
    DoubleSided = ResolveBoolean(request.FieldValues, WiiUMaterialSchemaIds.DoubleSidedFieldId, false)
};
```

Implement these class-level methods with substantive XML comments. Generalize the existing parser's error text from “base color” to “color” so both color fields report the root cause.

```csharp
static float ResolveScalar(
    IReadOnlyDictionary<string, string> fieldValues,
    string fieldId,
    float defaultValue) {
    if (fieldValues == null) {
        throw new ArgumentNullException(nameof(fieldValues));
    } else if (string.IsNullOrWhiteSpace(fieldId)) {
        throw new ArgumentException("Field id must be provided.", nameof(fieldId));
    } else if (!fieldValues.TryGetValue(fieldId, out string value) || string.IsNullOrWhiteSpace(value)) {
        return defaultValue;
    } else if (!double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double parsedValue)
        || !double.IsFinite(parsedValue)) {
        throw new InvalidOperationException($"Wii U StandardShader field '{fieldId}' must be a finite floating-point value.");
    }

    return (float)Math.Clamp(parsedValue, 0.0, 1.0);
}

static void ResolveEmissiveColor(
    IReadOnlyDictionary<string, string> fieldValues,
    out byte red,
    out byte green,
    out byte blue,
    out byte alpha) {
    if (fieldValues == null) {
        throw new ArgumentNullException(nameof(fieldValues));
    } else if (!fieldValues.TryGetValue(WiiUMaterialSchemaIds.EmissiveColorFieldId, out string value)
        || string.IsNullOrWhiteSpace(value)) {
        red = 255;
        green = 255;
        blue = 255;
        alpha = 0;
        return;
    }

    ParseColor(value, out red, out green, out blue, out alpha);
}
```

- [ ] **Step 5: Update the public builder test to decode the new payload**

Rename `CookMaterial_WithStandardShaderSchema_ProducesCookedPlatformMaterialAsset` to `CookMaterial_WithStandardShaderSchema_ProducesCookedStandardShaderMaterialAsset`. Decode with `WiiUStandardShaderMaterialBinarySerializer`, assert the original base color/texture/flags, and retain the shader dependency assertion.

- [ ] **Step 6: Run focused cooker tests and verify GREEN**

Run the command from Step 2.

Expected: all selected cooker and builder tests pass.

- [ ] **Step 7: Commit material cooking**

```powershell
rtk git add -- builder/WiiUMaterialSchemaIds.cs builder/WiiUMaterialCooker.cs builder.tests/WiiUMaterialCookerTests.cs builder.tests/WiiUPlatformAssetBuilderTests.cs
rtk git commit -m "feat: cook Wii U StandardShader parameters"
```

### Task 3: Read the platform-owned payload in the native runtime

**Files:**
- Create: `src/platform/wiiu/WiiUStandardShaderMaterialReader.hpp`
- Create: `src/platform/wiiu/WiiUStandardShaderMaterialReader.cpp`
- Modify: `src/platform/wiiu/WiiURuntimeMaterial.hpp:10-100`
- Modify: `src/platform/wiiu/WiiURenderManager3D.hpp:17-116`
- Modify: `src/platform/wiiu/WiiURenderManager3D.cpp:184-263,630-707`
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write a failing native material-reader source contract**

Add this fact to `WiiURuntimeSourceTests`, with a substantive XML comment:

```csharp
[Fact]
public void RuntimeSeam_ReadsVersionedStandardShaderMaterialBeforeLegacyFallback() {
    string repositoryRootPath = WiiUTestSourcePaths.FindRepositoryRootPath();
    string readerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUStandardShaderMaterialReader.hpp"));
    string readerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUStandardShaderMaterialReader.cpp"));
    string managerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));

    Assert.Contains("struct WiiUStandardShaderMaterial final", readerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static bool TryRead(::Stream* stream, WiiUStandardShaderMaterial& material);", readerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("{ 'W', 'U', 'M', 'T' }", readerSource, StringComparison.Ordinal);
    Assert.Contains("WiiUStandardShaderMaterialVersion = 1u", readerSource, StringComparison.Ordinal);
    Assert.Contains("TryReadFloat", readerSource, StringComparison.Ordinal);
    Assert.Contains("WiiUStandardShaderMaterialReader::TryRead", managerSource, StringComparison.Ordinal);
    Assert.Contains("BuildStandardShaderRuntimeMaterial", managerSource, StringComparison.Ordinal);
    Assert.Contains("AssetSerializer::Deserialize", managerSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the native reader source test and verify RED**

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_ReadsVersionedStandardShaderMaterialBeforeLegacyFallback" -v minimal
```

Expected: test fails because the reader files do not exist.

- [ ] **Step 3: Define the native decoded material record and reader interface**

Create `WiiUStandardShaderMaterialReader.hpp` under `HELENGINE_WIIU_HAS_GENERATED_CORE`. Forward-declare `Stream` and define `WiiUStandardShaderMaterial` with fields in the same semantic order as the C# asset:

```cpp
struct WiiUStandardShaderMaterial final {
    std::string MaterialAssetId;
    std::string ShaderAssetId;
    std::string VertexProgramName;
    std::string PixelProgramName;
    std::string VariantName;
    std::string DiffuseTextureAssetId;
    std::uint8_t BaseColorR;
    std::uint8_t BaseColorG;
    std::uint8_t BaseColorB;
    std::uint8_t BaseColorA;
    float Roughness;
    float Metallic;
    float Specular;
    std::uint8_t EmissiveColorR;
    std::uint8_t EmissiveColorG;
    std::uint8_t EmissiveColorB;
    std::uint8_t EmissiveColorA;
    bool Lit;
    bool DoubleSided;
};

class WiiUStandardShaderMaterialReader final {
public:
    static bool TryRead(const std::string& path, WiiUStandardShaderMaterial& material);
    static bool TryRead(::Stream* stream, WiiUStandardShaderMaterial& material);

private:
    static bool TryReadExact(::Stream* stream, std::uint8_t* destination, std::size_t byteCount);
    static bool TryReadUInt32(::Stream* stream, std::uint32_t* value);
    static bool TryReadInt32(::Stream* stream, std::int32_t* value);
    static bool TryReadFloat(::Stream* stream, float* value);
    static bool TryReadBoolean(::Stream* stream, bool* value);
    static bool TryReadString(::Stream* stream, std::string& value);
    static void Validate(const WiiUStandardShaderMaterial& material);
};
```

Add substantive `///` comments to the struct, class, every field, and every method.

- [ ] **Step 4: Implement endian-safe native decoding**

In `WiiUStandardShaderMaterialReader.cpp`, use magic `{ 'W', 'U', 'M', 'T' }` and version `1u`. `TryRead(Stream*, WiiUStandardShaderMaterial&)` must return false only for a nonmatching magic prefix; after a matching prefix, version mismatch, truncation, invalid Boolean bytes, invalid required identities, non-finite scalars, and out-of-range scalars must throw `std::runtime_error`.

Decode fields in this exact order:

```text
MaterialAssetId, ShaderAssetId, VertexProgramName, PixelProgramName, VariantName,
DiffuseTextureAssetId, BaseColorR, BaseColorG, BaseColorB, BaseColorA,
Roughness, Metallic, Specular,
EmissiveColorR, EmissiveColorG, EmissiveColorB, EmissiveColorA,
Lit, DoubleSided
```

Implement the primitive readers as follows, with substantive comments on each method:

```cpp
bool WiiUStandardShaderMaterialReader::TryReadExact(::Stream* stream, std::uint8_t* destination, std::size_t byteCount) {
    if (stream == nullptr || destination == nullptr) {
        return false;
    }

    return stream->Read(destination, 0u, byteCount) == byteCount;
}

bool WiiUStandardShaderMaterialReader::TryReadUInt32(::Stream* stream, std::uint32_t* value) {
    if (value == nullptr) {
        return false;
    }

    std::uint8_t bytes[4];
    if (!TryReadExact(stream, bytes, sizeof(bytes))) {
        return false;
    }

    *value = static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8)
        | (static_cast<std::uint32_t>(bytes[2]) << 16)
        | (static_cast<std::uint32_t>(bytes[3]) << 24);
    return true;
}

bool WiiUStandardShaderMaterialReader::TryReadInt32(::Stream* stream, std::int32_t* value) {
    if (value == nullptr) {
        return false;
    }

    std::uint32_t encoded = 0u;
    if (!TryReadUInt32(stream, &encoded)) {
        return false;
    }

    *value = static_cast<std::int32_t>(encoded);
    return true;
}

bool WiiUStandardShaderMaterialReader::TryReadFloat(::Stream* stream, float* value) {
    if (value == nullptr) {
        return false;
    }

    std::uint32_t bits = 0u;
    if (!TryReadUInt32(stream, &bits)) {
        return false;
    }

    static_assert(sizeof(bits) == sizeof(*value));
    std::memcpy(value, &bits, sizeof(bits));
    return true;
}

bool WiiUStandardShaderMaterialReader::TryReadBoolean(::Stream* stream, bool* value) {
    if (value == nullptr) {
        return false;
    }

    std::uint8_t encoded = 0u;
    if (!TryReadExact(stream, &encoded, sizeof(encoded)) || encoded > 1u) {
        return false;
    }

    *value = encoded != 0u;
    return true;
}

bool WiiUStandardShaderMaterialReader::TryReadString(::Stream* stream, std::string& value) {
    std::int32_t byteCount = 0;
    if (!TryReadInt32(stream, &byteCount)) {
        return false;
    } else if (byteCount < 0) {
        throw std::runtime_error("Wii U StandardShader material string lengths cannot be negative.");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(byteCount));
    if (byteCount > 0 && !TryReadExact(stream, bytes.data(), static_cast<std::size_t>(byteCount))) {
        return false;
    }

    if (bytes.empty()) {
        value.clear();
    } else {
        value.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    return true;
}
```

After reading the magic and version, decode the complete record with one explicit truncation check:

```cpp
WiiUStandardShaderMaterial decodedMaterial;
if (!TryReadString(stream, decodedMaterial.MaterialAssetId)
    || !TryReadString(stream, decodedMaterial.ShaderAssetId)
    || !TryReadString(stream, decodedMaterial.VertexProgramName)
    || !TryReadString(stream, decodedMaterial.PixelProgramName)
    || !TryReadString(stream, decodedMaterial.VariantName)
    || !TryReadString(stream, decodedMaterial.DiffuseTextureAssetId)
    || !TryReadExact(stream, &decodedMaterial.BaseColorR, sizeof(decodedMaterial.BaseColorR))
    || !TryReadExact(stream, &decodedMaterial.BaseColorG, sizeof(decodedMaterial.BaseColorG))
    || !TryReadExact(stream, &decodedMaterial.BaseColorB, sizeof(decodedMaterial.BaseColorB))
    || !TryReadExact(stream, &decodedMaterial.BaseColorA, sizeof(decodedMaterial.BaseColorA))
    || !TryReadFloat(stream, &decodedMaterial.Roughness)
    || !TryReadFloat(stream, &decodedMaterial.Metallic)
    || !TryReadFloat(stream, &decodedMaterial.Specular)
    || !TryReadExact(stream, &decodedMaterial.EmissiveColorR, sizeof(decodedMaterial.EmissiveColorR))
    || !TryReadExact(stream, &decodedMaterial.EmissiveColorG, sizeof(decodedMaterial.EmissiveColorG))
    || !TryReadExact(stream, &decodedMaterial.EmissiveColorB, sizeof(decodedMaterial.EmissiveColorB))
    || !TryReadExact(stream, &decodedMaterial.EmissiveColorA, sizeof(decodedMaterial.EmissiveColorA))
    || !TryReadBoolean(stream, &decodedMaterial.Lit)
    || !TryReadBoolean(stream, &decodedMaterial.DoubleSided)) {
    throw std::runtime_error("Wii U StandardShader material payload is truncated or contains an invalid Boolean field.");
}

Validate(decodedMaterial);
material = decodedMaterial;
return true;
```

`Validate` must reject blank material/shader/program/variant identities and require `std::isfinite` plus the inclusive zero-to-one range for roughness, metallic, and specular. The path overload must open one `FileStream`, delegate to the stream overload, call `Dispose`, delete the stream on success and failure, and preserve thrown validation errors.

The Makefile already wildcard-discovers `src/platform/wiiu/*.cpp`, so no build-list edit is required.

- [ ] **Step 5: Add parameter-complete runtime material state**

In `WiiURuntimeMaterial`, add PascalCase fields `Roughness`, `Metallic`, and `Specular`, each with substantive comments. Initialize the constructor to the shared StandardShader defaults and change the emissive default to white with zero strength:

```cpp
, Roughness(0.4f)
, Metallic(0.0f)
, Specular(0.5f)
, EmissiveColor(1.0f, 1.0f, 1.0f, 0.0f)
```

Add these documented public members:

```cpp
void SetRoughness(float roughness) { Roughness = roughness; }
float GetRoughness() const { return Roughness; }
void SetMetallic(float metallic) { Metallic = metallic; }
float GetMetallic() const { return Metallic; }
void SetSpecular(float specular) { Specular = specular; }
float GetSpecular() const { return Specular; }
```

Change `CreateRuntimeMaterial` consistently in the manager header and source to accept:

```cpp
std::string runtimeMaterialId,
float4 baseColor,
float roughness,
float metallic,
float specular,
float4 emissiveColor,
bool isLit,
bool isDoubleSided
```

Set every value explicitly. The `PlatformMaterialAsset*` legacy overloads must pass `0.4f`, `0.0f`, `0.5f`, and `float4(1.0f, 1.0f, 1.0f, 0.0f)`.

- [ ] **Step 6: Probe the new payload before both legacy loading paths**

Include the reader in `WiiURenderManager3D.cpp`. In the direct path overload, call `WiiUStandardShaderMaterialReader::TryRead(cookedAssetPath, standardMaterial)` before opening the legacy stream. In the content-source overload, open a probe stream, call `TryRead(probeStream, standardMaterial)`, dispose it, and open a fresh stream for `AssetSerializer::Deserialize` only when the magic does not match.

Add this private manager method and forward declaration in the header:

```cpp
WiiURuntimeMaterial* BuildStandardShaderRuntimeMaterial(const WiiUStandardShaderMaterial& materialAsset);
```

Its body must convert byte colors with division by `255.0f` and call the parameter-complete `CreateRuntimeMaterial` with the cooked values. After that common conversion, each public path overload must load `DiffuseTextureAssetId` through its matching texture overload: the direct path uses `BuildTextureHandleFromCooked(path)`, while the content-source path uses `BuildTextureHandleFromCooked(path, contentStreamSource)`.

- [ ] **Step 7: Run the reader source test and verify GREEN**

Run the command from Step 2.

Expected: the source-contract test passes.

- [ ] **Step 8: Commit native decoding and dispatch**

```powershell
rtk git add -- src/platform/wiiu/WiiUStandardShaderMaterialReader.hpp src/platform/wiiu/WiiUStandardShaderMaterialReader.cpp src/platform/wiiu/WiiURuntimeMaterial.hpp src/platform/wiiu/WiiURenderManager3D.hpp src/platform/wiiu/WiiURenderManager3D.cpp builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "feat: load Wii U StandardShader material payloads"
```

### Task 4: Carry runtime parameters into the reflected GX2 blocks

**Files:**
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp:1693-1772`
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing runtime-to-presenter source test**

Add this fact with a substantive XML comment:

```csharp
[Fact]
public void RuntimeSeam_UploadsAuthoredStandardShaderMaterialParameters() {
    string repositoryRootPath = WiiUTestSourcePaths.FindRepositoryRootPath();
    string runtimeMaterialSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeMaterial.hpp"));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

    Assert.Contains("void SetRoughness(float roughness)", runtimeMaterialSource, StringComparison.Ordinal);
    Assert.Contains("float GetRoughness() const", runtimeMaterialSource, StringComparison.Ordinal);
    Assert.Contains("void SetMetallic(float metallic)", runtimeMaterialSource, StringComparison.Ordinal);
    Assert.Contains("float GetMetallic() const", runtimeMaterialSource, StringComparison.Ordinal);
    Assert.Contains("void SetSpecular(float specular)", runtimeMaterialSource, StringComparison.Ordinal);
    Assert.Contains("float GetSpecular() const", runtimeMaterialSource, StringComparison.Ordinal);
    Assert.Contains("runtimeMaterial->SetRoughness(roughness);", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("runtimeMaterial->SetMetallic(metallic);", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("runtimeMaterial->SetSpecular(specular);", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("runtimeMaterial.GetRoughness()", presenterSource, StringComparison.Ordinal);
    Assert.Contains("runtimeMaterial.GetMetallic()", presenterSource, StringComparison.Ordinal);
    Assert.Contains("runtimeMaterial.GetSpecular()", presenterSource, StringComparison.Ordinal);
    Assert.DoesNotContain("const float roughnessData[] = { 1.0f", presenterSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the runtime parameter test and verify RED**

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_UploadsAuthoredStandardShaderMaterialParameters" -v minimal
```

Expected: test fails because the presenter still contains hardcoded scalar values instead of calling the runtime material accessors added in Task 3.

- [ ] **Step 3: Replace presenter hardcodes with runtime values**

In `RenderStandard3DDrawCommandToColorBuffer`, replace only these arrays:

```cpp
const float roughnessData[] = { runtimeMaterial.GetRoughness(), 0.0f, 0.0f, 0.0f };
const float metallicData[] = { runtimeMaterial.GetMetallic(), 0.0f, 0.0f, 0.0f };
const float specularData[] = { runtimeMaterial.GetSpecular(), 0.0f, 0.0f, 0.0f };
const float emissiveData[] = {
    runtimeMaterial.GetEmissiveColor().X,
    runtimeMaterial.GetEmissiveColor().Y,
    runtimeMaterial.GetEmissiveColor().Z,
    runtimeMaterial.GetEmissiveColor().W
};
```

Do not change shader selection, reflected block lookup, sampler selection, shadow metadata, or texture fallbacks.

- [ ] **Step 4: Run focused material/runtime tests and verify GREEN**

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~WiiUStandardShaderMaterialBinarySerializerTests|FullyQualifiedName~WiiUMaterialCookerTests|FullyQualifiedName~CookMaterial_WithStandardShaderSchema|FullyQualifiedName~RuntimeSeam_ReadsVersionedStandardShaderMaterialBeforeLegacyFallback|FullyQualifiedName~RuntimeSeam_UploadsAuthoredStandardShaderMaterialParameters" -v minimal
```

Expected: all selected tests pass.

- [ ] **Step 5: Commit runtime parameter upload**

```powershell
rtk git add -- src/platform/wiiu/WiiUGx2Presenter.cpp builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "feat: upload authored Wii U StandardShader parameters"
```

### Task 5: Verify the complete unshadowed material path in tests, build, and Cemu

**Files:**
- Validate: `builder.tests/helengine.wiiu.builder.tests.csproj`
- Validate: `C:/dev/helprojs/demodisc/wiiu-build/helengine_wiiu.wuhb`
- Validate: `C:/dev/helprojs/demodisc/wiiu-build/content/cooked/materials`
- Modify only if findings require a scoped correction: files named in Tasks 1-4

- [ ] **Step 1: Run the complete Wii U builder test suite**

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore -v minimal
```

Expected: all tests pass; the previous baseline was 80 passing tests, and this change adds the new serializer, cooker, and runtime-source cases.

- [ ] **Step 2: Check formatting and repository scope**

```powershell
rtk git diff --check
rtk git status --short
```

Expected: no whitespace errors. Only planned tracked source/test/doc changes may appear; `.diagnostics`, `bin_diagnostic`, `tmp_*.txt`, and build outputs remain untracked and unstaged.

- [ ] **Step 3: Build the authoritative DemoDisc Wii U package**

```powershell
rtk dotnet run --project ..\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helprojs\demodisc\wiiu-build --require helengine_wiiu.wuhb -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wiiu -Output C:\dev\helprojs\demodisc\wiiu-build
```

Expected: exit code 0 with fresh `helengine_wiiu.rpx` and `helengine_wiiu.wuhb` timestamps. Do not stage the package output.

- [ ] **Step 4: Close Cemu and remove only DemoDisc's exact cache files**

First confirm all four resolved paths remain under Cemu's shader-cache directory. Then remove only:

```powershell
rtk powershell -NoProfile -Command "Remove-Item -LiteralPath 'C:\Users\Helena\AppData\Roaming\Cemu\shaderCache\driver\vk\0005000f7dd6d7c4.bin','C:\Users\Helena\AppData\Roaming\Cemu\shaderCache\precompiled\0005000f7dd6d7c4_spirv.bin','C:\Users\Helena\AppData\Roaming\Cemu\shaderCache\transferable\0005000f7dd6d7c4_shaders.bin','C:\Users\Helena\AppData\Roaming\Cemu\shaderCache\transferable\0005000f7dd6d7c4_vkpipeline.bin' -Force -ErrorAction SilentlyContinue"
```

Expected: only title `0005000f7dd6d7c4` cache files are absent. No directory is removed recursively.

- [ ] **Step 5: Launch the freshly built WUHB**

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File scripts\launch_in_emulator.ps1 -ArtifactPath 'C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb'
```

Expected: Cemu remains open at the DemoDisc menu.

- [ ] **Step 6: Ask Helena to validate `cube_test` manually**

Ask Helena to navigate to `cube_test` and report whether the rotating cube still matches Windows, including face lighting and authored color/material response. Do not navigate Cemu automatically, type into Helena's UI, use OCR, or take screenshots unless Helena explicitly requests one of those actions.

- [ ] **Step 7: Record verification and finish the implementation commit**

If code changed after the Task 4 commit, rerun the focused tests, complete test suite, and authoritative build before staging only the correction. Stage only the following known implementation files; unchanged paths are harmless:

```powershell
rtk git add -- builder/WiiUStandardShaderMaterialAsset.cs builder/WiiUStandardShaderMaterialBinarySerializer.cs builder/WiiUMaterialSchemaIds.cs builder/WiiUMaterialCooker.cs builder.tests/WiiUStandardShaderMaterialBinarySerializerTests.cs builder.tests/WiiUMaterialCookerTests.cs builder.tests/WiiUPlatformAssetBuilderTests.cs builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUStandardShaderMaterialReader.hpp src/platform/wiiu/WiiUStandardShaderMaterialReader.cpp src/platform/wiiu/WiiURuntimeMaterial.hpp src/platform/wiiu/WiiURenderManager3D.hpp src/platform/wiiu/WiiURenderManager3D.cpp src/platform/wiiu/WiiUGx2Presenter.cpp
rtk git commit -m "fix: preserve Wii U StandardShader material parity"
```

Before committing, inspect the staged file list and unstage any generated outputs, `.diagnostics`, `bin_diagnostic`, `tmp_*.txt`, Cemu files, or DemoDisc build artifacts. If no post-validation correction was needed, report the four implementation commits plus the manual result; the plan document is committed before execution begins.
