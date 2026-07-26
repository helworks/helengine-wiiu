namespace helengine.wiiu.builder;

/// <summary>
/// Rejects includes because platform shader cooking receives one fully resolved authored source file.
/// </summary>
public sealed class WiiUShaderIncludeResolver : IShaderIncludeResolver {
    /// <summary>
    /// Reports an unsupported include request with the requesting source information.
    /// </summary>
    /// <param name="requestingFile">Source file that requested the include.</param>
    /// <param name="includePath">Include path requested by the compiler.</param>
    /// <returns>Never returns because source includes must have been resolved before platform cooking.</returns>
    public ShaderIncludeResult Resolve(string requestingFile, string includePath) {
        throw new InvalidOperationException($"Wii U shader source '{requestingFile}' contains unresolved include '{includePath}'.");
    }
}
