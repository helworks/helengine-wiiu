using helengine.baseplatform.Requests;

namespace helengine.wiiu.builder;

/// <summary>
/// Resolves the well-known Wii U repository and output paths used by the first builder slice.
/// </summary>
public static class WiiUBuilderPaths {
    /// <summary>
    /// Stable RPX file name emitted by the native Wii U build.
    /// </summary>
    public const string RpxFileName = "helengine_wiiu.rpx";

    /// <summary>
    /// Stable Docker image name used by the native Wii U build.
    /// </summary>
    public const string DockerImageName = "helengine-wiiu";

    /// <summary>
    /// Resolves the repository root for the currently running Wii U builder assembly.
    /// </summary>
    /// <returns>Absolute repository root path.</returns>
    public static string ResolveRepositoryRootPath() {
        return Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    }

    /// <summary>
    /// Resolves the RPX path emitted by the native Wii U Makefile build.
    /// </summary>
    /// <param name="request">Resolved platform build request.</param>
    /// <returns>Absolute path to the repo-built RPX artifact.</returns>
    public static string ResolveBuiltRpxPath(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        return Path.Combine(ResolveRepositoryRootPath(), "build", RpxFileName);
    }
}
