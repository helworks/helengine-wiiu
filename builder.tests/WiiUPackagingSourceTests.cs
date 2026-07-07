namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the Wii U packaging source contracts that feed bundled content into Cemu-launchable artifacts.
/// </summary>
public sealed class WiiUPackagingSourceTests {
    /// <summary>
    /// Ensures the Wii U Makefile bridges the repository content variable into WUT's bundled-content input.
    /// </summary>
    [Fact]
    public void Makefile_MapsContentIntoAppContentForWuhbPackaging() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));

        Assert.Contains("CONTENT :=", makefileSource, StringComparison.Ordinal);
        Assert.Contains("APP_CONTENT := $(CONTENT)", makefileSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the canonical launcher and README accept the packaged WUHB artifact for Cemu verification.
    /// </summary>
    [Fact]
    public void LauncherAndReadme_DocumentWuhbLaunchContract() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptSource = File.ReadAllText(Path.Combine(repositoryRootPath, "scripts", "launch_in_emulator.ps1"));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains(".wuhb", scriptSource, StringComparison.Ordinal);
        Assert.Contains("helengine_wiiu.wuhb", readmeSource, StringComparison.Ordinal);
    }
}
