# Generate a multi-resolution Windows icon from the approved user-supplied
# original rEGS artwork. GetHicon() would quantize to one small 32x32
# image and can erase the small red underscore. Store PNG-compressed
# images at multiple sizes inside the ICO instead.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$folder = Split-Path -Parent $MyInvocation.MyCommand.Path
$png = Join-Path $folder "mregs-logo.png"
$ico = Join-Path $folder "mregs.ico"
if (!(Test-Path -LiteralPath $png)) { throw "MrEGS logo missing" }
$original = [System.Drawing.Bitmap]::FromFile($png)
try {
    $sizes = @(256, 128, 64, 48, 32, 16)
    $images = New-Object System.Collections.Generic.List[byte[]]
    foreach ($size in $sizes) {
        $bitmap = New-Object System.Drawing.Bitmap($size, $size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.InterpolationMode =
                    [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode =
                    [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($original, 0, 0, $size, $size)
            } finally { $graphics.Dispose() }
            # At 16/32 px the tiny red slash must survive downscaling.
            # It is in the lower right of the approved circle artwork.
            if ($size -le 32) {
                $red = [System.Drawing.Color]::FromArgb(255, 230, 0, 22)
                $y = [int][Math]::Round($size * 0.742)
                $x = [int][Math]::Round($size * 0.720)
                for ($i = 0; $i -lt [Math]::Max(2, [int]($size * 0.095)); $i++) {
                    $px = $x + $i
                    if ($px -lt $size) { $bitmap.SetPixel($px, $y, $red) }
                }
            }
            $memory = New-Object System.IO.MemoryStream
            try {
                $bitmap.Save($memory, [System.Drawing.Imaging.ImageFormat]::Png)
                $images.Add($memory.ToArray())
            } finally { $memory.Dispose() }
        } finally { $bitmap.Dispose() }
    }

    $file = [System.IO.File]::Create($ico)
    $writer = New-Object System.IO.BinaryWriter($file)
    try {
        $writer.Write([uint16]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        for ($i = 0; $i -lt $sizes.Count; $i++) {
            $size = $sizes[$i]
            $writer.Write([byte]($size % 256))
            $writer.Write([byte]($size % 256))
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]32)
            $writer.Write([uint32]$images[$i].Length)
            $writer.Write([uint32]$offset)
            $offset += $images[$i].Length
        }
        foreach ($image in $images) { $writer.Write([byte[]]$image) }
    } finally { $writer.Dispose() }

    $icon = New-Object System.Drawing.Icon($ico)
    try {
        if ($icon.Width -lt 16 || $icon.Height -lt 16) {
            throw "Generated MrEGS icon has invalid dimensions"
        }
    } finally { $icon.Dispose() }
} finally { $original.Dispose() }
Write-Host "Generated multi-size original MrEGS icon with preserved red accent."


# The updater is a separate tool: keep the approved MrEGS art but add a
# teal circular refresh mark to distinguish its Windows EXE icon.
$updater = Join-Path $folder "mregs-updater.ico"
$source = [System.Drawing.Bitmap]::FromFile($png)
try {
    $sizes = @(256, 128, 64, 48, 32, 16)
    $frames = New-Object System.Collections.Generic.List[byte[]]
    foreach ($size in $sizes) {
        $bitmap = New-Object System.Drawing.Bitmap($size, $size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.DrawImage($source, 0, 0, $size, $size)

                $badge = [Math]::Max(8, [int]($size * 0.48))
                $left = $size - $badge - [int]($size * 0.02)
                $top = $size - $badge - [int]($size * 0.02)
                $green = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 16, 160, 128))
                $outline = New-Object System.Drawing.Pen([System.Drawing.Color]::White,
                    [Math]::Max(1, [single]($size * 0.045)))
                try {
                    $graphics.FillEllipse($green, $left, $top, $badge, $badge)
                    $inset = [Math]::Max(2, [int]($badge * 0.24))
                    $arcSize = $badge - 2 * $inset
                    $graphics.DrawArc($outline, $left + $inset, $top + $inset,
                        $arcSize, $arcSize, 45, 275)
                    $ax = $left + $badge - $inset
                    $ay = $top + [int]($badge * 0.40)
                    $arrow = [Math]::Max(2, [int]($badge * 0.15))
                    $graphics.DrawLine($outline, $ax, $ay, $ax - $arrow, $ay)
                    $graphics.DrawLine($outline, $ax, $ay, $ax, $ay + $arrow)
                } finally {
                    $outline.Dispose()
                    $green.Dispose()
                }
            } finally { $graphics.Dispose() }
            $buffer = New-Object System.IO.MemoryStream
            try {
                $bitmap.Save($buffer, [System.Drawing.Imaging.ImageFormat]::Png)
                $frames.Add($buffer.ToArray())
            } finally { $buffer.Dispose() }
        } finally { $bitmap.Dispose() }
    }
    $output = [System.IO.File]::Create($updater)
    $writer = New-Object System.IO.BinaryWriter($output)
    try {
        $writer.Write([uint16]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        for ($i=0; $i -lt $sizes.Count; $i++) {
            $writer.Write([byte]($sizes[$i] % 256))
            $writer.Write([byte]($sizes[$i] % 256))
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$i].Length)
            $writer.Write([uint32]$offset)
            $offset += $frames[$i].Length
        }
        foreach ($frame in $frames) { $writer.Write([byte[]]$frame) }
    } finally { $writer.Dispose() }
} finally { $source.Dispose() }
Write-Host "Generated separate MrEGS updater icon with refresh marker."
