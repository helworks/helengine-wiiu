using helengine.baseplatform.Definitions;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards Wii U platform-definition metadata that generated-core code selection depends on.
/// </summary>
public sealed class WiiUPlatformDefinitionFactoryTests {
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
