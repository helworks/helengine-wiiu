namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the Wii U-owned canonical-to-packaged content path adapter.
/// </summary>
public sealed class WiiUContentStreamSourceTests {
    /// <summary>
    /// Ensures the native adapter maps the canonical Standard material path while retaining ordinary paths.
    /// </summary>
    [Fact]
    public void RuntimeSource_MapsCanonicalStandardMaterialPathAtPlatformBoundary() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string header = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUContentStreamSource.hpp"));
        string source = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUContentStreamSource.cpp"));

        Assert.Contains("class WiiUContentStreamSource final : public ::IContentStreamSource", header, StringComparison.Ordinal);
        Assert.Contains("CanonicalStandardMaterialPath", source, StringComparison.Ordinal);
        Assert.Contains("PackagedStandardMaterialAliasPath", source, StringComparison.Ordinal);
        Assert.Contains("assetPath = PackagedStandardMaterialAliasPath", source, StringComparison.Ordinal);
        Assert.Contains("return HostSource->OpenRead(assetPath);", source, StringComparison.Ordinal);
    }
}