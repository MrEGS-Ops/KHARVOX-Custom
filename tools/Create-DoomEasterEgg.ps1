param(
    [Parameter(Mandatory = $true)]
    [string] $Video,
    [double] $StartSeconds = 0,
    [string] $LauncherDirectory = (Join-Path $PSScriptRoot "..\launcher\KharvoxLauncher\bin\Release\net48")
)
$ErrorActionPreference = "Stop"
$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
if (-not $ffmpeg) { throw "ffmpeg must be installed and on PATH." }
$source = (Resolve-Path -LiteralPath $Video).Path
$destinationDir = Join-Path $LauncherDirectory "assets\easteregg"
New-Item -Path $destinationDir -ItemType Directory -Force | Out-Null
$destination = Join-Path $destinationDir "e1m1_outline.gif"
$filters = "fps=12,scale=512:-2:flags=area,format=gray,edgedetect=low=0.06:high=0.16"
& $ffmpeg.Source -hide_banner -loglevel error -y -ss $StartSeconds -i $source -t 26 -an -vf $filters -loop 0 $destination
if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed with exit code $LASTEXITCODE." }
Write-Host "Generated local outline clip: $destination"
Write-Host "Copy this assets\easteregg folder beside the KHARVOX launcher executable."
