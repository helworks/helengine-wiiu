namespace helengine.wiiu.builder.tests;

/// <summary>
/// Resolves checked-in Wii U test source paths independently of the test runner's output and working directories.
/// </summary>
public static class WiiUTestSourcePaths {
    /// <summary>
    /// Resolves the Wii U repository root from the compile-time path of the calling test source file.
    /// </summary>
    /// <param name="sourceFilePath">Compile-time path supplied for the calling test source file.</param>
    /// <returns>Absolute path to the Wii U repository root.</returns>
    public static string ResolveRepositoryRootPath([System.Runtime.CompilerServices.CallerFilePath] string sourceFilePath = "") {
        string testSourceDirectoryPath = Path.GetDirectoryName(sourceFilePath)
            ?? throw new InvalidOperationException("The Wii U test source directory could not be resolved.");
        return Path.GetFullPath(Path.Combine(testSourceDirectoryPath, ".."));
    }
}
