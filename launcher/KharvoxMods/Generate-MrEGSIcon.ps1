# Build a proper multi-resolution Windows .ico from the user-approved
# original gothic MrEGS PNG. The legacy Bitmap.GetHicon()/Icon.Save() path
# dithered away the small red underline, even when the bitmap was ARGB32.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$folder = Split-Path -Parent $MyInvocation.MyCommand.Path
$png = Join-Path $folder "mregs-logo.png"
$ico = Join-Path $folder "mregs.ico"
if (!(Test-Path -LiteralPath $png)) { throw "Missing original MrEGS logo: $png" }

$image = [System.Drawing.Bitmap]::FromFile($png)
try {
    $redX = -1
    $redY = -1
    for ($y = 0; $y -lt $image.Height; $y++) {
        for ($x = 0; $x -lt $image.Width; $x++) {
            $pixel = $image.GetPixel($x, $y)
            if ($pixel.R -gt 120 -and $pixel.R -gt (1.6 * $pixel.G) -and
                $pixel.R -gt (1.4 * $pixel.B)) {
                $redX = $x; $redY = $y
            }
        }
    }
    if ($redX -lt 0) { throw "Original MrEGS image is missing its red accent" }

    $sizes = @(16, 24, 32, 48, 64, 128, 256)
    $frames = New-Object 'System.Collections.Generic.List[byte[]]'
    foreach ($size in $sizes) {
        $bitmap = [System.Drawing.Bitmap]::new($size, $size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Black)
                $graphics.InterpolationMode =
                    [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode =
                    [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($image, 0, 0, $size, $size)
            } finally { $graphics.Dispose() }

            $foundRed = $false
            for ($y = 0; $y -lt $size -and !$foundRed; $y++) {
                for ($x = 0; $x -lt $size; $x++) {
                    $color = $bitmap.GetPixel($x, $y)
                    if ($color.R -gt 130 -and $color.R -gt (1.6 * $color.G) -and
                        $color.R -gt (1.4 * $color.B)) {
                        $foundRed = $true; break
                    }
                }
            }
            # Tiny 16/24px taskbar frames can average the ~1px red accent
            # away. Reinstate that pixel at its correctly scaled source
            # location; larger frames retain the original red geometry.
            if (!$foundRed) {
                $rx = [Math]::Min($size - 1, [Math]::Max(0,
                    [int][Math]::Floor($redX * $size / $image.Width)))
                $ry = [Math]::Min($size - 1, [Math]::Max(0,
                    [int][Math]::Floor($redY * $size / $image.Height)))
                $bitmap.SetPixel($rx, $ry,
                    [System.Drawing.Color]::FromArgb(230, 0, 22))
            }

            $pngStream = [System.IO.MemoryStream]::new()
            try {
                $bitmap.Save($pngStream,
                    [System.Drawing.Imaging.ImageFormat]::Png)
                $frames.Add($pngStream.ToArray())
            } finally { $pngStream.Dispose() }
        } finally { $bitmap.Dispose() }
    }

    $output = [System.IO.File]::Create($ico)
    try {
        $writer = [System.IO.BinaryWriter]::new($output)
        try {
            # ICONDIR: reserved=0, type=1, number of images.
            $writer.Write([uint16]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]$frames.Count)
            $offset = 6 + 16 * $frames.Count
            for ($i = 0; $i -lt $frames.Count; $i++) {
                $size = $sizes[$i]
                $writer.Write([byte]($size % 256)) # 0 means 256
                $writer.Write([byte]($size % 256))
                $writer.Write([byte]0) # true colour
                $writer.Write([byte]0) # reserved
                $writer.Write([uint16]1) # colour planes
                $writer.Write([uint16]32) # BGRA bit depth
                $writer.Write([uint32]$frames[$i].Length)
                $writer.Write([uint32]$offset)
                $offset += $frames[$i].Length
            }
            foreach ($frame in $frames) {
                $writer.Write([byte[]]$frame)
            }
        } finally { $writer.Dispose() }
    } finally { $output.Dispose() }
} finally { $image.Dispose() }

if ((Get-Item -LiteralPath $ico).Length -lt 1000) {
    throw "Generated MrEGS multi-size application icon is invalid"
}
Write-Host "Created seven-size MrEGS icon preserving original black/white ring and red accent."
