param(
    [Parameter(Mandatory = $true)]
    [string]$RpxPath
)

$ErrorActionPreference = 'Stop'

$resolvedRpxPath = [System.IO.Path]::GetFullPath($RpxPath)
if (-not (Test-Path -LiteralPath $resolvedRpxPath)) {
    throw "Wii U RPX was not found: $resolvedRpxPath"
}

$repositoryRootPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cemuPath = 'C:\dev\helworks\emus\cemu-2.6-windows-x64\Cemu_2.6\Cemu.exe'
$userDir = Join-Path $repositoryRootPath 'tmp\cemu-launcher-user'

if (-not (Test-Path -LiteralPath $cemuPath)) {
    throw "Cemu executable was not found: $cemuPath"
}

$existingCemuProcesses = @(Get-Process -Name 'Cemu' -ErrorAction SilentlyContinue)
foreach ($process in $existingCemuProcesses) {
    Stop-Process -Id $process.Id -Force
}

New-Item -ItemType Directory -Force -Path $userDir | Out-Null
$rpxItem = Get-Item -LiteralPath $resolvedRpxPath

Write-Output ("RPX=" + $resolvedRpxPath)
Write-Output ("RPX_LAST_WRITE_TIME=" + $rpxItem.LastWriteTime.ToString('O'))
Write-Output ("CEMU=" + $cemuPath)
Write-Output ("USER_DIR=" + $userDir)

$process = Start-Process -FilePath $cemuPath -ArgumentList '-g', $resolvedRpxPath -WorkingDirectory $userDir -PassThru
Write-Output ("PROCESS_ID=" + $process.Id)
