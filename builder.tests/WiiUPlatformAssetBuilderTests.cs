using helengine;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.baseplatform.Targets;
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
        Assert.Equal(RuntimeMaterialResolutionMode.CookedPlatformOwned, builder.Definition.RuntimeGenerationContract.MaterialResolutionMode);
        PlatformBuildProfileDefinition buildProfile = Assert.Single(
            builder.Definition.BuildProfiles,
            profile => profile.ProfileId == "wiiu-default");
        Assert.Contains(builder.Definition.GraphicsProfiles, profile => profile.ProfileId == "wiiu-default");
        Assert.Contains(builder.Definition.MaterialSchemas, schema => schema.SchemaId == WiiUMaterialSchemaIds.StandardTexturedSchemaId);
        Assert.Equal("default", buildProfile.CodegenProfileId);
        PlatformStorageProfileDefinition storageProfile = Assert.Single(
            builder.Definition.StorageProfiles,
            profile => profile.ProfileId == "loose-files");
        PlatformMediaProfileDefinition mediaProfile = Assert.Single(
            builder.Definition.MediaProfiles,
            profile => profile.ProfileId == "wiiu-install-tree");
        Assert.Equal(PlatformStorageProfileKind.LooseFiles, storageProfile.StorageKind);
        Assert.Equal("wiiu-loose-files", storageProfile.RuntimeSpecializationId);
        Assert.Equal(PlatformMediaLayoutKind.InstallTree, mediaProfile.LayoutKind);

        PlatformCodegenProfileDefinition codegenProfile = Assert.Single(
            builder.Definition.CodegenProfiles,
            profile => profile.ProfileId == "default");
        Assert.Equal(PlatformCodegenLanguage.Cpp, codegenProfile.OutputLanguage);
        Assert.Equal(PlatformSerializationEndianness.BigEndian, codegenProfile.Endianness);
    }

    /// <summary>
    /// Ensures the Wii U builder cooks the shared standard-shader schema into the versioned Wii U StandardShader payload.
    /// </summary>
    [Fact]
    public void CookMaterial_WithStandardShaderSchema_ProducesCookedStandardShaderMaterialAsset() {
        WiiUPlatformAssetBuilder builder = new();
        PlatformMaterialCookRequest request = new(
            "wiiu-material-01",
            "Materials/rendering/cube_test/Cube00.helmat",
            "wiiu",
            "wiiu-default",
            "wiiu-default",
            "standard-shader",
            new Dictionary<string, string> {
                ["texture-id"] = "cooked/textures/test.hasset",
                ["double-sided"] = "true",
                ["vertex-color-mode"] = "ignore",
                ["base-color"] = "#804020FF",
                ["lighting-mode"] = "unlit"
            });

        PlatformMaterialCookResult result = builder.CookMaterial(request);
        WiiUStandardShaderMaterialAsset asset = new WiiUStandardShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);

        Assert.NotNull(result);
        Assert.NotEmpty(result.CookedMaterialBytes);
        Assert.Equal("cooked/textures/test.hasset", asset.DiffuseTextureAssetId);
        Assert.Equal((byte)128, asset.BaseColorR);
        Assert.Equal((byte)64, asset.BaseColorG);
        Assert.Equal((byte)32, asset.BaseColorB);
        Assert.Equal((byte)255, asset.BaseColorA);
        Assert.False(asset.Lit);
        Assert.True(asset.DoubleSided);
        Assert.Equal("ForwardStandardShader", Assert.Single(result.ReferencedShaderAssetIds));
    }

    /// <summary>
    /// Ensures the default Wii U build flow copies the built packaged artifacts into the requested output root.
    /// </summary>
    [Fact]
    public async Task BuildAsync_WhenUsingDefaultFlow_WritesPackagedArtifactsIntoOutputRoot() {
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
        Assert.True(File.Exists(Path.Combine(request.OutputRoot, "helengine_wiiu.wuhb")));
        Assert.Contains("helengine_wiiu.rpx", nativeBuildExecutor.LastProducedArtifactPath, StringComparison.Ordinal);
        Assert.Contains("helengine_wiiu.wuhb", nativeBuildExecutor.LastProducedBundlePath, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U build flow stages the referenced packaged content payloads into the output content tree.
    /// </summary>
    [Fact]
    public async Task BuildAsync_WhenManifestContainsPackagedPayloads_StagesContentIntoOutputRoot() {
        RecordingWiiUNativeBuildExecutor nativeBuildExecutor = new();
        WiiUPlatformAssetBuilder builder = new(nativeBuildExecutor);
        PlatformBuildRequest request = CreatePackagedContentBuildRequest();
        RecordingProgressReporter progressReporter = new();
        RecordingDiagnosticReporter diagnosticReporter = new();

        PlatformBuildReport report = await builder.BuildAsync(
            request,
            progressReporter,
            diagnosticReporter,
            CancellationToken.None);

        Assert.True(report.Succeeded);
        Assert.True(File.Exists(Path.Combine(request.OutputRoot, "content", "cooked", "scenes", "DemoDiscMainMenu.hasset")));
        Assert.True(File.Exists(Path.Combine(request.OutputRoot, "content", "cooked", "fonts", "default.hefont")));
    }

    /// <summary>
    /// Creates one build request whose builder working root contains staged package-source payloads.
    /// </summary>
    /// <returns>Build request that exercises packaged-content staging.</returns>
    static PlatformBuildRequest CreatePackagedContentBuildRequest() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wiiu-builder-tests", Guid.NewGuid().ToString("N"));
        string builderWorkingRootPath = Path.Combine(workingRootPath, "tmp");
        string packageSourceRootPath = Path.Combine(builderWorkingRootPath, "package-source");
        string startupSceneRelativePath = "cooked/scenes/DemoDiscMainMenu.hasset";
        string fontRelativePath = "cooked/fonts/default.hefont";
        WritePackageSourceFile(packageSourceRootPath, startupSceneRelativePath, "scene");
        WritePackageSourceFile(packageSourceRootPath, fontRelativePath, "font");

        PlatformBuildManifest manifest = new(
            1,
            "project",
            "1.0.0",
            "1.0.0",
            "wiiu",
            "1.0.0",
            "Scenes/DemoDiscMainMenu.helen",
            [
                new PlatformBuildScene(
                    "Scenes/DemoDiscMainMenu.helen",
                    "DemoDiscMainMenu",
                    "Scenes/DemoDiscMainMenu.helen",
                    [new PlatformBuildPayloadReference(startupSceneRelativePath, startupSceneRelativePath)],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, startupSceneRelativePath)])
            ],
            [
                new PlatformBuildAsset(
                    "Fonts/Default",
                    "DefaultFont",
                    "Fonts/default.hefont",
                    new PlatformBuildPayloadReference(fontRelativePath, fontRelativePath),
                    [])
            ],
            [],
            [],
            [],
            new PlatformContainerWritePlan("default", []));

        return new PlatformBuildRequest(
            manifest,
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
            builderWorkingRootPath);
    }

    /// <summary>
    /// Writes one package-source payload file used by the staged-content test request.
    /// </summary>
    /// <param name="packageSourceRootPath">Builder package-source root that should receive the payload file.</param>
    /// <param name="relativePath">Runtime-relative payload path inside the package-source root.</param>
    /// <param name="contents">File contents to write for the payload.</param>
    static void WritePackageSourceFile(string packageSourceRootPath, string relativePath, string contents) {
        string fullPath = Path.Combine(packageSourceRootPath, relativePath.Replace('/', Path.DirectorySeparatorChar));
        string directoryPath = Path.GetDirectoryName(fullPath)
            ?? throw new InvalidOperationException("Package-source payload directory path could not be resolved.");
        Directory.CreateDirectory(directoryPath);
        File.WriteAllText(fullPath, contents);
    }
}
