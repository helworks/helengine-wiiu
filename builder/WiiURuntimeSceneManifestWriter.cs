using System.Text;
using helengine.baseplatform.Manifest;

namespace helengine.wiiu.builder;

/// <summary>
/// Writes the packaged Wii U startup-scene and scene-catalog manifest consumed by the native Wii U scene bootstrap.
/// </summary>
public sealed class WiiURuntimeSceneManifestWriter {
    /// <summary>
    /// Writes the packaged runtime scene manifest into the generated-core runtime folder.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root that receives the runtime manifest.</param>
    /// <param name="manifest">Resolved build manifest that defines the startup scene and cooked scene metadata.</param>
    public void Write(string generatedCoreRootPath, PlatformBuildManifest manifest) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        string runtimeRootPath = Path.Combine(generatedCoreRootPath, "runtime");
        Directory.CreateDirectory(runtimeRootPath);
        Write(
            Path.Combine(runtimeRootPath, "wiiu_runtime_scene_manifest.hpp"),
            ResolveStartupSceneId(manifest),
            ResolveSceneEntries(manifest));
    }

    /// <summary>
    /// Writes one header-only packaged runtime scene manifest to the supplied output path.
    /// </summary>
    /// <param name="outputPath">Absolute path that receives the generated manifest header.</param>
    /// <param name="startupSceneId">Authored startup scene id that runtime-backed builds must expose.</param>
    /// <param name="sceneEntries">Packaged scene catalog entries keyed by scene id and cooked relative path.</param>
    public void Write(string outputPath, string startupSceneId, IReadOnlyList<KeyValuePair<string, string>> sceneEntries) {
        if (string.IsNullOrWhiteSpace(outputPath)) {
            throw new ArgumentException("Output path must be provided.", nameof(outputPath));
        } else if (string.IsNullOrWhiteSpace(startupSceneId)) {
            throw new ArgumentException("Startup scene id must be provided.", nameof(startupSceneId));
        } else if (sceneEntries == null) {
            throw new ArgumentNullException(nameof(sceneEntries));
        } else if (sceneEntries.Count == 0) {
            throw new InvalidOperationException("At least one packaged scene entry is required.");
        }

        string outputDirectoryPath = Path.GetDirectoryName(outputPath) ?? throw new InvalidOperationException("Output directory path could not be resolved.");
        Directory.CreateDirectory(outputDirectoryPath);
        File.WriteAllText(outputPath, BuildHeaderContents(startupSceneId, sceneEntries));
    }

    /// <summary>
    /// Builds the header-only packaged runtime manifest contents consumed by the native Wii U scene bootstrap.
    /// </summary>
    /// <param name="startupSceneId">Authored startup scene id that runtime-backed builds must expose.</param>
    /// <param name="sceneEntries">Packaged scene catalog entries keyed by scene id and cooked relative path.</param>
    /// <returns>Header source for the packaged runtime scene manifest.</returns>
    static string BuildHeaderContents(string startupSceneId, IReadOnlyList<KeyValuePair<string, string>> sceneEntries) {
        StringBuilder builder = new();
        builder.AppendLine("#pragma once");
        builder.AppendLine();
        builder.AppendLine("#include <cstddef>");
        builder.AppendLine();
        builder.AppendLine("struct HEWiiURuntimeSceneEntry {");
        builder.AppendLine("    const char* SceneId;");
        builder.AppendLine("    const char* CookedRelativePath;");
        builder.AppendLine("};");
        builder.AppendLine();
        builder.AppendLine("static const char kRuntimeWiiUStartupSceneId[] = \"" + EscapeCpp(startupSceneId) + "\";");
        builder.AppendLine("static const HEWiiURuntimeSceneEntry kRuntimeWiiUSceneEntries[] = {");
        for (int index = 0; index < sceneEntries.Count; index++) {
            KeyValuePair<string, string> sceneEntry = sceneEntries[index];
            builder.AppendLine("    { \"" + EscapeCpp(sceneEntry.Key) + "\", \"" + EscapeCpp(sceneEntry.Value) + "\" },");
        }

        builder.AppendLine("};");
        builder.AppendLine("static const std::size_t kRuntimeWiiUSceneEntryCount = sizeof(kRuntimeWiiUSceneEntries) / sizeof(kRuntimeWiiUSceneEntries[0]);");
        builder.AppendLine();
        builder.AppendLine("inline const char* he_get_runtime_wiiu_startup_scene_id() {");
        builder.AppendLine("    return kRuntimeWiiUStartupSceneId;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("inline const HEWiiURuntimeSceneEntry* he_get_runtime_wiiu_scene_entries(std::size_t* count) {");
        builder.AppendLine("    if (count != nullptr) {");
        builder.AppendLine("        *count = kRuntimeWiiUSceneEntryCount;");
        builder.AppendLine("    }");
        builder.AppendLine();
        builder.AppendLine("    return kRuntimeWiiUSceneEntries;");
        builder.AppendLine("}");
        return builder.ToString();
    }

    /// <summary>
    /// Resolves the startup scene id from the build manifest.
    /// </summary>
    /// <param name="manifest">Resolved build manifest.</param>
    /// <returns>Startup scene id that runtime-backed builds must expose.</returns>
    static string ResolveStartupSceneId(PlatformBuildManifest manifest) {
        if (string.IsNullOrWhiteSpace(manifest.StartupSceneId)) {
            throw new InvalidOperationException("The build manifest did not define a Wii U startup scene id.");
        }

        return manifest.StartupSceneId;
    }

    /// <summary>
    /// Resolves the packaged scene entries from the build manifest.
    /// </summary>
    /// <param name="manifest">Resolved build manifest.</param>
    /// <returns>Packaged scene entries keyed by scene id and cooked relative path.</returns>
    static IReadOnlyList<KeyValuePair<string, string>> ResolveSceneEntries(PlatformBuildManifest manifest) {
        List<KeyValuePair<string, string>> sceneEntries = [];
        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildScene scene = manifest.Scenes[index];
            sceneEntries.Add(new KeyValuePair<string, string>(scene.SceneId, ResolveCookedRelativePath(scene)));
        }

        if (sceneEntries.Count == 0) {
            throw new InvalidOperationException("The build manifest did not define any Wii U packaged scene entries.");
        }

        return sceneEntries;
    }

    /// <summary>
    /// Resolves the cooked-relative scene path from resolved scene metadata.
    /// </summary>
    /// <param name="scene">Resolved scene entry.</param>
    /// <returns>Cooked-relative path used by the runtime scene catalog.</returns>
    static string ResolveCookedRelativePath(PlatformBuildScene scene) {
        for (int index = 0; index < scene.ResolvedMetadata.Length; index++) {
            KeyValuePair<string, string> metadataEntry = scene.ResolvedMetadata[index];
            if (string.Equals(metadataEntry.Key, "cooked-relative-path", StringComparison.OrdinalIgnoreCase)
                && !string.IsNullOrWhiteSpace(metadataEntry.Value)) {
                return metadataEntry.Value.Replace('\\', '/');
            }
        }

        throw new InvalidOperationException($"The scene '{scene.SceneId}' did not define a cooked-relative-path metadata entry.");
    }

    /// <summary>
    /// Escapes one string value for embedding inside a C++ string literal.
    /// </summary>
    /// <param name="value">String value to escape.</param>
    /// <returns>Escaped C++ string-literal contents.</returns>
    static string EscapeCpp(string value) {
        return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }
}
