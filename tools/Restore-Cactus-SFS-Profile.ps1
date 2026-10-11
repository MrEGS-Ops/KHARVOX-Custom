param(
    [string]$InstallRoot = (Get-Location).Path,
    [string]$ReleaseZip = ''
)

# Restore KHARVOX's original SFS profile for personal use, using the original
# developer's published release. No executables, DLLs or user mods are copied.
# Tested source: CactusVRStudios/KHARVOX v1.1.1, 2026-09-29.
$ErrorActionPreference = 'Stop'
$sourceUrl = 'https://github.com/CactusVRStudios/KHARVOX/releases/download/v1.1.1/KHARVOX-v1.11.zip'
$expectedSha256 = '79666be1c330b7a737c2e2766b2207557ace7c9b453194972719ef3693dd4dc5'
$root = [IO.Path]::GetFullPath($InstallRoot)
$launcher = Join-Path $root 'KharvoxLauncher.exe'
if (!(Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "KharvoxLauncher.exe was not found in '$root'. Pass -InstallRoot with your extracted KHARVOX installation folder."
}

$work = Join-Path ([IO.Path]::GetTempPath()) ('KHARVOX-SFS-' + [Guid]::NewGuid().ToString('N'))
$stage = Join-Path $root ('.sfs-profile-stage-' + [Guid]::NewGuid().ToString('N'))
$destination = Join-Path $root 'sfs-profile'
$backup = $null
$backupMade = $false
$installed = $false
$archive = $null

try {
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    $zipPath = Join-Path $work 'KHARVOX-v1.11.zip'
    if ($ReleaseZip) {
        $sourcePath = [IO.Path]::GetFullPath($ReleaseZip)
        if (!(Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "The provided -ReleaseZip file does not exist: $sourcePath"
        }
        Copy-Item -LiteralPath $sourcePath -Destination $zipPath
    } else {
        Write-Host 'Downloading official KHARVOX v1.1.1 release from CactusVRStudios...' -ForegroundColor Cyan
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $sourceUrl -UseBasicParsing -OutFile $zipPath
    }

    $actualSha256 = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -ne $expectedSha256) {
        throw "Official release SHA256 mismatch: $actualSha256. No shader files were installed."
    }
    Write-Host 'Official release ZIP SHA256 verified.' -ForegroundColor Green

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
    $entries = @($archive.Entries | Where-Object {
        $_.FullName -match '^sfs-profile/[^/\\]+\.spv$'
    })
    if ($entries.Count -ne 75) {
        throw "Expected 75 SFS shaders in official release, found $($entries.Count). No changes made."
    }
    $unique = @($entries | ForEach-Object { [IO.Path]::GetFileName($_.FullName) } |
        Sort-Object -Unique)
    if ($unique.Count -ne 75) { throw 'Duplicate SFS filenames found in ZIP. No changes made.' }

    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    foreach ($entry in $entries) {
        $name = [IO.Path]::GetFileName($entry.FullName)
        if ($entry.Length -lt 20 -or $entry.Length -gt 16777216 -or $entry.Length % 4 -ne 0) {
            throw "Invalid SPIR-V shader length: $name"
        }
        $source = $entry.Open()
        $target = [IO.File]::Create((Join-Path $stage $name))
        try { $source.CopyTo($target) }
        finally { $target.Dispose(); $source.Dispose() }
    }
    $archive.Dispose()
    $archive = $null

    $shaderFiles = @(Get-ChildItem -LiteralPath $stage -File -Filter '*.spv')
    if ($shaderFiles.Count -ne 75) { throw 'Staged shader count failed verification.' }
    foreach ($file in $shaderFiles) {
        $stream = [IO.File]::OpenRead($file.FullName)
        try {
            if ($stream.Length -lt 20 -or $stream.Length % 4 -ne 0) {
                throw "Bad shader size: $($file.Name)"
            }
            $magic = New-Object byte[] 4
            if ($stream.Read($magic, 0, 4) -ne 4 -or
                [BitConverter]::ToUInt32($magic, 0) -ne 0x07230203) {
                throw "Bad SPIR-V header: $($file.Name)"
            }
        } finally { $stream.Dispose() }
    }
    Write-Host 'All 75 SFS files passed basic SPIR-V integrity checks.' -ForegroundColor Green

    # No modification until the full archive has been verified and staged.
    if (Test-Path -LiteralPath $destination -PathType Container) {
        $backup = Join-Path $root ('sfs-profile-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 6))
        [IO.Directory]::Move($destination, $backup)
        $backupMade = $true
    } elseif (Test-Path -LiteralPath $destination) {
        throw "The destination exists but is not a folder: $destination"
    }
    try {
        [IO.Directory]::Move($stage, $destination)
        $installed = $true
    } catch {
        if ($backupMade -and !(Test-Path -LiteralPath $destination)) {
            [IO.Directory]::Move($backup, $destination)
            $backupMade = $false
        }
        throw
    }
    Write-Host "Restored 75/75 shaders into $destination" -ForegroundColor Green
    if ($backupMade) { Write-Host "Previous profile safely backed up to: $backup" -ForegroundColor Yellow }
    Write-Host 'No launcher, DLL, game, settings or mod files were overwritten.' -ForegroundColor Cyan
    Write-Host 'SFS is ready for a headset compatibility test; matching file count is not proof of visual correctness.'
} finally {
    if ($archive) { $archive.Dispose() }
    if (!$installed -and (Test-Path -LiteralPath $stage)) {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}
