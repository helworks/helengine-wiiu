using System.Reflection;
using helengine.editor;

namespace helengine.wiiu.builder;

/// <summary>
/// Imports source textures and source fonts through the shared editor import contracts for Wii U builder-owned cook work items.
/// </summary>
public sealed class WiiUPlatformCookSourceProcessor : IWiiUPlatformCookSourceProcessor {
    /// <summary>
    /// Stable metadata suffix appended to cooked font asset ids when generating their atlas texture ids.
    /// </summary>
    const string FontAtlasSuffix = "#atlas";

    /// <summary>
    /// Builder-owned cooker that preserves imported storage while applying generic or native Wii U settings.
    /// </summary>
    readonly WiiUTextureCooker TextureCooker;

    /// <summary>
    /// Lazily initialized default texture importer registrations provided by the portable editor backend.
    /// </summary>
    IReadOnlyList<TextureImporterRegistration> TextureImporterRegistrationsValue;

    /// <summary>
    /// Lazily initialized source font importer provided by the portable editor backend.
    /// </summary>
    IFontImporter FontImporterValue;

    /// <summary>
    /// Initializes the shared source processor used by Wii U builder-owned work items.
    /// </summary>
    public WiiUPlatformCookSourceProcessor() {
        TextureCooker = new WiiUTextureCooker();
    }

    /// <summary>
    /// Imports one source texture asset and applies the resolved Wii U texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved Wii U texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime texture payload ready for serialization.</returns>
    public TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Texture asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (!File.Exists(sourceAssetPath)) {
            throw new FileNotFoundException("Source texture file was not found.", sourceAssetPath);
        }

