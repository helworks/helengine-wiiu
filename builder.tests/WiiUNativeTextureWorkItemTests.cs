using helengine;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.editor;

namespace helengine.wiiu.builder.tests;

/// <summary>Runs production image and atlas workitems through the real builder up to its injected native executor boundary.</summary>
public sealed class WiiUNativeTextureWorkItemTests {
    /// <summary>Checks image and font-atlas workitem payloads load under each target policy.</summary>
    [Theory]
    [InlineData(16)]
    [InlineData(32)]
    [InlineData(64)]
    public async Task PayloadsLoadWithTargetNumericPolicy(int bits) {
        var policy = new NumericPrecisionPolicy(bits, bits);
        string root = PrepareFixture("numeric-policy");
        string source = WriteSource(root, ".hasset");
        string font = WriteFontSource(root, "source.hefont", "cooked/fonts/atlas.hetex");
        var items = new[] { Item("cooked/image.hetex", source), Item("cooked/fonts/atlas.hetex", font, "font-atlas-texture") };
        var request = Request(root, items, numericPolicy: policy);
        var report = await new WiiUPlatformAssetBuilder(new RecordingWiiUNativeBuildExecutor()).BuildAsync(request, new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.True(report.Succeeded);
        foreach (var item in items) {
            using var stream = File.OpenRead(Path.Combine(request.OutputRoot, "content", item.OutputRelativePath));
            using TextureAsset cooked = Assert.IsType<TextureAsset>(PackagedAssetBinarySerializer.Deserialize(stream, policy));
            Assert.Equal(5, cooked.Width);
        }
    }

    /// <summary>Prepares every image-cookable GX2 format for both images and embedded font atlases in one real package batch.</summary>
    [Fact]
    public async Task EveryNativeImageAndFontAtlasWorkItemIsCookedBeforeNativeBuild() {
        string root = PrepareFixture("all-native-workitems");
        string source = WriteSource(root, ".hasset");
        string fontSource = WriteFontSource(root, "font-source.hefont", "cooked/fonts/atlas.hetex");
        byte[] original = File.ReadAllBytes(source);
        byte[] originalFont = File.ReadAllBytes(fontSource);
        List<PlatformCookWorkItem> items = [];
        foreach (WiiUNativeTextureFormatDefinition format in WiiUNativeTextureFormatCatalog.Formats.Where(format => format.SupportsCooking)) {
            items.Add(Item($"cooked/images/{format.Id}.hetex", source, "texture", format.Id));
            items.Add(Item($"cooked/fonts/{format.Id}.hetex", fontSource, "font-atlas-texture", format.Id));
        }
        PlatformBuildRequest request = Request(root, items.ToArray());
        RecordingWiiUNativeBuildExecutor executor = new();
        var report = await new WiiUPlatformAssetBuilder(executor).BuildAsync(request, new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.True(report.Succeeded);
        Assert.NotEmpty(executor.LastProducedArtifactPath);
        foreach (PlatformCookWorkItem item in items) {
            string relative = item.OutputRelativePath.Replace('/', Path.DirectorySeparatorChar);
            byte[] staged = File.ReadAllBytes(Path.Combine(request.OutputRoot, "content", relative));
            Assert.Equal(staged, File.ReadAllBytes(Path.Combine(request.WorkingRoot, "package-source", relative)));
            using TextureAsset cooked = Assert.IsType<TextureAsset>(helengine.files.AssetSerializer.DeserializeFromBytes(staged));
            Assert.Equal(item.SourceAssetKind == "texture" ? "authored-image" : "authored-image#atlas", cooked.Id);
            Assert.Equal(item.SourceAssetKind == "texture" ? "authored-source" : string.Empty, cooked.AuthoringAssetId);
            Assert.Equal((ushort)5, cooked.Width);
            Assert.Equal((ushort)3, cooked.Height);
            TextureAssetProcessorSettings settings = WiiUTextureCookSettingsSerializer.Deserialize(item.SerializedPlatformSettings);
            WiiUNativeTextureFormatCatalog.TryGetFormat(settings.ColorFormatId, out WiiUNativeTextureFormatDefinition expected);
            Assert.Equal(expected.HardwareFormat, WiiUNativeTextureCodec.ReadLayout(cooked).Format.HardwareFormat);
        }
        Assert.Equal(original, File.ReadAllBytes(source));
        Assert.Equal(originalFont, File.ReadAllBytes(fontSource));
    }

    /// <summary>Loads each supported serialized extension without depending on the Windows raw-image importer.</summary>
    [Theory]
    [InlineData(".hetex")]
    [InlineData(".heasset")]
    [InlineData(".hasset")]
    public void SerializedSourceExtensionsPreserveAuthoredMetadata(string extension) {
        string root = PrepareFixture("serialized" + extension);
        string source = WriteSource(root, extension);
        using TextureAsset cooked = new WiiUPlatformCookSourceProcessor().CookTexture(source, "authored-image", WiiUTextureCookSettingsSerializer.CreateDefaults());
        Assert.Equal("authored-source", cooked.AuthoringAssetId);
        Assert.Equal(new[] { "former-source" }, cooked.FormerAuthoringAssetIds);
        Assert.Equal(TextureAssetColorFormat.Rgba32, cooked.ColorFormat);
    }

    /// <summary>Imports literal PNG rows without a Windows editor host, retaining RGBA, alpha and runtime identity.</summary>
    [Fact]
    public void PngSourcePreservesRgbaRowsAndRuntimeIdentity() {
        string root = PrepareFixture("portable-png");
        string source = Path.Combine(root, "source.png");
        byte[] png = Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVR4nGP4z8DwHwgbGIA0GAAARNIJeLaHI0sAAAAASUVORK5CYII=");
        File.WriteAllBytes(source, png);
        using TextureAsset cooked = new WiiUPlatformCookSourceProcessor().CookTexture(source, "portable-image", WiiUTextureCookSettingsSerializer.CreateDefaults());
        Assert.Equal("portable-image", cooked.Id);
        Assert.Equal(RuntimeAssetIdGenerator.Generate(cooked.Id), cooked.RuntimeAssetId);
        Assert.Equal(new byte[] { 255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 255, 255, 255, 255, 255 }, TextureAssetPixelCodec.DecodeToRgba32(cooked));
        Assert.Equal(png, File.ReadAllBytes(source));
    }

    /// <summary>Rasterizes an explicit source face with the existing default pixel request and cooks a nonempty atlas without Windows UI assemblies.</summary>
    [Fact]
    public void RawFontAtlasCooksWithoutWindowsEditorHost() {
        string source = Path.Combine(AppContext.BaseDirectory, "fixtures", "Synthetic.ttf");
        using TextureAsset cooked = new WiiUPlatformCookSourceProcessor().CookFontAtlasTexture(source, "portable-font", WiiUTextureCookSettingsSerializer.CreateDefaults());
        Assert.Equal("portable-font#atlas", cooked.Id);
        Assert.Equal(RuntimeAssetIdGenerator.Generate(cooked.Id), cooked.RuntimeAssetId);
        Assert.True(cooked.Width > 0 && cooked.Height > 0);
        Assert.Contains(TextureAssetPixelCodec.DecodeToRgba32(cooked), channel => channel != 0);
    }
    /// <summary>Copies an explicitly supplied native atlas and its font metadata without changing bytes or recooking the texture.</summary>
    [Fact]
    public async Task ExplicitNativeAtlasAndFontMetadataArePreserved() {
        string root = PrepareFixture("explicit-font-atlas");
        string fontSource = WriteFontSource(root, "font-source.hefont", "cooked/fonts/atlas.hetex");
        PlatformBuildRequest request = Request(root, []);
        string packageRoot = WiiUBuilderPaths.ResolvePackageSourceRootPath(request);
        string fontRelative = "cooked/fonts/body.hefont";
        string atlasRelative = "cooked/fonts/atlas.hetex";
        Directory.CreateDirectory(Path.Combine(packageRoot, "cooked/fonts"));
        File.Copy(fontSource, Path.Combine(packageRoot, fontRelative), true);
        using TextureAsset raw = WiiUNativeTextureCookTests.Source(5, 3);
        using TextureAsset native = new WiiUTextureCooker().CookTexture(raw, WiiUNativeTextureCookTests.Settings("WiiU.UNORM_R8_G8_B8_A8", TextureAssetAlphaPrecision.A8));
        byte[] atlasBytes = helengine.files.AssetSerializer.SerializeToBytes(native);
        File.WriteAllBytes(Path.Combine(packageRoot, atlasRelative), atlasBytes);
        request = Request(root, [], [new PlatformBuildArtifact(atlasRelative, "atlas", "hash", "texture", "default")],
            [new PlatformBuildAsset("font", "Font", "font", new PlatformBuildPayloadReference(fontRelative, fontRelative), [])]);
        var report = await new WiiUPlatformAssetBuilder(new RecordingWiiUNativeBuildExecutor()).BuildAsync(request, new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.True(report.Succeeded);
        Assert.Equal(atlasBytes, File.ReadAllBytes(Path.Combine(request.OutputRoot, "content", atlasRelative)));
        Assert.Equal(File.ReadAllBytes(fontSource), File.ReadAllBytes(Path.Combine(request.OutputRoot, "content", fontRelative)));
    }

    /// <summary>Rejects unsafe runtime paths before replacing any previous package or invoking the native executor.</summary>
    [Theory]
    [InlineData("../outside.hetex")]
    [InlineData("C:/outside.hetex")]
    [InlineData("bad.hetex:stream")]
    [InlineData("bad?.hetex")]
    [InlineData("bad|.hetex")]
    [InlineData("bad<.hetex")]
    [InlineData("bad>.hetex")]
    [InlineData("bad*.hetex")]
    [InlineData("bad\".hetex")]
    [InlineData("bad\u0001.hetex")]
    [InlineData("cooked/NUL.hetex")]
    [InlineData("cooked/file.hetex.")]
    [InlineData("cooked/a/../file.hetex")]
    public async Task UnsafeWorkItemPathPreservesPreviousOutput(string output) {
        string root = PrepareFixture("unsafe-path");
        string source = WriteSource(root, ".hetex");
        await AssertPreflightFailure(root, [Item(output, source)]);
    }

    /// <summary>Rejects case aliases and file/ancestor collisions across the complete batch before any cook is published.</summary>
    [Theory]
    [InlineData("cooked/shared.hetex", "COOKED/shared.hetex")]
    [InlineData("cooked/shared.hetex", "cooked/shared.hetex/child.hetex")]
    public async Task DuplicateAndAncestorWorkItemPathsAreRejected(string first, string second) {
        string root = PrepareFixture("colliding-items");
        string source = WriteSource(root, ".hetex");
        await AssertPreflightFailure(root, [Item(first, source), Item(second, source)]);
    }

    /// <summary>Does not overwrite a manifest-declared payload with a new cook targeting the same runtime path.</summary>
    [Fact]
    public async Task WorkItemConflictWithManifestPayloadPreservesOldOutput() {
        string root = PrepareFixture("manifest-conflict");
        string source = WriteSource(root, ".hetex");
        string packageRoot = Path.Combine(root, "working/package-source");
        Directory.CreateDirectory(Path.Combine(packageRoot, "cooked"));
        File.WriteAllBytes(Path.Combine(packageRoot, "cooked/shared.hetex"), File.ReadAllBytes(source));
        await AssertPreflightFailure(root, [Item("cooked/shared.hetex", source)], [new PlatformBuildArtifact("cooked/shared.hetex", "old", "hash", "texture", "default")]);
    }

    /// <summary>Retains previous output when a later source fails decoding after an earlier real native cook has succeeded in memory.</summary>
    [Fact]
    public async Task LaterMalformedSourceCannotPublishEarlierCook() {
        string root = PrepareFixture("later-cook-failure");
        string source = WriteSource(root, ".hetex");
        string broken = Path.Combine(root, "broken.hetex");
        File.WriteAllBytes(broken, [1, 2, 3]);
        await AssertPreflightFailure(root, [Item("cooked/first.hetex", source), Item("cooked/second.hetex", broken)]);
    }

    /// <summary>Runs a failing preparation against sentinels and verifies it never reaches the native build boundary.</summary>
    static async Task AssertPreflightFailure(string root, PlatformCookWorkItem[] items, PlatformBuildArtifact[] artifacts = null) {
        PlatformBuildRequest request = Request(root, items, artifacts);
        string outputSentinel = Path.Combine(request.OutputRoot, "previous.txt");
        string sourceSentinel = Path.Combine(request.WorkingRoot, "package-source/previous.txt");
        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(Path.GetDirectoryName(sourceSentinel));
        File.WriteAllText(outputSentinel, "previous");
        File.WriteAllText(sourceSentinel, "previous");
        RecordingWiiUNativeBuildExecutor executor = new();
        await Assert.ThrowsAnyAsync<Exception>(() => new WiiUPlatformAssetBuilder(executor).BuildAsync(request, new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None));
        Assert.Empty(executor.LastProducedArtifactPath);
        Assert.Equal("previous", File.ReadAllText(outputSentinel));
        Assert.Equal("previous", File.ReadAllText(sourceSentinel));
        Assert.False(File.Exists(Path.Combine(request.WorkingRoot, "package-source/cooked/first.hetex")));
    }

    /// <summary>Resets only an exactly resolved fixture child beneath the visible artifact root, never a user project or source tree.</summary>
    static string PrepareFixture(string name) {
        string fixtureRoot = Environment.GetEnvironmentVariable("HELENGINE_WIIU_TEXTURE_TEST_ROOT") ?? Path.Combine(AppContext.BaseDirectory, "wiiu-texture-fixtures");
        string root = Path.GetFullPath(Path.Combine(fixtureRoot, name));
        if (!root.StartsWith(Path.GetFullPath(fixtureRoot).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Fixture escaped its artifact root.");
        if (Directory.Exists(root)) Directory.Delete(root, true);
        Directory.CreateDirectory(root);
        return root;
    }

    /// <summary>Writes a real serialized texture with identity metadata for the source decoder and workitem cooker.</summary>
    static string WriteSource(string root, string extension) {
        string source = Path.Combine(root, "source" + extension);
        using TextureAsset raw = WiiUNativeTextureCookTests.Source(5, 3);
        File.WriteAllBytes(source, helengine.files.AssetSerializer.SerializeToBytes(raw));
        return source;
    }

    /// <summary>Writes a real font carrying an embedded source atlas and an explicit runtime atlas path.</summary>
    static string WriteFontSource(string root, string name, string atlasPath) {
        string source = Path.Combine(root, name);
        using TextureAsset raw = WiiUNativeTextureCookTests.Source(5, 3);
        using FontAsset font = new(new FontInfo("Body", 16, 8), null, new Dictionary<char, FontChar>(), 16, 5, 3) {
            SourceTextureAsset = raw, CookedAtlasTextureRelativePath = atlasPath
        };
        using (FileStream stream = File.Create(source)) helengine.files.FontAssetBinarySerializer.Serialize(stream, font);
        font.SourceTextureAsset = null;
        return source;
    }

    /// <summary>Builds a valid native image or atlas workitem with explicit source identity metadata.</summary>
    static PlatformCookWorkItem Item(string output, string source, string kind = "texture", string formatId = "WiiU.UNORM_R8_G8_B8_A8") {
        WiiUNativeTextureFormatCatalog.TryGetFormat(formatId, out WiiUNativeTextureFormatDefinition format);
        return new PlatformCookWorkItem(output, source, kind, "wiiu", "runtime-texture", output, output, "source", "settings",
            WiiUTextureCookSettingsSerializer.Serialize(WiiUNativeTextureCookTests.Settings(formatId, format.DefaultAlphaPrecision)), [new PlatformCookWorkItemMetadata("source-asset-id", "authored-image")]);
    }

    /// <summary>Uses the real manifest and builder contracts with stable workspace roots while injecting only the SDK execution boundary.</summary>
    static PlatformBuildRequest Request(string root, PlatformCookWorkItem[] items, PlatformBuildArtifact[] artifacts = null, PlatformBuildAsset[] assets = null, NumericPrecisionPolicy numericPolicy = null) {
        Dictionary<string, string> numericOptions = [];
        helengine.baseplatform.Definitions.PlatformNumericPolicyResolver.ApplyToOptions(numericPolicy ?? NumericPrecisionPolicy.Default, numericOptions);
        PlatformBuildManifest manifest = new(2, "project", "1.0.0", "1.0.0", "wiiu", "1.0.0", "", [], assets ?? [], artifacts ?? [], [], [],
            new PlatformContainerWritePlan("default", []), items, PlatformBuildRuntimeFeatureManifest.Empty);
        return new PlatformBuildRequest(manifest, [new PlatformBuildTargetVariant("wiiu-default", "wiiu", "wiiu", "wiiu-default")],
            [new PlatformCookProfile("wiiu-default", "Wii U", new PlatformCookProfileCapabilities("wiiu", "raw", "rgba", "scene", PlatformSerializationEndianness.BigEndian))],
            Path.Combine(root, "output"), Path.Combine(root, "working"), string.Empty, string.Empty, string.Empty, null, null, numericOptions);
    }
}
