using helengine.baseplatform.Manifest;
using helengine.editor;

namespace helengine.wiiu.builder;

/// <summary>Prepares complete Wii U package bytes before publishing payloads or invoking the native bundle build.</summary>
public sealed class WiiUPackagedContentStager {
    /// <summary>Validates and cooks all manifest payloads before changing any destination file.</summary>
    public void Stage(PlatformBuildManifest manifest, string sourceRootPath, string destinationRootPath, NumericPrecisionPolicy numericPolicy = null) {
        IReadOnlyList<WiiUPreparedPayload> prepared = Prepare(manifest, sourceRootPath, destinationRootPath, CancellationToken.None, numericPolicy ?? NumericPrecisionPolicy.Default);
        Publish(prepared, destinationRootPath);
    }

    /// <summary>Reads all packaged inputs and executes image/atlas workitems in memory after validating the complete batch.</summary>
    internal IReadOnlyList<WiiUPreparedPayload> Prepare(PlatformBuildManifest manifest, string sourceRootPath, string destinationRootPath, CancellationToken cancellationToken, NumericPrecisionPolicy numericPolicy) {
        ArgumentNullException.ThrowIfNull(manifest);
        RealBinaryCodec.ValidatePolicy(numericPolicy);
        WiiUTextureCookPath.ValidateRoot(sourceRootPath);
        WiiUTextureCookPath.ValidateRoot(destinationRootPath);
        string sourceRoot = Path.GetFullPath(sourceRootPath);
        List<WiiUPreparedPayload> prepared = [];
        HashSet<string> reserved = new(StringComparer.OrdinalIgnoreCase);
        HashSet<string> sourcePaths = new(StringComparer.OrdinalIgnoreCase);
        foreach (PlatformBuildScene scene in manifest.Scenes) {
            foreach (PlatformBuildPayloadReference reference in scene.PayloadReferences) {
                AddSource(reference.SourceIdentity, sourceRoot, destinationRootPath, prepared, reserved, sourcePaths, cancellationToken);
            }
        }
        foreach (PlatformBuildAsset asset in manifest.LooseAssets) {
            AddSource(asset.PayloadReference.SourceIdentity, sourceRoot, destinationRootPath, prepared, reserved, sourcePaths, cancellationToken);
        }
        foreach (PlatformBuildArtifact artifact in manifest.CookedArtifacts) {
            AddSource(artifact.RelativePath, sourceRoot, destinationRootPath, prepared, reserved, sourcePaths, cancellationToken);
        }
        foreach (PlatformCookWorkItem item in manifest.PlatformCookWorkItems) {
            cancellationToken.ThrowIfCancellationRequested();
            if (item.TargetPlatformId != "wiiu" || item.TargetArtifactKind != "runtime-texture" || (item.SourceAssetKind != "texture" && item.SourceAssetKind != "font-atlas-texture")) {
                throw new InvalidOperationException($"Unsupported Wii U workitem contract '{item.WorkItemId}'.");
            }
            WiiUTextureCookSettingsSerializer.Deserialize(item.SerializedPlatformSettings);
            WiiUTextureCookPath.Reserve(reserved, item.OutputRelativePath);
            WiiUTextureCookPath.Resolve(sourceRoot, item.OutputRelativePath);
            WiiUTextureCookPath.Resolve(destinationRootPath, item.OutputRelativePath);
            ValidateSource(ResolveWorkItemSource(item.SourceAssetPath, sourceRoot));
            ResolveAssetId(item);
        }
        WiiUPlatformCookSourceProcessor processor = new();
        foreach (PlatformCookWorkItem item in manifest.PlatformCookWorkItems) {
            cancellationToken.ThrowIfCancellationRequested();
            TextureAssetProcessorSettings settings = WiiUTextureCookSettingsSerializer.Deserialize(item.SerializedPlatformSettings);
            string source = ResolveWorkItemSource(item.SourceAssetPath, sourceRoot);
            string assetId = ResolveAssetId(item);
            using TextureAsset texture = item.SourceAssetKind == "texture"
                ? processor.CookTexture(source, assetId, settings)
                : processor.CookFontAtlasTexture(source, assetId, settings);
            prepared.Add(new WiiUPreparedPayload {
                RelativePath = WiiUTextureCookPath.Normalize(item.OutputRelativePath),
                Bytes = global::helengine.files.AssetSerializer.SerializeToBytes(texture, numericPolicy)
            });
        }
        return prepared;
    }

    /// <summary>Writes only fully prepared bytes after every source and cook has passed preflight.</summary>
    internal void Publish(IReadOnlyList<WiiUPreparedPayload> prepared, string destinationRootPath) {
        foreach (WiiUPreparedPayload payload in prepared) {
            string destination = WiiUTextureCookPath.Resolve(destinationRootPath, payload.RelativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.WriteAllBytes(destination, payload.Bytes);
        }
    }

    /// <summary>Reads repeated runtime payloads once and reserves their paths against workitem and parent/child collisions.</summary>
    static void AddSource(string relativePath, string sourceRoot, string destinationRoot, List<WiiUPreparedPayload> prepared, HashSet<string> reserved, HashSet<string> sourcePaths, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        string normalized = WiiUTextureCookPath.Normalize(relativePath);
        if (!sourcePaths.Add(normalized)) return;
        WiiUTextureCookPath.Reserve(reserved, normalized);
        string source = WiiUTextureCookPath.Resolve(sourceRoot, normalized);
        WiiUTextureCookPath.Resolve(destinationRoot, normalized);
        ValidateSource(source);
        prepared.Add(new WiiUPreparedPayload { RelativePath = normalized, Bytes = File.ReadAllBytes(source) });
    }

    /// <summary>Accepts absolute editor source paths or safely contained package-relative serialized sources.</summary>
    static string ResolveWorkItemSource(string sourcePath, string sourceRoot) {
        return Path.IsPathRooted(sourcePath) ? Path.GetFullPath(sourcePath) : WiiUTextureCookPath.Resolve(sourceRoot, sourcePath);
    }

    /// <summary>Rejects missing or linked source files without changing previous package or executable output.</summary>
    static void ValidateSource(string sourcePath) {
        if (!File.Exists(sourcePath)) throw new FileNotFoundException("Wii U package source was not found.", sourcePath);
        string current = sourcePath;
        while (!string.IsNullOrEmpty(current)) {
            if ((File.Exists(current) || Directory.Exists(current)) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) {
                throw new InvalidOperationException("Wii U package source crosses a filesystem link.");
            }
            current = Path.GetDirectoryName(current);
        }
    }

    /// <summary>Uses explicit source identity metadata and rejects ambiguous values instead of inventing asset identifiers.</summary>
    static string ResolveAssetId(PlatformCookWorkItem item) {
        string assetId = item.OutputLogicalArtifactId;
        bool found = false;
        foreach (PlatformCookWorkItemMetadata metadata in item.Metadata) {
            if (metadata.Key != "source-asset-id") continue;
            if (found || string.IsNullOrWhiteSpace(metadata.Value)) throw new InvalidOperationException("Wii U workitem source-asset-id must be unique and nonempty.");
            found = true;
            assetId = metadata.Value;
        }
        return assetId;
    }
}
