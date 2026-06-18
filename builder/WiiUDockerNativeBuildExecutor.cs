using System.Diagnostics;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

namespace helengine.wiiu.builder;

/// <summary>
/// Invokes the Dockerized Wii U native build.
/// </summary>
public sealed class WiiUDockerNativeBuildExecutor : IWiiUNativeBuildExecutor {
    /// <summary>
    /// Builds the native Wii U RPX and returns the produced artifact path.
    /// </summary>
    /// <param name="request">Resolved platform build request.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter for streamed build failures.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the build cooperatively.</param>
    /// <returns>Absolute path to the produced RPX artifact.</returns>
    public async Task<string> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        }

        string repositoryRootPath = WiiUBuilderPaths.ResolveRepositoryRootPath();
        string generatedCoreRootPath = WiiUBuilderPaths.ResolveGeneratedCoreRootPath(request);
        ProcessStartInfo startInfo = CreateStartInfo(repositoryRootPath, generatedCoreRootPath);

        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Could not start the Wii U Docker build process.");
        Task<string> standardOutputTask = process.StandardOutput.ReadToEndAsync(cancellationToken);
        Task<string> standardErrorTask = process.StandardError.ReadToEndAsync(cancellationToken);
        while (!process.HasExited) {
            cancellationToken.ThrowIfCancellationRequested();
            process.WaitForExit(100);
        }

        await Task.WhenAll(standardOutputTask, standardErrorTask);
        if (process.ExitCode != 0) {
            throw new InvalidOperationException(
                "Wii U native RPX build failed."
                + Environment.NewLine
                + standardOutputTask.Result
                + Environment.NewLine
                + standardErrorTask.Result);
        }

        string builtRpxPath = WiiUBuilderPaths.ResolveBuiltRpxPath(request);
        if (!File.Exists(builtRpxPath)) {
            throw new FileNotFoundException("The native Wii U RPX was not produced by the Docker build.", builtRpxPath);
        }

        return builtRpxPath;
    }

    /// <summary>
    /// Creates the Docker process start info for one native Wii U build.
    /// </summary>
    /// <param name="repositoryRootPath">Absolute Wii U repository root.</param>
    /// <param name="generatedCoreRootPath">Absolute generated-core root that should be passed into the native build.</param>
    /// <returns>Configured Docker process start info.</returns>
    static ProcessStartInfo CreateStartInfo(string repositoryRootPath, string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(repositoryRootPath)) {
            throw new ArgumentException("Repository root path is required.", nameof(repositoryRootPath));
        } else if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path is required.", nameof(generatedCoreRootPath));
        }

        ProcessStartInfo startInfo = new() {
            FileName = "docker",
            WorkingDirectory = repositoryRootPath,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        string generatedCoreContainerPath = "/workspace/generated-core";
        startInfo.ArgumentList.Add("run");
        startInfo.ArgumentList.Add("--rm");
        startInfo.ArgumentList.Add("-v");
        startInfo.ArgumentList.Add(repositoryRootPath + ":/workspace");
        if (!IsPathUnderRoot(generatedCoreRootPath, repositoryRootPath)) {
            generatedCoreContainerPath = "/helengine-generated-core";
            startInfo.ArgumentList.Add("-v");
            startInfo.ArgumentList.Add(generatedCoreRootPath + ":" + generatedCoreContainerPath);
        }

        startInfo.ArgumentList.Add("-w");
        startInfo.ArgumentList.Add("/workspace");
        startInfo.ArgumentList.Add("-e");
        startInfo.ArgumentList.Add("HELENGINE_CORE_CPP_ROOT=" + generatedCoreContainerPath);
        startInfo.ArgumentList.Add(WiiUBuilderPaths.DockerImageName);
        startInfo.ArgumentList.Add("sh");
        startInfo.ArgumentList.Add("-lc");
        startInfo.ArgumentList.Add("make clean && make");
        return startInfo;
    }

    /// <summary>
    /// Returns true when one filesystem path is inside the supplied root path.
    /// </summary>
    /// <param name="candidatePath">Candidate filesystem path.</param>
    /// <param name="rootPath">Containing filesystem root path.</param>
    /// <returns>True when the candidate path is inside the supplied root path.</returns>
    static bool IsPathUnderRoot(string candidatePath, string rootPath) {
        if (string.IsNullOrWhiteSpace(candidatePath)) {
            throw new ArgumentException("Candidate path is required.", nameof(candidatePath));
        } else if (string.IsNullOrWhiteSpace(rootPath)) {
            throw new ArgumentException("Root path is required.", nameof(rootPath));
        }

        string fullCandidatePath = Path.GetFullPath(candidatePath);
        string fullRootPath = Path.GetFullPath(rootPath);
        string relativePath = Path.GetRelativePath(fullRootPath, fullCandidatePath);
        return !relativePath.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(relativePath);
    }
}
