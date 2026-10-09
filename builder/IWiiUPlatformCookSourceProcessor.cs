using helengine.editor;

namespace helengine.wiiu.builder;

/// <summary>Loads authoring or serialized image and atlas sources into owned Wii U runtime texture payloads.</summary>
public interface IWiiUPlatformCookSourceProcessor {
    /// <summary>Imports an image source and applies its explicit platform format and resize settings.</summary>
    TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);

    /// <summary>Detaches an owned source atlas from its font metadata and cooks only its texture payload.</summary>
    TextureAsset CookFontAtlasTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);
}
