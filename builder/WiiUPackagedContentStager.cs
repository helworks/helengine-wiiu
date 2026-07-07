using helengine.baseplatform.Manifest;

namespace helengine.wiiu.builder;

/// <summary>
/// Copies packaged runtime payloads from the builder package-source root into the Wii U output content tree.
/// </summary>
public sealed class WiiUPackagedContentStager {
    /// <summary>
    /// Stages the payloads referenced by the supplied manifest into the Wii U packaged content root.
    /// </summary>
    /// <param name="manifest">Build manifest that identifies the payloads to stage.</param>
    /// <param name="sourceRootPath">Builder package-source root that already contains the payload files.</param>
    /// <param name="destinationRootPath">Destination content root that should receive the copied payload files.</param>
    public void Stage(PlatformBuildManifest manifest, string sourceRootPath, string destinationRootPath) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        } else if (string.IsNullOrWhiteSpace(sourceRootPath)) {
            throw new ArgumentException("Wii U packaged-content source root path must be provided.", nameof(sourceRootPath));
        } else if (string.IsNullOrWhiteSpace(destinationRootPath)) {
            throw new ArgumentException("Wii U packaged-content destination root path must be provided.", nameof(destinationRootPath));
        }

        string fullSourceRootPath = Path.GetFullPath(sourceRootPath);
        if (!Directory.Exists(fullSourceRootPath)) {
            if (!HasReferencedPayloads(manifest)) {
                return;
            }

            throw new DirectoryNotFoundException($"Wii U packaged-content source root '{fullSourceRootPath}' was not found.");
        }

        string fullDestinationRootPath = Path.GetFullPath(destinationRootPath);
        string sourceRootPrefix = EnsureTrailingDirectorySeparator(fullSourceRootPath);
        HashSet<string> stagedRelativePaths = new(StringComparer.OrdinalIgnoreCase);

        Directory.CreateDirectory(fullDestinationRootPath);

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildPayloadReference[] payloadReferences = manifest.Scenes[index].PayloadReferences;
            for (int payloadIndex = 0; payloadIndex < payloadReferences.Length; payloadIndex++) {
                StagePayloadReference(payloadReferences[payloadIndex], sourceRootPrefix, fullDestinationRootPath, stagedRelativePaths);
            }
        }

        for (int index = 0; index < manifest.LooseAssets.Length; index++) {
            StagePayloadReference(manifest.LooseAssets[index].PayloadReference, sourceRootPrefix, fullDestinationRootPath, stagedRelativePaths);
        }

        PlatformCookWorkItem[] platformCookWorkItems = manifest.PlatformCookWorkItems ?? [];
        for (int index = 0; index < platformCookWorkItems.Length; index++) {
            StageRelativePath(platformCookWorkItems[index].OutputRelativePath, sourceRootPrefix, fullDestinationRootPath, stagedRelativePaths);
        }
    }

    /// <summary>
    /// Returns whether the manifest references any payload files that require packaged-content staging.
    /// </summary>
    /// <param name="manifest">Manifest whose payload references should be inspected.</param>
    /// <returns>True when the manifest references any packaged payloads; otherwise false.</returns>
    static bool HasReferencedPayloads(PlatformBuildManifest manifest) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            if (manifest.Scenes[index].PayloadReferences.Length > 0) {
                return true;
            }
        }

        if (manifest.LooseAssets.Length > 0) {
            return true;
        }

        return (manifest.PlatformCookWorkItems ?? []).Length > 0;
    }

    /// <summary>
    /// Stages one payload reference into the destination content tree when it has not already been copied.
    /// </summary>
    /// <param name="payloadReference">Payload reference that identifies one packaged file.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="destinationRootPath">Destination content root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StagePayloadReference(
        PlatformBuildPayloadReference payloadReference,
        string sourceRootPrefix,
        string destinationRootPath,
        HashSet<string> stagedRelativePaths) {
        if (payloadReference == null) {
            throw new ArgumentNullException(nameof(payloadReference));
        }

        StageRelativePath(payloadReference.SourceIdentity, sourceRootPrefix, destinationRootPath, stagedRelativePaths);
    }

    /// <summary>
    /// Stages one relative output path into the destination content tree when it has not already been copied.
    /// </summary>
    /// <param name="relativePath">Runtime-relative path that should be copied from the package source root.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="destinationRootPath">Destination content root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StageRelativePath(
        string relativePath,
        string sourceRootPrefix,
        string destinationRootPath,
        HashSet<string> stagedRelativePaths) {
        if (stagedRelativePaths == null) {
            throw new ArgumentNullException(nameof(stagedRelativePaths));
        }

        string normalizedRelativePath = NormalizeRelativePath(relativePath);
        if (!stagedRelativePaths.Add(normalizedRelativePath)) {
            return;
        }

        string sourceRootPath = sourceRootPrefix.TrimEnd(Path.DirectorySeparatorChar);
        string sourceFilePath = Path.GetFullPath(Path.Combine(sourceRootPath, normalizedRelativePath.Replace('/', Path.DirectorySeparatorChar)));
        if (!sourceFilePath.StartsWith(sourceRootPrefix, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException("Wii U packaged-content payload paths must stay inside the package source root.");
        } else if (!File.Exists(sourceFilePath)) {
            throw new InvalidOperationException($"Wii U packaged-content payload '{normalizedRelativePath}' was not found in the package source root.");
        }

        string destinationFilePath = Path.Combine(destinationRootPath, normalizedRelativePath.Replace('/', Path.DirectorySeparatorChar));
        string destinationDirectoryPath = Path.GetDirectoryName(destinationFilePath)
            ?? throw new InvalidOperationException("Unable to resolve the Wii U packaged-content destination directory.");
        Directory.CreateDirectory(destinationDirectoryPath);
        File.Copy(sourceFilePath, destinationFilePath, overwrite: true);
    }

    /// <summary>
    /// Normalizes one payload path to a forward-slash relative path.
    /// </summary>
    /// <param name="path">Payload path to normalize.</param>
    /// <returns>Normalized forward-slash relative path.</returns>
    static string NormalizeRelativePath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new InvalidOperationException("Wii U packaged-content payload paths must be provided.");
        }

        string normalizedPath = path.Replace('\\', '/');
        if (Path.IsPathRooted(normalizedPath)) {
            throw new InvalidOperationException("Wii U packaged-content payload paths must not be rooted.");
        } else if (normalizedPath.StartsWith("../", StringComparison.Ordinal) || normalizedPath.Contains("/../", StringComparison.Ordinal)) {
            throw new InvalidOperationException("Wii U packaged-content payload paths must stay inside the package source root.");
        }

        return normalizedPath;
    }

    /// <summary>
    /// Ensures a path ends with one directory separator.
    /// </summary>
    /// <param name="path">Path to normalize.</param>
    /// <returns>Path with a trailing directory separator.</returns>
    static string EnsureTrailingDirectorySeparator(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new ArgumentException("Path must be provided.", nameof(path));
        }

        return path.EndsWith(Path.DirectorySeparatorChar)
            ? path
            : path + Path.DirectorySeparatorChar;
    }
}
