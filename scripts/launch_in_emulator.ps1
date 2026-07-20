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
$cemuProfileUserRoot = 'C:\Users\Helena'
$cemuAppDataRoot = Join-Path $cemuProfileUserRoot 'AppData\Roaming'
$cemuLocalAppDataRoot = Join-Path $cemuProfileUserRoot 'AppData\Local'
$cemuProfilePath = Join-Path $cemuAppDataRoot 'Cemu'
$cemuWorkingDirectory = Split-Path -Parent $cemuPath

if (-not (Test-Path -LiteralPath $cemuPath -PathType Leaf)) {
    throw "Cemu executable was not found: $cemuPath"
}

if (-not (Test-Path -LiteralPath $cemuProfilePath -PathType Container)) {
    throw "Cemu profile was not found: $cemuProfilePath"
}

$existingCemuProcesses = @(Get-Process -Name 'Cemu' -ErrorAction SilentlyContinue)
foreach ($process in $existingCemuProcesses) {
    Stop-Process -Id $process.Id -Force
}

$artifactItem = Get-Item -LiteralPath $resolvedArtifactPath

Write-Output ("ARTIFACT=" + $resolvedArtifactPath)
Write-Output ("ARTIFACT_LAST_WRITE_TIME=" + $artifactItem.LastWriteTime.ToString('O'))
Write-Output ("CEMU=" + $cemuPath)
Write-Output ("CEMU_PROFILE=" + $cemuProfilePath)

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $cemuPath
$startInfo.Arguments = ('-g "' + $resolvedArtifactPath + '"')
$startInfo.WorkingDirectory = $cemuWorkingDirectory
$startInfo.UseShellExecute = $false
$startInfo.EnvironmentVariables['APPDATA'] = $cemuAppDataRoot
$startInfo.EnvironmentVariables['LOCALAPPDATA'] = $cemuLocalAppDataRoot
$startInfo.EnvironmentVariables['USERPROFILE'] = $cemuProfileUserRoot
$startInfo.EnvironmentVariables['HOMEDRIVE'] = [System.IO.Path]::GetPathRoot($cemuProfileUserRoot).TrimEnd('\')
$startInfo.EnvironmentVariables['HOMEPATH'] = $cemuProfileUserRoot.Substring(2)

$process = [System.Diagnostics.Process]::Start($startInfo)
Write-Output ("PROCESS_ID=" + $process.Id)
