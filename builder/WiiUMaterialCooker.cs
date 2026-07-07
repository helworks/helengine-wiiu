using System.Globalization;
using helengine;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.wiiu.builder;

/// <summary>
/// Translates Wii U material schema payloads into cooked platform-owned runtime material assets.
/// </summary>
public sealed class WiiUMaterialCooker {
    /// <summary>
    /// Cooks one Wii U material request into a serialized platform-owned material payload.
    /// </summary>
    /// <param name="request">Builder-owned material translation request.</param>
    /// <returns>Serialized platform-owned material asset with no shader-package dependencies.</returns>
    public PlatformMaterialCookResult Cook(PlatformMaterialCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (!string.Equals(request.TargetPlatformId, "wiiu", StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"Wii U cannot cook materials for target platform '{request.TargetPlatformId}'.");
        } else if (!IsSupportedSchema(request.SchemaId)) {
            throw new InvalidOperationException($"Wii U does not support material schema '{request.SchemaId}'.");
        }

        ResolveBaseColor(
            request.FieldValues,
            out byte baseColorRed,
            out byte baseColorGreen,
            out byte baseColorBlue,
            out byte baseColorAlpha);

        PlatformMaterialAsset cookedAsset = new() {
            Id = request.MaterialAssetId,
            RendererFamilyId = string.IsNullOrWhiteSpace(request.SelectedGraphicsProfileId)
                ? throw new InvalidOperationException("Wii U material cooking requires a graphics profile id.")
                : request.SelectedGraphicsProfileId,
            TextureRelativePath = ResolveTextureRelativePath(request.FieldValues),
            DoubleSided = ResolveBoolean(request.FieldValues, WiiUMaterialSchemaIds.DoubleSidedFieldId, false),
            UseVertexColor = ResolveVertexColorMode(request.FieldValues),
            Lit = ResolveLightingMode(request.FieldValues),
            BaseColorR = baseColorRed,
            BaseColorG = baseColorGreen,
            BaseColorB = baseColorBlue,
            BaseColorA = baseColorAlpha
        };

        return new PlatformMaterialCookResult(
            global::helengine.files.AssetSerializer.SerializeToBytes(cookedAsset),
            []);
    }

