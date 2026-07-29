namespace helengine.wiiu.builder;

/// <summary>
/// Stores the schema and field identifiers used by Wii U fixed-pipeline material cooking.
/// </summary>
public static class WiiUMaterialSchemaIds {
    /// <summary>
    /// Standard textured Wii U schema id.
    /// </summary>
    public const string StandardTexturedSchemaId = "wiiu-standard-textured";

    /// <summary>
    /// Shared generic standard-shader schema id accepted for backward compatibility.
    /// </summary>
    public const string StandardShaderSchemaId = "standard-shader";

    /// <summary>
    /// Texture-relative-path field id.
    /// </summary>
    public const string TextureRelativePathFieldId = "texture-relative-path";

    /// <summary>
    /// Shared generic texture-id field id accepted for backward compatibility.
    /// </summary>
    public const string TextureIdFieldId = "texture-id";

    /// <summary>
    /// Double-sided field id.
    /// </summary>
    public const string DoubleSidedFieldId = "double-sided";

    /// <summary>
    /// Vertex-color-mode field id.
    /// </summary>
    public const string VertexColorModeFieldId = "vertex-color-mode";

    /// <summary>
    /// Base-color field id.
    /// </summary>
    public const string BaseColorFieldId = "base-color";

    /// <summary>
    /// Identifies the authored normalized roughness value consumed by the StandardShader cooker.
    /// </summary>
    public const string RoughnessFieldId = "roughness";

    /// <summary>
    /// Identifies the authored normalized metallic value consumed by the StandardShader cooker.
    /// </summary>
    public const string MetallicFieldId = "metallic";

    /// <summary>
    /// Identifies the authored normalized specular value consumed by the StandardShader cooker.
    /// </summary>
    public const string SpecularFieldId = "specular";

    /// <summary>
    /// Identifies the authored emissive color and strength consumed by the StandardShader cooker.
    /// </summary>
    public const string EmissiveColorFieldId = "emissive-color";

    /// <summary>
    /// Lighting-mode field id.
    /// </summary>
    public const string LightingModeFieldId = "lighting-mode";
}
