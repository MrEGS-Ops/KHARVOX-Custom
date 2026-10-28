# Convert the user-supplied, locally embedded original MrEGS PNG to the
# .ico format required for KharvoxMods.exe. No external downloads or APIs.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$folder = Split-Path -Parent $MyInvocation.MyCommand.Path
$png = Join-Path $folder "mregs-logo.png"
$ico = Join-Path $folder "mregs.ico"
if (!(Test-Path -LiteralPath $png)) {
    throw "MrEGS original branding file not found: $png"
}
$image = [System.Drawing.Bitmap]::FromFile($png)
try {
    $hicon = $image.GetHicon()
    if ($hicon -eq [IntPtr]::Zero) { throw "Could not convert MrEGS logo to icon" }
    $icon = [System.Drawing.Icon]::FromHandle($hicon)
    try {
        $stream = [System.IO.File]::Create($ico)
        try { $icon.Save($stream) }
        finally { $stream.Dispose() }
    } finally { $icon.Dispose() }
} finally { $image.Dispose() }
if ((Get-Item -LiteralPath $ico).Length -lt 100) {
    throw "Generated MrEGS application icon is invalid"
}
Write-Host "Generated MrEGS application icon from user-provided screenshot."
