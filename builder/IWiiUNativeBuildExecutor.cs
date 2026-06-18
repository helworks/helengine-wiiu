using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

namespace helengine.wiiu.builder;

/// <summary>
/// Abstracts the native Wii U build step used by the editor-facing builder workspace.
/// </summary>
public interface IWiiUNativeBuildExecutor {
    /// <summary>
    /// Builds the native Wii U player and returns the produced RPX path.
    /// </summary>
    /// <param name="request">Resolved platform build request.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter for streamed build failures.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the build cooperatively.</param>
    /// <returns>Absolute path to the produced RPX artifact.</returns>
    Task<string> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken);
}
