using helengine.editor;

namespace helengine.wiiu.builder;

/// <summary>Resizes a private RGBA copy before encoding the requested GX2 texture payload.</summary>
public sealed class WiiUTextureCooker {
    /// <summary>Shares the editor's aspect-preserving resize implementation with platform cooks.</summary>
    readonly TextureAssetProcessor Processor = new();

    /// <summary>Produces a new native texture while retaining authored identity and leaving source bytes intact.</summary>
    public TextureAsset CookTexture(TextureAsset source, TextureAssetProcessorSettings settings) {
        ArgumentNullException.ThrowIfNull(source);
        WiiUTextureCookSettingsSerializer.Validate(settings);
        TextureAsset rgba = new() {
            Width = source.Width, Height = source.Height,
            ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8,
            Colors = TextureAssetPixelCodec.DecodeToRgba32(source), PaletteColors = Array.Empty<byte>()
        };
        CopyIdentity(source, rgba);
        TextureAsset resized = null;
        TextureAsset result = null;
        try {
            resized = Processor.Apply(rgba, new TextureAssetProcessorSettings {
                MaxResolution = settings.MaxResolution, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8
            });
            if (resized.Width > 8192 || resized.Height > 8192) throw new InvalidOperationException("Wii U cooked texture dimensions cannot exceed 8192 pixels after resizing.");

            if (TextureAssetProcessorSettings.TryResolveGenericColorFormat(settings.ColorFormatId, out TextureAssetColorFormat genericFormat)) {
                result = Processor.Apply(resized, new TextureAssetProcessorSettings {
                    MaxResolution = 0, ColorFormat = genericFormat, AlphaPrecision = settings.AlphaPrecision, IndexingMethodId = settings.IndexingMethodId
                });
                CopyIdentity(source, result);
                return result;
            }
            result = WiiUNativeTextureCodec.Encode(resized, settings.ColorFormatId, settings.AlphaPrecision);
            CopyIdentity(source, result);
            return result;
        } finally {
            if (resized != null && !ReferenceEquals(resized, rgba) && !ReferenceEquals(resized, result)) resized.Dispose();
            if (!ReferenceEquals(rgba, result)) rgba.Dispose();
        }
    }

    /// <summary>Copies identity metadata while keeping the mutable alias-array container independent.</summary>
    internal static void CopyIdentity(TextureAsset source, TextureAsset destination) {
        destination.Id = source.Id;
        destination.RuntimeAssetId = source.RuntimeAssetId;
        destination.AuthoringAssetId = source.AuthoringAssetId;
        destination.FormerAuthoringAssetIds = source.FormerAuthoringAssetIds == null ? null : (string[])source.FormerAuthoringAssetIds.Clone();
        destination.IsEngineOwned = source.IsEngineOwned;
    }
}
