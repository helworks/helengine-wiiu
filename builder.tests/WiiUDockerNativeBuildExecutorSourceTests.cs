namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the Docker executor contract used by the Wii U editor-driven build path.
/// </summary>
public sealed class WiiUDockerNativeBuildExecutorSourceTests {
    /// <summary>
    /// Ensures the Docker executor performs one clean native build, mounts packaged content when available, and avoids a container-side make clean target.
    /// </summary>
    [Fact]
    public void DockerExecutor_UsesContentAwareMakeAfterCleaningBuildDirectoryInManagedCode() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string executorSource = File.ReadAllText(Path.Combine(repositoryRootPath, "builder", "WiiUDockerNativeBuildExecutor.cs"));

        Assert.Contains("EnsureCleanNativeBuildOutput(repositoryRootPath);", executorSource, StringComparison.Ordinal);
        Assert.Contains("Directory.Delete(buildRootPath, recursive: true);", executorSource, StringComparison.Ordinal);
        Assert.Contains("Directory.Exists(packageSourceRootPath)", executorSource, StringComparison.Ordinal);
        Assert.Contains("packageSourceRootPath + \":/workspace/content\"", executorSource, StringComparison.Ordinal);
        Assert.Contains("make CONTENT=/workspace/content", executorSource, StringComparison.Ordinal);
        Assert.Contains("APP_CONTENT=/workspace/content", executorSource, StringComparison.Ordinal);
        Assert.DoesNotContain("make clean && make", executorSource, StringComparison.Ordinal);
    }
}
