namespace helengine.wiiu.builder.tests;

/// <summary>
/// Verifies the checked-in CafeGLSL compiler against GPU-visible Wii U binding behavior.
/// </summary>
public sealed class CafeGlslCompilerIntegrationTests {
    /// <summary>
    /// Ensures an explicit GLSL uniform-block binding selects the same physical GX2 constant-buffer bank.
    /// </summary>
    [Fact]
    public async Task ExplicitUniformBlockBinding_SelectsMatchingHardwareConstantBuffer() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        System.Diagnostics.ProcessStartInfo processStartInfo = new System.Diagnostics.ProcessStartInfo {
            FileName = "docker",
            RedirectStandardError = true,
            RedirectStandardOutput = true,
            UseShellExecute = false
        };
        processStartInfo.ArgumentList.Add("run");
        processStartInfo.ArgumentList.Add("--rm");
        processStartInfo.ArgumentList.Add("-v");
        processStartInfo.ArgumentList.Add(string.Concat(repositoryRootPath, ":/workspace"));
        processStartInfo.ArgumentList.Add("-w");
        processStartInfo.ArgumentList.Add("/workspace");
        processStartInfo.ArgumentList.Add("helengine-wiiu");
        processStartInfo.ArgumentList.Add("sh");
        processStartInfo.ArgumentList.Add("-lc");
        processStartInfo.ArgumentList.Add("tools/cafeglsl/glslcompiler.elf -v -ps builder.tests/fixtures/cafeglsl_explicit_uniform_block.ps");

        using System.Diagnostics.Process process = System.Diagnostics.Process.Start(processStartInfo)
            ?? throw new InvalidOperationException("Failed to start the CafeGLSL compiler integration process.");
        Task<string> standardOutputTask = process.StandardOutput.ReadToEndAsync();
        Task<string> standardErrorTask = process.StandardError.ReadToEndAsync();
        await process.WaitForExitAsync();
        string compilerOutput = string.Concat(await standardOutputTask, await standardErrorTask);

        Assert.Equal(0, process.ExitCode);
        Assert.Contains("CB7", compilerOutput, StringComparison.Ordinal);
        Assert.DoesNotContain("CB0", compilerOutput, StringComparison.Ordinal);
    }
}
