namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the developer launcher contract for running explicit Wii U RPX builds in Cemu.
/// </summary>
public sealed class WiiUCemuLauncherScriptTests {
    /// <summary>
    /// Ensures the launcher keeps an explicit RPX path contract, force-closes Cemu, prints RPX timestamp data, and uses a dedicated launcher user profile.
    /// </summary>
    [Fact]
    public void CemuLauncher_KeepsExplicitRpxPathAndDedicatedProfileContract() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "scripts", "launch_wiiu_rpx_in_cemu.ps1");

        Assert.True(File.Exists(scriptPath), "Expected scripts/launch_wiiu_rpx_in_cemu.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[Parameter(Mandatory = $true)]", scriptSource, StringComparison.Ordinal);
        Assert.Contains("[string]$RpxPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Process -Name 'Cemu'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Stop-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Item -LiteralPath $resolvedRpxPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("LastWriteTime", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Cemu.exe", scriptSource, StringComparison.Ordinal);
        Assert.Contains("tmp\\cemu-launcher-user", scriptSource, StringComparison.Ordinal);
        Assert.Contains("'-g', $resolvedRpxPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PROCESS_ID=", scriptSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the README documents the editor CLI build flow and explicit Cemu launcher workflow.
    /// </summary>
    [Fact]
    public void Readme_DocumentsEditorCliBuildAndCemuLauncherWorkflow() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("build-platform.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-Platform wiiu", readmeSource, StringComparison.Ordinal);
        Assert.Contains("launch_wiiu_rpx_in_cemu.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-RpxPath", readmeSource, StringComparison.Ordinal);
        Assert.Contains("Cemu", readmeSource, StringComparison.Ordinal);
        Assert.Contains("process id", readmeSource, StringComparison.OrdinalIgnoreCase);
    }
}
