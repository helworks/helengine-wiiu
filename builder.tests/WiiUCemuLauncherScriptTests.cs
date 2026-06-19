namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the canonical Wii U launcher contract for running explicit RPX builds in Cemu.
/// </summary>
public sealed class WiiUCemuLauncherScriptTests {
    /// <summary>
    /// Ensures the canonical launcher requires one explicit artifact path, force-closes Cemu, prints RPX timestamp data, and uses a dedicated launcher user profile.
    /// </summary>
    [Fact]
    public void Launcher_RequiresArtifactPath_AndKeepsDedicatedProfileContract() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "scripts", "launch_in_emulator.ps1");

        Assert.True(File.Exists(scriptPath), "Expected scripts/launch_in_emulator.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[Parameter(Mandatory = $true)]", scriptSource, StringComparison.Ordinal);
        Assert.Contains("[string]$ArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Process -Name 'Cemu'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Stop-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Item -LiteralPath $resolvedArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("LastWriteTime", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Cemu.exe", scriptSource, StringComparison.Ordinal);
        Assert.Contains("tmp\\cemu-launcher-user", scriptSource, StringComparison.Ordinal);
        Assert.Contains("'-g', $resolvedArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PROCESS_ID=", scriptSource, StringComparison.Ordinal);
        Assert.DoesNotContain("[string]$RpxPath", scriptSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the README documents the editor CLI build flow and canonical Cemu launcher workflow.
    /// </summary>
    [Fact]
    public void Readme_DocumentsCanonicalLauncherWorkflow() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("build-platform.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-Platform wiiu", readmeSource, StringComparison.Ordinal);
        Assert.Contains("launch_in_emulator.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-ArtifactPath", readmeSource, StringComparison.Ordinal);
        Assert.DoesNotContain("launch_wiiu_rpx_in_cemu.ps1", readmeSource, StringComparison.Ordinal);
    }
}
