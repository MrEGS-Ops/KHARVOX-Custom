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
# Bitmap.GetHicon() can silently lose the red detail when the PNG uses
# an indexed palette. Render through a true-colour 32-bit ARGB bitmap first.
$image = [System.Drawing.Bitmap]::FromFile($png)
try {
    $argb = [System.Drawing.Bitmap]::new($image.Width, $image.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($argb)
        try {
            $graphics.Clear([System.Drawing.Color]::Black)
            $graphics.DrawImage($image, 0, 0, $argb.Width, $argb.Height)
        } finally { $graphics.Dispose() }
        $hicon = $argb.GetHicon()
        if ($hicon -eq [IntPtr]::Zero) { throw "Could not convert MrEGS logo to icon" }
        try {
            $icon = [System.Drawing.Icon]::FromHandle($hicon)
            try {
                $stream = [System.IO.File]::Create($ico)
                try { $icon.Save($stream) }
                finally { $stream.Dispose() }
            } finally { $icon.Dispose() }
        } finally {
            Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KharvoxIconHandle {
    [DllImport("user32.dll")] public static extern bool DestroyIcon(IntPtr icon);
}
'@ -ErrorAction SilentlyContinue
            [KharvoxIconHandle]::DestroyIcon($hicon) | Out-Null
        }
    } finally { $argb.Dispose() }
} finally { $image.Dispose() }
if ((Get-Item -LiteralPath $ico).Length -lt 100) {
    throw "Generated MrEGS application icon is invalid"
}
Write-Host "Generated MrEGS application icon from user-provided screenshot."
