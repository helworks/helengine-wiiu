using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>Validates source checkout selection when the builder is published to an isolated artifacts directory.</summary>
public sealed class WiiURepositoryRootResolutionTests {
    /// <summary>A validated source override takes precedence over the unrelated assembly publication root.</summary>
    [Fact]
    public void ExplicitSourceCheckoutOverridesAssemblyLocation() {
        string root = Path.Combine(Path.GetTempPath(), "wiiu-repository-path-tests");
        Directory.CreateDirectory(Path.Combine(root, "src"));
        File.WriteAllText(Path.Combine(root, "Makefile"), "path-test marker");
        File.WriteAllText(Path.Combine(root, "src", "main.cpp"), "path-test marker");
        Assert.Equal(Path.GetFullPath(root), WiiUBuilderPaths.ResolveRepositoryRootPath(root, Path.Combine(root, "unrelated", "builder.dll")));
    }

    /// <summary>Relative and nonexistent overrides cannot silently fall back to an unintended source tree.</summary>
    [Theory]
    [InlineData("helengine-wiiu")]
    [InlineData("C:/missing-helengine-wiiu-checkout")]
    public void InvalidOverrideIsRejected(string root) {
        Assert.Throws<InvalidOperationException>(() => WiiUBuilderPaths.ResolveRepositoryRootPath(root, "ignored.dll"));
    }

    /// <summary>The established builder/bin/Debug/framework installation remains the fallback without an override.</summary>
    [Fact]
    public void InstalledAssemblyLayoutRemainsCompatible() {
        string root = Path.Combine(Path.GetTempPath(), "wiiu-installed-root");
        Assert.Equal(root, WiiUBuilderPaths.ResolveRepositoryRootPath(string.Empty, Path.Combine(root, "builder", "bin", "Debug", "net9.0", "builder.dll")));
    }
}
