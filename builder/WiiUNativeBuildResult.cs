namespace helengine.wiiu.builder;

/// <summary>
/// Describes the native Wii U artifacts produced by one builder-native build invocation.
/// </summary>
public sealed class WiiUNativeBuildResult {
    /// <summary>
    /// Initializes one native build result with the produced RPX and WUHB artifact paths.
    /// </summary>
    /// <param name="rpxPath">Absolute path to the produced RPX artifact.</param>
    /// <param name="wuhbPath">Absolute path to the produced WUHB artifact.</param>
    public WiiUNativeBuildResult(string rpxPath, string wuhbPath) {
        if (string.IsNullOrWhiteSpace(rpxPath)) {
            throw new ArgumentException("A native Wii U RPX path is required.", nameof(rpxPath));
        } else if (string.IsNullOrWhiteSpace(wuhbPath)) {
            throw new ArgumentException("A native Wii U WUHB path is required.", nameof(wuhbPath));
        }

        RpxPath = rpxPath;
        WuhbPath = wuhbPath;
    }

    /// <summary>
    /// Gets the absolute path to the produced RPX artifact.
    /// </summary>
    public string RpxPath { get; }

    /// <summary>
    /// Gets the absolute path to the produced WUHB artifact.
    /// </summary>
    public string WuhbPath { get; }
}
