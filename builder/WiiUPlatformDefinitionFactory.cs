using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.wiiu.builder;

/// <summary>
/// Creates the minimal Wii U platform definition consumed by the shared editor in the first build slice.
/// </summary>
public static class WiiUPlatformDefinitionFactory {
    /// <summary>
    /// Generic native numeric type remaps required by C++ platforms that cannot emit System.Numerics runtime types directly.
    /// </summary>
    const string NativeNumericTypeRemaps = "System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4";

    /// <summary>
    /// Generic generated-math-convention value that instructs the shared C++ generator to emit native column-vector math helpers.
    /// </summary>
    const string NativeColumnVectorMathConvention = "native-column-vector";

    /// <summary>
    /// Generic pointer-size contract forwarded to the shared C++ generator for Wii U-native output.
    /// </summary>
    const string WiiUPointerSizeInBytes = "4";

    /// <summary>
    /// Generic runtime specialization id used by the first Wii U loose-file packaged runtime slice.
    /// </summary>
    const string WiiURuntimeSpecializationId = "wiiu-loose-files";

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
                    "default",
                    [
                        new PlatformSettingDefinition(
                            "game-name",
                            "Game Name",
                            PlatformSettingKind.Text,
                            "helengine",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "game-description",
                            "Game Description",
                            PlatformSettingKind.Text,
                            "Made with helengine",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "wiiu-default",
                    "Wii U Default",
                    "Default Wii U graphics profile",
                    Array.Empty<PlatformSettingDefinition>())
            ],
            Array.Empty<PlatformAssetRequirementDefinition>(),
            [
                new PlatformMaterialSchemaDefinition(
                    WiiUMaterialSchemaIds.StandardTexturedSchemaId,
                    "Wii U Standard Textured",
                    ["wiiu-default"],
                    [
                        new PlatformMaterialFieldDefinition(
                            WiiUMaterialSchemaIds.TextureRelativePathFieldId,
                            "Texture",
                            PlatformMaterialFieldKind.Text,
                            string.Empty,
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            WiiUMaterialSchemaIds.DoubleSidedFieldId,
                            "Double Sided",
                            PlatformMaterialFieldKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            WiiUMaterialSchemaIds.VertexColorModeFieldId,
                            "Vertex Color",
                            PlatformMaterialFieldKind.Choice,
                            "multiply",
                            true,
                            ["multiply", "ignore"]),
                        new PlatformMaterialFieldDefinition(
                            WiiUMaterialSchemaIds.BaseColorFieldId,
                            "Base Color",
                            PlatformMaterialFieldKind.Color,
                            "#FFFFFFFF",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            WiiUMaterialSchemaIds.LightingModeFieldId,
                            "Lighting",
                            PlatformMaterialFieldKind.Choice,
                            "lit",
                            true,
                            ["lit", "unlit"])
                    ])
            ],
            Array.Empty<PlatformComponentSupportRule>(),
            [
                new PlatformCodegenProfileDefinition(
                    "default",
                    "Default",
                    "Wii U C# to C++ codegen profile",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.BigEndian,
                    [
                        new PlatformSettingDefinition(
                            "write-conversion-report",
                            "Write Conversion Report",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "include-project-defined-preprocessor-symbols",
                            "Include Project Symbols",
                            PlatformSettingKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "load-native-runtime-metadata",
                            "Load Native Runtime Metadata",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            PlatformCodegenSettingIds.EnabledFeatures,
                            "Enabled Runtime Features",
                            PlatformSettingKind.Text,
                            "host_file_system",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "generated-math-convention",
                            "Generated Math Convention",
                            PlatformSettingKind.Text,
                            NativeColumnVectorMathConvention,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "pointer-size-bytes",
                            "Pointer Size (Bytes)",
                            PlatformSettingKind.Text,
                            WiiUPointerSizeInBytes,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "type-remaps",
                            "Type Remaps",
                            PlatformSettingKind.Text,
                            NativeNumericTypeRemaps,
                            true,
                            [])
                    ])
            ],
            [
                new PlatformStorageProfileDefinition(
                    "loose-files",
                    "Loose Files",
                    PlatformStorageProfileKind.LooseFiles,
                    WiiURuntimeSpecializationId,
                    allowContainerSegmentation: false)
            ],
            [
                new PlatformMediaProfileDefinition(
                    "wiiu-install-tree",
                    "Wii U Install Tree",
                    PlatformMediaLayoutKind.InstallTree,
                    allowPhysicalDuplication: true,
                    preferLocalityOverDeduplication: false)
            ],
            new RuntimeGenerationContract(
                RuntimeMaterialResolutionMode.CookedPlatformOwned,
                true,
                PackagedPathPolicy.ContentRelativeOnly,
                [PortableInputPreprocessorSymbolCatalog.BepuUseClassicBroadPhaseUpdateSymbol]));
    }
}
