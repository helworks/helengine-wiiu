using helengine.baseplatform.Definitions;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards Wii U platform-definition metadata that generated-core code selection depends on.
/// </summary>
public sealed class WiiUPlatformDefinitionFactoryTests {
    /// <summary>
    /// Ensures the native loose-file bootstrap retains the generated host filesystem stream source referenced outside managed reachability analysis.
    /// </summary>
    [Fact]
    public void Create_force_enables_host_file_system_runtime_feature() {
        PlatformDefinition definition = WiiUPlatformDefinitionFactory.Create();
        PlatformCodegenProfileDefinition codegenProfile = Assert.Single(definition.CodegenProfiles);
        PlatformSettingDefinition enabledFeatures = Assert.Single(
            codegenProfile.Settings,
            setting => setting.SettingId == PlatformCodegenSettingIds.EnabledFeatures);

        Assert.Equal("host_file_system", enabledFeatures.DefaultValue);
    }

    /// <summary>
    /// Ensures the Wii U generated runtime opts into the classic BEPU broadphase update path.
    /// </summary>
    [Fact]
    public void Create_includes_classic_broadphase_portable_symbol() {
        PlatformDefinition definition = WiiUPlatformDefinitionFactory.Create();

        Assert.Contains(
            PortableInputPreprocessorSymbolCatalog.BepuUseClassicBroadPhaseUpdateSymbol,
            definition.RuntimeGenerationContract.PortableInputPreprocessorSymbols);
    }
}
