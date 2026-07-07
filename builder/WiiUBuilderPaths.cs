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
    /// Stable WUHB file name emitted by the native Wii U build.
    /// </summary>
    public const string WuhbFileName = "helengine_wiiu.wuhb";

    /// <summary>
    /// Stable Docker image name used by the native Wii U build.
    /// </summary>
    public const string DockerImageName = "helengine-wiiu";

    /// <summary>
    /// Stable builder-working-root directory name that contains the copied package source tree.
    /// </summary>
    public const string PackageSourceDirectoryName = "package-source";

    /// <summary>
    /// Resolves the repository root for the currently running Wii U builder assembly.
    /// </summary>
    /// <returns>Absolute repository root path.</returns>
    public static string ResolveRepositoryRootPath() {
        string assemblyDirectoryPath = Path.GetDirectoryName(typeof(WiiUBuilderPaths).Assembly.Location)
            ?? throw new InvalidOperationException("The Wii U builder assembly directory could not be resolved.");
        return Path.GetFullPath(Path.Combine(assemblyDirectoryPath, "..", "..", "..", ".."));
    }

    /// <summary>
    /// Resolves the generated-core root used to pass packaged runtime metadata into the native Wii U build.
    /// </summary>
    /// <param name="request">Resolved platform build request.</param>
    /// <returns>Absolute generated-core root path for the current build request.</returns>
    public static string ResolveGeneratedCoreRootPath(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        if (!string.IsNullOrWhiteSpace(request.GeneratedCoreCppRootPath)) {
            return request.GeneratedCoreCppRootPath;
        }

        return Path.Combine(request.WorkingRoot, "generated-core");
    }

    /// <summary>
    /// Resolves the builder package-source root that mirrors the editor-staged package tree.
    /// </summary>
    /// <param name="request">Resolved platform build request.</param>
    /// <returns>Absolute path to the builder package-source root.</returns>
    public static string ResolvePackageSourceRootPath(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        return Path.Combine(request.WorkingRoot, PackageSourceDirectoryName);
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

    /// <summary>
    /// Resolves the WUHB path emitted by the native Wii U Makefile build.
    /// </summary>
    /// <param name="request">Resolved platform build request.</param>
    /// <returns>Absolute path to the repo-built WUHB artifact.</returns>
    public static string ResolveBuiltWuhbPath(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        return Path.Combine(ResolveRepositoryRootPath(), "build", WuhbFileName);
    }
}
