using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies the Wii U builder path resolution rules used by the Dockerized native build.
/// </summary>
public sealed class WiiUBuilderPathsTests {
    /// <summary>
    /// Ensures the builder prefers the shared generated-core root supplied by the editor build graph when it is available.
    /// </summary>
    [Fact]
    public void ResolveGeneratedCoreRootPath_PrefersSharedGeneratedCoreRootFromBuildRequest() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wiiu-builder-path-tests", Guid.NewGuid().ToString("N"));
        string sharedGeneratedCoreRootPath = Path.Combine(workingRootPath, "shared-generated-core");
        PlatformBuildRequest request = new(
            new PlatformBuildManifest(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "wiiu",
                "1.0.0",
                string.Empty,
                [],
                [],
                [],
                [],
                [],
                new PlatformContainerWritePlan("default", [])),
            [new PlatformBuildTargetVariant("wiiu-default", "wiiu", "wiiu", "wiiu-default")],
            [new PlatformCookProfile(
                "wiiu-default",
                "Wii U Default",
                new PlatformCookProfileCapabilities(
                    "wiiu",
                    "raw",
                    "rgba",
                    "wiiu-scene-v1",
                    PlatformSerializationEndianness.LittleEndian))],
            Path.Combine(workingRootPath, "out"),
            Path.Combine(workingRootPath, "builder"),
            string.Empty,
            string.Empty,
            string.Empty,
            null,
            null,
            null,
            generatedCoreCppRootPath: sharedGeneratedCoreRootPath);

        string resolvedPath = WiiUBuilderPaths.ResolveGeneratedCoreRootPath(request);

        Assert.Equal(sharedGeneratedCoreRootPath, resolvedPath);
    }

    /// <summary>
    /// Ensures the repository-root resolver is anchored to the builder assembly location so editor-hosted runs still find the Wii U Makefile.
    /// </summary>
    [Fact]
    public void ResolveRepositoryRootPath_UsesBuilderAssemblyLocationAndFindsTheRepositoryMakefile() {
        string repositoryRootPath = WiiUBuilderPaths.ResolveRepositoryRootPath();

        Assert.True(File.Exists(Path.Combine(repositoryRootPath, "Makefile")));
        Assert.True(File.Exists(Path.Combine(repositoryRootPath, "src", "main.cpp")));
    }
}
