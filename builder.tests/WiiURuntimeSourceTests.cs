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
}
