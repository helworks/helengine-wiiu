param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactPath
)

$ErrorActionPreference = 'Stop'

$resolvedArtifactPath = [System.IO.Path]::GetFullPath($ArtifactPath)
if (-not (Test-Path -LiteralPath $resolvedArtifactPath -PathType Leaf)) {
    throw "Wii U artifact was not found: $resolvedArtifactPath"
}

$artifactExtension = [System.IO.Path]::GetExtension($resolvedArtifactPath)
if ($artifactExtension -ine '.rpx' -and $artifactExtension -ine '.wuhb') {
    throw "Expected a .rpx or .wuhb artifact but got '$resolvedArtifactPath'."
}

$repositoryRootPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cemuPath = 'C:\dev\helworks\emus\cemu-2.6-windows-x64\Cemu_2.6\Cemu.exe'
$userDir = Join-Path $repositoryRootPath 'tmp\cemu-launcher-user'

if (-not (Test-Path -LiteralPath $cemuPath -PathType Leaf)) {
    throw "Cemu executable was not found: $cemuPath"
}

$existingCemuProcesses = @(Get-Process -Name 'Cemu' -ErrorAction SilentlyContinue)
foreach ($process in $existingCemuProcesses) {
    Stop-Process -Id $process.Id -Force
}

New-Item -ItemType Directory -Force -Path $userDir | Out-Null
$artifactItem = Get-Item -LiteralPath $resolvedArtifactPath

Write-Output ("ARTIFACT=" + $resolvedArtifactPath)
Write-Output ("ARTIFACT_LAST_WRITE_TIME=" + $artifactItem.LastWriteTime.ToString('O'))
Write-Output ("CEMU=" + $cemuPath)
Write-Output ("USER_DIR=" + $userDir)

$process = Start-Process -FilePath $cemuPath -ArgumentList '-g', $resolvedArtifactPath -WorkingDirectory $userDir -PassThru
Write-Output ("PROCESS_ID=" + $process.Id)
