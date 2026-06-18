using helengine.baseplatform.Definitions;

namespace helengine.wiiu.builder;

/// <summary>
/// Creates the minimal Wii U platform definition consumed by the shared editor in the first build slice.
/// </summary>
public static class WiiUPlatformDefinitionFactory {
    /// <summary>
    /// Creates the current Wii U platform definition.
    /// </summary>
    /// <returns>The Wii U platform definition.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "wiiu",
            "Nintendo Wii U",
            [
                new PlatformBuildProfileDefinition(
                    "wiiu-default",
                    "Wii U Default",
                    "Standard Wii U RPX build",
                    "wiiu-default",
                    Array.Empty<PlatformSettingDefinition>())
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "wiiu-default",
                    "Wii U Default",
                    "Default Wii U graphics profile",
                    Array.Empty<PlatformSettingDefinition>())
            ],
            Array.Empty<PlatformAssetRequirementDefinition>(),
            Array.Empty<PlatformMaterialSchemaDefinition>(),
            Array.Empty<PlatformComponentSupportRule>(),
            Array.Empty<PlatformCodegenProfileDefinition>());
    }
}
