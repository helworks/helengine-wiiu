namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the first Wii U runtime seam between the boot host entrypoint and the application loop.
/// </summary>
public sealed class WiiURuntimeSourceTests {
    /// <summary>
    /// Ensures the boot host delegates into a dedicated Wii U application boundary and the new source is visible in the build contract.
    /// </summary>
    [Fact]
    public void RuntimeSeam_AddsApplicationBoundaryBehindBootHost() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
        string bootHostSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUBootHost.cpp"));
        string applicationHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp");
        string applicationSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp");

        Assert.True(File.Exists(applicationHeaderPath), "Expected WiiUApplication.hpp to exist.");
        Assert.True(File.Exists(applicationSourcePath), "Expected WiiUApplication.cpp to exist.");
        Assert.Contains("WiiUApplication.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/wiiu/WiiUApplication.hpp\"", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("WiiUApplication application {};", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("return application.Run();", bootHostSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged bootstrap exposes explicit packaged scene helpers and the application consumes them through the scene bootstrap boundary.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_DeclaresPackagedSceneHelpersAndRuntimeCalls() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.hpp");
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.True(File.Exists(bootstrapHeaderPath), "Expected WiiUSceneBootstrap.hpp to exist.");

        string bootstrapHeaderSource = File.ReadAllText(bootstrapHeaderPath);
        Assert.Contains("static std::string GetPackagedContentRootPath();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static RuntimeSceneCatalog* CreatePackagedSceneCatalog();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static std::string GetPackagedStartupSceneId();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiUSceneBootstrap::GetPackagedContentRootPath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiUSceneBootstrap::CreatePackagedSceneCatalog()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiUSceneBootstrap::GetPackagedStartupSceneId()", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged bootstrap fallback points at the authored cube_test scene and the README documents the final verification flow.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_UsesCubeTestAsTheAuthoredStartupScene() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.cpp"));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("Scenes/rendering/cube_test.helen", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("cooked/scenes/rendering/cube_test.hasset", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("cube_test", readmeSource, StringComparison.Ordinal);
        Assert.Contains("launch_in_emulator.ps1", readmeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U runtime seam advances the generated core each frame and owns explicit native bridge members.
    /// </summary>
    [Fact]
    public void RuntimeSeam_AdvancesGeneratedCoreWithPlatformBridges() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("bool UpdateEngineCore();", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("bool DrawEngineCore();", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EnginePlatformInfo;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"Core.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"PlatformInfo.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Initialize(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!UpdateEngineCore()) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!DrawEngineCore()) {", applicationSource, StringComparison.Ordinal);
    }
}
