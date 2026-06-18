using helengine.baseplatform.Definitions;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.wiiu.builder;

namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies the first Wii U builder metadata and artifact flow contract.
/// </summary>
public sealed class WiiUPlatformAssetBuilderTests {
    /// <summary>
    /// Ensures the builder exposes the expected public Wii U metadata and platform definition.
    /// </summary>
    [Fact]
    public void DescriptorAndDefinition_ExposeExpectedWiiUMetadata() {
        WiiUPlatformAssetBuilder builder = new();

        Assert.Equal("helengine.wiiu.builder", builder.Descriptor.BuilderId);
        Assert.Equal("wiiu", builder.Descriptor.TargetPlatformId);
        Assert.Contains("wiiu", builder.Descriptor.SupportedRuntimeBackendIds);
        Assert.Equal("wiiu", builder.Definition.PlatformId);
        Assert.Contains(builder.Definition.BuildProfiles, profile => profile.ProfileId == "wiiu-default");
        Assert.Contains(builder.Definition.GraphicsProfiles, profile => profile.ProfileId == "wiiu-default");
    }

    /// <summary>
    /// Ensures the default Wii U build flow copies the built RPX into the requested output root.
    /// </summary>
    [Fact]
    public async Task BuildAsync_WhenUsingDefaultFlow_WritesRpxIntoOutputRoot() {
        RecordingWiiUNativeBuildExecutor nativeBuildExecutor = new();
        WiiUPlatformAssetBuilder builder = new(nativeBuildExecutor);
        PlatformBuildRequest request = WiiUTestBuildRequestFactory.CreateDefault();
        RecordingProgressReporter progressReporter = new();
        RecordingDiagnosticReporter diagnosticReporter = new();

        PlatformBuildReport report = await builder.BuildAsync(
            request,
            progressReporter,
            diagnosticReporter,
            CancellationToken.None);

        Assert.True(report.Succeeded);
        Assert.True(File.Exists(Path.Combine(request.OutputRoot, "helengine_wiiu.rpx")));
        Assert.Contains("helengine_wiiu.rpx", nativeBuildExecutor.LastProducedArtifactPath, StringComparison.Ordinal);
    }
}
