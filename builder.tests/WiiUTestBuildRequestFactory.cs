using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Creates minimal Wii U build requests for the first builder tests.
/// </summary>
public static class WiiUTestBuildRequestFactory {
    /// <summary>
    /// Creates one default Wii U build request with temporary output and intermediate roots.
    /// </summary>
    /// <returns>Minimal build request for the first editor-build slice.</returns>
    public static PlatformBuildRequest CreateDefault() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wiiu-builder-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(workingRootPath);
        return new PlatformBuildRequest(
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
            Path.Combine(workingRootPath, "tmp"));
    }
}