        using TextureAsset importedTextureAsset = LoadSourceTexture(sourceAssetPath);
        TextureAsset cookedTextureAsset = TextureCooker.CookTexture(importedTextureAsset, settings);
        cookedTextureAsset.Id = assetId;
        cookedTextureAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId);
        return cookedTextureAsset;
    }

    /// <summary>
    /// Imports one source font atlas asset and applies the resolved Wii U atlas texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked atlas texture should preserve.</param>
    /// <param name="settings">Resolved Wii U texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime atlas texture payload ready for serialization.</returns>
    public TextureAsset CookFontAtlasTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source font path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Font atlas asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (!File.Exists(sourceAssetPath)) {
            throw new FileNotFoundException("Source font file was not found.", sourceAssetPath);
        }

        using TextureAsset importedAtlasTextureAsset = LoadSourceFontAtlasTexture(sourceAssetPath);
        TextureAsset cookedAtlasAsset = TextureCooker.CookTexture(importedAtlasTextureAsset, settings);
        cookedAtlasAsset.Id = assetId + FontAtlasSuffix;
        cookedAtlasAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(cookedAtlasAsset.Id);
        return cookedAtlasAsset;
    }

    /// <summary>Loads serialized texture assets without Windows import dependencies, or imports one supported authoring image into owned storage.</summary>
    TextureAsset LoadSourceTexture(string sourceAssetPath) {
        string extension = Path.GetExtension(sourceAssetPath);
        if (extension.Equals(".hetex", StringComparison.OrdinalIgnoreCase) || extension.Equals(".hasset", StringComparison.OrdinalIgnoreCase) || extension.Equals(".heasset", StringComparison.OrdinalIgnoreCase)) {
            using FileStream serialized = File.OpenRead(sourceAssetPath);
            Asset asset = global::helengine.files.AssetSerializer.Deserialize(serialized);
            if (asset is TextureAsset texture) return texture;
            if (asset is IDisposable disposable) disposable.Dispose();
            throw new InvalidOperationException($"Serialized source '{sourceAssetPath}' is not a texture.");
        }
        TextureImporterRegistration importer = ResolveTextureImporter(sourceAssetPath);
        using FileStream stream = File.OpenRead(sourceAssetPath);
        return importer.Importer.ImportTexture(stream)
            ?? throw new InvalidOperationException($"Texture importer '{importer.ImporterId}' did not return a texture asset for '{sourceAssetPath}'.");
    }

    /// <summary>
    /// Resolves the default texture importer registration for one source asset path.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path.</param>
    /// <returns>Matching default texture importer registration.</returns>
    TextureImporterRegistration ResolveTextureImporter(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        }

        string normalizedExtension = NormalizeExtension(Path.GetExtension(sourceAssetPath));
        IReadOnlyList<TextureImporterRegistration> registrations = GetTextureImporterRegistrations();
        for (int registrationIndex = 0; registrationIndex < registrations.Count; registrationIndex++) {
            TextureImporterRegistration registration = registrations[registrationIndex];
            string[] extensions = registration.Extensions;
            for (int extensionIndex = 0; extensionIndex < extensions.Length; extensionIndex++) {
                if (string.Equals(extensions[extensionIndex], normalizedExtension, StringComparison.OrdinalIgnoreCase)) {
                    return registration;
                }
            }
        }

        throw new InvalidOperationException($"No default editor texture importer supports extension '{normalizedExtension}' for source texture '{sourceAssetPath}'.");
    }

    /// <summary>
    /// Loads one source font atlas asset from either a packaged <c>.hefont</c> document, one serialized texture-asset payload, one raw source texture file, or one raw source font file.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <returns>Imported or deserialized raw atlas texture asset.</returns>
    TextureAsset LoadSourceFontAtlasTexture(string sourceAssetPath) {
        if (string.Equals(Path.GetExtension(sourceAssetPath), ".hefont", StringComparison.OrdinalIgnoreCase)) {
            using FileStream stream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            using FontAsset fontAsset = global::helengine.files.FontAssetBinarySerializer.Deserialize(stream);
            if (fontAsset.SourceTextureAsset == null) {
                throw new InvalidOperationException($"Packaged font source '{sourceAssetPath}' did not provide an atlas texture payload.");
            }

            TextureAsset atlas = fontAsset.SourceTextureAsset;
            fontAsset.SourceTextureAsset = null;
            return atlas;
        }

        if (string.Equals(Path.GetExtension(sourceAssetPath), ".hasset", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(Path.GetExtension(sourceAssetPath), ".hetex", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(Path.GetExtension(sourceAssetPath), ".heasset", StringComparison.OrdinalIgnoreCase)) {
            return LoadSourceTexture(sourceAssetPath);
        }

        if (CanResolveTextureImporter(sourceAssetPath)) {
            return LoadSourceTexture(sourceAssetPath);
        }

        using FontAsset importedFontAsset = ImportRawFontAsset(sourceAssetPath);
        if (importedFontAsset.SourceTextureAsset == null) {
            throw new InvalidOperationException($"Source font '{sourceAssetPath}' did not provide an atlas texture payload.");
        }

        TextureAsset importedAtlas = importedFontAsset.SourceTextureAsset;
        importedFontAsset.SourceTextureAsset = null;
        return importedAtlas;
    }

    /// <summary>
    /// Returns whether one source path can be imported through the default editor texture importer registrations.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path.</param>
    /// <returns>True when one registered texture importer supports the source extension; otherwise false.</returns>
    bool CanResolveTextureImporter(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        }

        string normalizedExtension = NormalizeExtension(Path.GetExtension(sourceAssetPath));
        IReadOnlyList<TextureImporterRegistration> registrations = GetTextureImporterRegistrations();
        for (int registrationIndex = 0; registrationIndex < registrations.Count; registrationIndex++) {
            TextureImporterRegistration registration = registrations[registrationIndex];
            string[] extensions = registration.Extensions;
            for (int extensionIndex = 0; extensionIndex < extensions.Length; extensionIndex++) {
                if (string.Equals(extensions[extensionIndex], normalizedExtension, StringComparison.OrdinalIgnoreCase)) {
                    return true;
                }
            }
        }

        return false;
    }

    /// <summary>
    /// Imports one raw source font file through the editor host font importer.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <returns>Imported runtime font asset.</returns>
    FontAsset ImportRawFontAsset(string sourceAssetPath) {
        IFontImporter importer = GetFontImporter();
        using FileStream fontStream = new FileStream(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        FontAsset importedFontAsset = importer.ImportFont(fontStream, new FontAssetProcessorSettings {
            PixelSize = FontAssetProcessorSettings.DefaultPixelSize
        });
        if (importedFontAsset == null) {
            throw new InvalidOperationException($"Font importer '{importer.GetType().FullName}' did not return a font asset for '{sourceAssetPath}'.");
        }

        return importedFontAsset;
    }

    /// <summary>Gets the existing portable texture registrations, retaining their stable authored importer identifiers.</summary>
    /// <returns>Portable texture importer registrations.</returns>
    IReadOnlyList<TextureImporterRegistration> GetTextureImporterRegistrations() {
        return TextureImporterRegistrationsValue ??= PortableImporterFactory.CreateDefault()
            .OfType<TextureImporterRegistration>().ToArray();
    }

    /// <summary>Gets the existing file-backed font rasterizer without loading a Windows editor UI assembly.</summary>
    /// <returns>Portable source font importer honoring the supplied pixel request.</returns>
    IFontImporter GetFontImporter() {
        return FontImporterValue ??= new PortableFontImporter();
    }
    /// <summary>
    /// Normalizes one texture-file extension into a lowercase extension including the leading dot.
    /// </summary>
    /// <param name="extension">Extension to normalize.</param>
    /// <returns>Normalized extension.</returns>
    static string NormalizeExtension(string extension) {
        if (string.IsNullOrWhiteSpace(extension)) {
            return string.Empty;
        }

        return extension.StartsWith('.')
            ? extension.ToLowerInvariant()
            : "." + extension.ToLowerInvariant();
    }

}