    /// <summary>
    /// Returns whether one schema id is supported by the Wii U cooked-material contract.
    /// </summary>
    /// <param name="schemaId">Schema id to evaluate.</param>
    /// <returns>True when the schema id maps to the Wii U cooked-material contract.</returns>
    static bool IsSupportedSchema(string schemaId) {
        return string.Equals(schemaId, WiiUMaterialSchemaIds.StandardTexturedSchemaId, StringComparison.OrdinalIgnoreCase)
            || string.Equals(schemaId, WiiUMaterialSchemaIds.StandardShaderSchemaId, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Resolves the optional texture-relative path for one Wii U material request.
    /// </summary>
    /// <param name="fieldValues">Authored material field values.</param>
    /// <returns>Resolved texture-relative path, or an empty string when the schema does not bind a texture.</returns>
    static string ResolveTextureRelativePath(IReadOnlyDictionary<string, string> fieldValues) {
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (fieldValues.TryGetValue(WiiUMaterialSchemaIds.TextureRelativePathFieldId, out string textureRelativePath)
            && !string.IsNullOrWhiteSpace(textureRelativePath)) {
            return textureRelativePath;
        } else if (fieldValues.TryGetValue(WiiUMaterialSchemaIds.TextureIdFieldId, out string textureId)
            && !string.IsNullOrWhiteSpace(textureId)) {
            return textureId;
        }

        return string.Empty;
    }

    /// <summary>
    /// Resolves whether the material should use vertex colors.
    /// </summary>
    /// <param name="fieldValues">Authored material field values.</param>
    /// <returns>True when vertex color should affect the runtime material.</returns>
    static bool ResolveVertexColorMode(IReadOnlyDictionary<string, string> fieldValues) {
        string value;
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (!fieldValues.TryGetValue(WiiUMaterialSchemaIds.VertexColorModeFieldId, out value)
            || string.IsNullOrWhiteSpace(value)) {
            return true;
        } else if (string.Equals(value, "multiply", StringComparison.OrdinalIgnoreCase)) {
            return true;
        } else if (string.Equals(value, "ignore", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        throw new InvalidOperationException($"Wii U vertex color mode '{value}' is not supported.");
    }

    /// <summary>
    /// Resolves whether the material should be lit.
    /// </summary>
    /// <param name="fieldValues">Authored material field values.</param>
    /// <returns>True when the runtime material should use lit behavior.</returns>
    static bool ResolveLightingMode(IReadOnlyDictionary<string, string> fieldValues) {
        string value;
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (!fieldValues.TryGetValue(WiiUMaterialSchemaIds.LightingModeFieldId, out value)
            || string.IsNullOrWhiteSpace(value)) {
            return true;
        } else if (string.Equals(value, "lit", StringComparison.OrdinalIgnoreCase)) {
            return true;
        } else if (string.Equals(value, "unlit", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        throw new InvalidOperationException($"Wii U lighting mode '{value}' is not supported.");
    }

    /// <summary>
    /// Resolves one authored base color into cooked byte channels.
    /// </summary>
    /// <param name="fieldValues">Authored material field values.</param>
    /// <param name="red">Resolved red byte.</param>
    /// <param name="green">Resolved green byte.</param>
    /// <param name="blue">Resolved blue byte.</param>
    /// <param name="alpha">Resolved alpha byte.</param>
    static void ResolveBaseColor(
        IReadOnlyDictionary<string, string> fieldValues,
        out byte red,
        out byte green,
        out byte blue,
        out byte alpha) {
        string value;
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (!fieldValues.TryGetValue(WiiUMaterialSchemaIds.BaseColorFieldId, out value)
            || string.IsNullOrWhiteSpace(value)) {
            red = 255;
            green = 255;
            blue = 255;
            alpha = 255;
            return;
        }

        ParseColor(value, out red, out green, out blue, out alpha);
    }

    /// <summary>
    /// Parses one authored color string into cooked byte channels.
    /// </summary>
    /// <param name="value">Authored color value in <c>#RRGGBB</c> or <c>#RRGGBBAA</c> form.</param>
    /// <param name="red">Resolved red byte.</param>
    /// <param name="green">Resolved green byte.</param>
    /// <param name="blue">Resolved blue byte.</param>
    /// <param name="alpha">Resolved alpha byte.</param>
    static void ParseColor(string value, out byte red, out byte green, out byte blue, out byte alpha) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new InvalidOperationException("Wii U base color must be provided.");
        }

        string normalizedValue = value.Trim();
        if (normalizedValue.StartsWith('#')) {
            normalizedValue = normalizedValue[1..];
        }

        if (normalizedValue.Length == 6) {
            normalizedValue += "FF";
        } else if (normalizedValue.Length != 8) {
            throw new InvalidOperationException($"Wii U color '{value}' must use #RRGGBB or #RRGGBBAA format.");
        }

        red = byte.Parse(normalizedValue.AsSpan(0, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        green = byte.Parse(normalizedValue.AsSpan(2, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        blue = byte.Parse(normalizedValue.AsSpan(4, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        alpha = byte.Parse(normalizedValue.AsSpan(6, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
    }

    /// <summary>
    /// Resolves one boolean field value with a supplied default.
    /// </summary>
    /// <param name="fieldValues">Authored material field values.</param>
    /// <param name="fieldId">Field identifier to resolve.</param>
    /// <param name="defaultValue">Default value used when the field is absent.</param>
    /// <returns>Resolved boolean value.</returns>
    static bool ResolveBoolean(IReadOnlyDictionary<string, string> fieldValues, string fieldId, bool defaultValue) {
        string value;
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (string.IsNullOrWhiteSpace(fieldId)) {
            throw new ArgumentException("Field id must be provided.", nameof(fieldId));
        } else if (!fieldValues.TryGetValue(fieldId, out value) || string.IsNullOrWhiteSpace(value)) {
            return defaultValue;
        } else if (bool.TryParse(value, out bool parsedValue)) {
            return parsedValue;
        }

        throw new InvalidOperationException($"Wii U boolean field '{fieldId}' value '{value}' is invalid.");
    }
}
