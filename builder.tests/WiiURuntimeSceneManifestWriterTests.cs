using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies the packaged Wii U runtime scene manifest emitted by the builder.
/// </summary>
public sealed class WiiURuntimeSceneManifestWriterTests {
    /// <summary>
    /// Ensures the writer emits the startup scene id and cooked relative scene path into the generated runtime manifest.
    /// </summary>
    [Fact]
    public void Write_EmitsStartupSceneIdAndCookedRelativePaths() {
        string outputRootPath = Path.Combine(Path.GetTempPath(), "wiiu-runtime-manifest-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(outputRootPath);

        try {
            string manifestPath = Path.Combine(outputRootPath, "WiiURuntimeSceneManifest.generated.cpp");
            WiiURuntimeSceneManifestWriter writer = new();

            writer.Write(
                manifestPath,
                "Scenes/rendering/cube_test.helen",
                [new KeyValuePair<string, string>("Scenes/rendering/cube_test.helen", "cooked/scenes/rendering/cube_test.hasset")]);

            string source = File.ReadAllText(manifestPath);
            Assert.Contains("Scenes/rendering/cube_test.helen", source, StringComparison.Ordinal);
            Assert.Contains("cooked/scenes/rendering/cube_test.hasset", source, StringComparison.Ordinal);
        } finally {
            Directory.Delete(outputRootPath, true);
        }
    }
}
