namespace helengine.wiiu.builder;

/// <summary>
/// Represents one builder-owned Wii U StandardShader material payload with resolved shader and texture identities.
/// </summary>
public sealed class WiiUStandardShaderMaterialAsset {
    /// <summary>
    /// Gets or sets the stable identity of the cooked material represented by this payload.
    /// </summary>
    public string MaterialAssetId { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the shared StandardShader asset identity that supplies this material's programs.
    /// </summary>
    public string ShaderAssetId { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the resolved StandardShader vertex-program name selected for this material.
    /// </summary>
    public string VertexProgramName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the resolved StandardShader pixel-program name selected for this material.
    /// </summary>
    public string PixelProgramName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the resolved StandardShader variant name selected for this material.
    /// </summary>
    public string VariantName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the diffuse texture asset identity, or an empty string when no authored diffuse texture is bound.
    /// </summary>
    public string DiffuseTextureAssetId { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the red byte channel of the material's base color.
    /// </summary>
    public byte BaseColorR { get; set; }

    /// <summary>
    /// Gets or sets the green byte channel of the material's base color.
    /// </summary>
    public byte BaseColorG { get; set; }

    /// <summary>
    /// Gets or sets the blue byte channel of the material's base color.
    /// </summary>
    public byte BaseColorB { get; set; }

    /// <summary>
    /// Gets or sets the alpha byte channel of the material's base color.
    /// </summary>
    public byte BaseColorA { get; set; }

    /// <summary>
    /// Gets or sets the normalized surface roughness used by the StandardShader lighting model.
    /// </summary>
    public float Roughness { get; set; }

    /// <summary>
    /// Gets or sets the normalized metallic response used by the StandardShader lighting model.
    /// </summary>
    public float Metallic { get; set; }

    /// <summary>
    /// Gets or sets the normalized specular response used by the StandardShader lighting model.
    /// </summary>
    public float Specular { get; set; }

    /// <summary>
    /// Gets or sets the red byte channel of the material's emissive color.
    /// </summary>
    public byte EmissiveColorR { get; set; }

    /// <summary>
    /// Gets or sets the green byte channel of the material's emissive color.
    /// </summary>
    public byte EmissiveColorG { get; set; }

    /// <summary>
    /// Gets or sets the blue byte channel of the material's emissive color.
    /// </summary>
    public byte EmissiveColorB { get; set; }

    /// <summary>
    /// Gets or sets the alpha byte channel of the material's emissive color.
    /// </summary>
    public byte EmissiveColorA { get; set; }

    /// <summary>
    /// Gets or sets whether the StandardShader evaluates the Wii U lighting path for this material.
    /// </summary>
    public bool Lit { get; set; }

    /// <summary>
    /// Gets or sets whether geometry using this material should render without back-face culling.
    /// </summary>
    public bool DoubleSided { get; set; }
}
