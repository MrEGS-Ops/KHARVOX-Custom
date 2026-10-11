param(
    [Parameter(Mandatory=$true)][string]$PackageRoot,
    [switch]$WriteNotice,
    [switch]$RequireComplete
)
$ErrorActionPreference = "Stop"
$root = [IO.Path]::GetFullPath($PackageRoot)
$folder = Join-Path $root "sfs-profile"
$shaders = if (Test-Path -LiteralPath $folder -PathType Container) {
    @(Get-ChildItem -LiteralPath $folder -File -Filter "*.spv")
} else { @() }
if ($RequireComplete -and $shaders.Count -ne 75) {
    throw "Build packaging must contain all 75 SFS replacement shaders; found $($shaders.Count) in $folder"
}
if ($shaders.Count -ne 0 -and $shaders.Count -ne 75) {
    throw "Incomplete SFS profile: $($shaders.Count)/75 shader files in $folder. Do not publish as complete."
}
if ($shaders.Count -eq 75) {
    foreach ($shader in $shaders) {
        $file = [IO.File]::OpenRead($shader.FullName)
        try {
            if ($file.Length -lt 20 -or $file.Length % 4 -ne 0) {
                throw "Invalid SPIR-V file size: $($shader.Name)"
            }
            $magic = New-Object byte[] 4
            $read = $file.Read($magic,0,4)
            $header = [BitConverter]::ToUInt32($magic,0)
            if (($read -ne 4) -or ($header -ne 0x07230203)) {
                throw "Invalid SPIR-V header: $($shader.Name)"
            }
        } finally { $file.Dispose() }
    }
    Write-Host "SFS profile verified: 75/75 compiled shaders."
    if ($WriteNotice) {
        Remove-Item -LiteralPath (Join-Path $root "SFS-PROFILE-NOT-INCLUDED.txt") -ErrorAction SilentlyContinue
    }
    exit 0
}
$notice = @"
KHARVOX SFS PROFILE NOT INCLUDED IN THIS ARTIFACT

This native Windows build does not generate the 75 locally compiled SFS
shader replacements; the files are separately provisioned runtime inputs.
An SFS launch needs all 75 valid .spv files in the sfs-profile directory
alongside KharvoxLauncher.exe.

To use SFS, restore a COMPLETE verified matching sfs-profile directory
from an existing installation you own. Do not download arbitrary shaders,
guess file names, or remove the launch safety validation.

You can use AER without the SFS profile. The launcher will offer to
switch to AER if the selected SFS renderer has missing shaders.

Existing locally supplied shaders are retained by the watcher overlay.
"@
if ($WriteNotice) {
    Set-Content -LiteralPath (Join-Path $root "SFS-PROFILE-NOT-INCLUDED.txt") -Value $notice -Encoding UTF8
}
Write-Warning "SFS profile absent (0/75). This artifact is AER-capable; SFS needs separately provisioned shaders."
exit 0
