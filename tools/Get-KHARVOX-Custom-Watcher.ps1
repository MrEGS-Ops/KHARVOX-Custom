param(
    [string]$Repository = "MrEGS-Ops/KHARVOX-Custom",
    [string]$Branch = "clean/custom-core",
    [string]$InstallRoot = "",
    [int]$PollSeconds = 30,
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"
$WorkflowFile = "build-custom.yml"
$FullArtifact = "KHARVOX-Full-Build"
$PatchArtifact = "KHARVOX-Dev-Patch"
$StateName = ".kharvox-watcher-state.json"

if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    $documents = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    $InstallRoot = Join-Path $documents "KHARVOX-Custom-Builds\KHARVOX-Custom"
}

function Write-Status([string]$Message, [ConsoleColor]$Color = [ConsoleColor]::Gray) {
    $stamp = Get-Date -Format "HH:mm:ss"
    Write-Host "[$stamp] $Message" -ForegroundColor $Color
}

function Get-LatestSuccessfulRun {
    $encodedBranch = [Uri]::EscapeDataString($Branch)
    $uri = "https://api.github.com/repos/$Repository/actions/workflows/$WorkflowFile/runs?branch=$encodedBranch&status=success&per_page=1"
    try {
        $headers = @{ "User-Agent" = "KHARVOX-Custom-Watcher" }
        $response = Invoke-RestMethod -UseBasicParsing -Headers $headers -Uri $uri -TimeoutSec 20
        return @($response.workflow_runs)[0]
    } catch {
        Write-Status "GitHub status check failed: $($_.Exception.Message)" DarkYellow
        return $null
    }
}

function Wait-ForDoomExit {
    $announced = $false
    while (Get-Process -Name "DOOMx64vk" -ErrorAction SilentlyContinue) {
        if (-not $announced) {
            Write-Status "DOOM is running. Update is ready; waiting for the game to close..." Yellow
            $announced = $true
        }
        Start-Sleep -Seconds 3
    }
}

function Stop-Launcher {
    # Only touch the launcher belonging to this installation. Experimental
    # KHARVOX builds in other directories must remain running.
    $expected = [IO.Path]::GetFullPath((Join-Path $InstallRoot "KharvoxLauncher.exe"))
    $instances = @(Get-CimInstance Win32_Process -Filter "Name = 'KharvoxLauncher.exe'" -ErrorAction Stop)
    foreach ($instance in $instances) {
        if ([string]::IsNullOrWhiteSpace([string]$instance.ExecutablePath) -or
            ![string]::Equals([IO.Path]::GetFullPath([string]$instance.ExecutablePath),
                $expected, [StringComparison]::OrdinalIgnoreCase)) { continue }
        $process = Get-Process -Id $instance.ProcessId -ErrorAction SilentlyContinue
        if (!$process) { continue }
        try {
            Write-Status "Closing this installation's KHARVOX Launcher before update..." DarkYellow
            if (-not $process.CloseMainWindow()) {
                $process.Kill()
            } elseif (-not $process.WaitForExit(5000)) {
                $process.Kill()
            }
        } catch {
            try { $process.Kill() } catch {}
        } finally {
            $process.Dispose()
        }
    }
}

function Get-State {
    $path = Join-Path $InstallRoot $StateName
    if (!(Test-Path -LiteralPath $path)) { return $null }
    try { return Get-Content -Raw -LiteralPath $path | ConvertFrom-Json }
    catch { return $null }
}

function Save-State($run, [string]$Mode) {
    New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
    $state = [ordered]@{
        schema = 1
        repository = $Repository
        branch = $Branch
        run = [int64]$run.run_number
        commit = [string]$run.head_sha
        installedUtc = (Get-Date).ToUniversalTime().ToString("o")
        mode = $Mode
    }
    $state | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath (Join-Path $InstallRoot $StateName)
}

function Get-ArtifactUrl([string]$ArtifactName) {
    $encodedBranch = [Uri]::EscapeDataString($Branch)
    return "https://nightly.link/$Repository/workflows/$WorkflowFile/$encodedBranch/$ArtifactName.zip"
}

function Download-And-Expand([string]$ArtifactName) {
    $work = Join-Path ([IO.Path]::GetTempPath()) ("KHARVOX-Watcher-" + [Guid]::NewGuid().ToString("N"))
    $zip = Join-Path $work "artifact.zip"
    $expanded = Join-Path $work "expanded"
    New-Item -ItemType Directory -Force -Path $expanded | Out-Null
    $url = Get-ArtifactUrl $ArtifactName

    Write-Status "Downloading $ArtifactName..." Cyan
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip -TimeoutSec 120
    Expand-Archive -LiteralPath $zip -DestinationPath $expanded -Force

    $manifestFile = Get-ChildItem -LiteralPath $expanded -Filter "PATCH-MANIFEST.json" -File -Recurse | Select-Object -First 1
    if (!$manifestFile) { throw "$ArtifactName did not contain PATCH-MANIFEST.json" }

    return [pscustomobject]@{
        Work = $work
        Root = $manifestFile.Directory.FullName
        ManifestPath = $manifestFile.FullName
        Manifest = (Get-Content -Raw -LiteralPath $manifestFile.FullName | ConvertFrom-Json)
    }
}

function Resolve-ValidatedChild([string]$Root, [string]$Relative) {
    $normalized = $Relative.Replace("/", "\")
    if ([string]::IsNullOrWhiteSpace($normalized) -or
        [IO.Path]::IsPathRooted($normalized) -or
        $normalized.Contains(":") -or
        @($normalized.Split("\") | Where-Object { $_ -eq "" -or $_ -eq "." -or $_ -eq ".." }).Count -gt 0) {
        throw "Unsafe artifact path: $Relative"
    }
    $base = [IO.Path]::GetFullPath($Root).TrimEnd('\','/')
    $resolved = [IO.Path]::GetFullPath((Join-Path $base $normalized))
    if (!$resolved.StartsWith($base + "\", [StringComparison]::OrdinalIgnoreCase)) {
        throw "Artifact path escapes its root: $Relative"
    }
    # Avoid writing through existing junctions/symlinks, whether in the
    # extracted archive or in the destination install tree.
    $walk = $base
    foreach ($segment in $normalized.Split("\")) {
        $walk = Join-Path $walk $segment
        if (Test-Path -LiteralPath $walk) {
            $info = Get-Item -LiteralPath $walk -Force
            if ($info.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Linked artifact path is not allowed: $Relative"
            }
        }
    }
    return $resolved
}

function Verify-Payload($payload, $run, [string]$ExpectedKind) {
    $manifest = $payload.Manifest
    if ([int]$manifest.schema -ne 1 -or [string]$manifest.kind -ne $ExpectedKind) {
        throw "Unexpected artifact format or kind (expected $ExpectedKind)."
    }
    if ([string]$manifest.commit -ne [string]$run.head_sha -or
        [string]$manifest.run -ne [string]$run.run_number) {
        throw "Artifact identity does not match the latest successful GitHub run."
    }
    $files = @($manifest.files)
    if ($files.Count -lt 3 -or $files.Count -gt 4096) {
        throw "Unexpected artifact file count: $($files.Count)"
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($file in $files) {
        $relative = [string]$file.path
        if (!$seen.Add($relative.Replace("/", "\"))) { throw "Duplicate artifact path: $relative" }
        $path = Resolve-ValidatedChild $payload.Root $relative
        # Check destination too, BEFORE altering any existing installation.
        $null = Resolve-ValidatedChild $InstallRoot $relative
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Artifact file missing: $relative"
        }
        if ([int64]$file.size -ne (Get-Item -LiteralPath $path).Length) {
            throw "Artifact file size mismatch: $relative"
        }
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
        if ($hash -ne ([string]$file.sha256).ToLowerInvariant()) {
            throw "Hash mismatch: $relative"
        }
    }
}

function Install-VerifiedPayload($payload, $run, [string]$Mode) {
    # A full install is also an overlay: NEVER delete the installation root.
    # Unknown folders (mods, tools, logs, user settings) must be preserved.
    New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
    $rollbackRoot = Join-Path $InstallRoot ".rollback"
    New-Item -ItemType Directory -Force -Path $rollbackRoot | Out-Null
    $pending = Join-Path $rollbackRoot (".pending-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $pending | Out-Null
    $existing = @{}
    $targets = @()
    $statePath = Join-Path $InstallRoot $StateName
    $oldState = Join-Path $pending "watcher-state-backup.json"
    $hadState = Test-Path -LiteralPath $statePath -PathType Leaf
    if ($hadState) { Copy-Item -LiteralPath $statePath -Destination $oldState }
    $complete = $false
    try {
        # Backup every target before writing anything. Include the previously
        # absent files so failure can remove partially added binaries.
        foreach ($file in @($payload.Manifest.files)) {
            $relative = [string]$file.path
            $target = Resolve-ValidatedChild $InstallRoot $relative
            $source = Resolve-ValidatedChild $payload.Root $relative
            $targets += [pscustomobject]@{ Relative=$relative; Target=$target; Source=$source }
            $present = Test-Path -LiteralPath $target -PathType Leaf
            $existing[$relative] = $present
            if ($present) {
                $copy = Resolve-ValidatedChild $pending ("files/" + $relative)
                New-Item -ItemType Directory -Force -Path (Split-Path $copy -Parent) | Out-Null
                Copy-Item -LiteralPath $target -Destination $copy -Force
            }
        }
        foreach ($item in $targets) {
            New-Item -ItemType Directory -Force -Path (Split-Path $item.Target -Parent) | Out-Null
            Copy-Item -LiteralPath $item.Source -Destination $item.Target -Force
            try { Unblock-File -LiteralPath $item.Target -ErrorAction SilentlyContinue } catch {}
        }
        foreach ($metadata in @("PATCH-MANIFEST.json","PATCH-BUILD.txt")) {
            $source = Resolve-ValidatedChild $payload.Root $metadata
            $target = Resolve-ValidatedChild $InstallRoot $metadata
            if (Test-Path -LiteralPath $source) {
                $backupMetadata = Resolve-ValidatedChild $pending ("metadata/" + $metadata)
                if (Test-Path -LiteralPath $target) {
                    New-Item -ItemType Directory -Force -Path (Split-Path $backupMetadata -Parent) | Out-Null
                    Copy-Item -LiteralPath $target -Destination $backupMetadata -Force
                }
                Copy-Item -LiteralPath $source -Destination $target -Force
            }
        }
        Save-State $run $Mode
        $complete = $true
        Write-Status "Installed $Mode build #$($run.run_number) ($(([string]$run.head_sha).Substring(0,7))); user files preserved." Green
    } catch {
        Write-Status "Update failed. Restoring the previous binaries and settings..." Red
        $rollbackErrors = @()
        foreach ($item in $targets) {
            try {
                if ($existing.ContainsKey($item.Relative) -and $existing[$item.Relative]) {
                    $copy = Resolve-ValidatedChild $pending ("files/" + $item.Relative)
                    if (Test-Path -LiteralPath $copy) {
                        Copy-Item -LiteralPath $copy -Destination $item.Target -Force
                    } else { throw "Missing backup for $($item.Relative)" }
                } elseif (Test-Path -LiteralPath $item.Target -PathType Leaf) {
                    Remove-Item -LiteralPath $item.Target -Force
                }
            } catch { $rollbackErrors += $_.Exception.Message }
        }
        foreach ($metadata in @("PATCH-MANIFEST.json","PATCH-BUILD.txt")) {
            try {
                $target = Resolve-ValidatedChild $InstallRoot $metadata
                $copy = Resolve-ValidatedChild $pending ("metadata/" + $metadata)
                if (Test-Path -LiteralPath $copy) {
                    Copy-Item -LiteralPath $copy -Destination $target -Force
                } elseif (Test-Path -LiteralPath $target) {
                    Remove-Item -LiteralPath $target -Force
                }
            } catch { $rollbackErrors += $_.Exception.Message }
        }
        try {
            if ($hadState) {
                Copy-Item -LiteralPath $oldState -Destination $statePath -Force
            } elseif (Test-Path -LiteralPath $statePath) {
                Remove-Item -LiteralPath $statePath -Force
            }
        } catch { $rollbackErrors += $_.Exception.Message }
        if ($rollbackErrors.Count -gt 0) {
            throw "Update failed and rollback was incomplete. Recovery files are in $pending. Errors: $($rollbackErrors -join '; ')"
        }
        throw
    } finally {
        if ($complete -and (Test-Path -LiteralPath $pending)) {
            # This backup is the previous working build. Keep it for manual recovery.
            $previous = Join-Path $rollbackRoot "previous"
            try {
                if (Test-Path -LiteralPath $previous) {
                    Remove-Item -Recurse -Force -LiteralPath $previous
                }
                Move-Item -LiteralPath $pending -Destination $previous
            } catch { Write-Status "Could not rotate rollback snapshot: $($_.Exception.Message)" DarkYellow }
        } elseif (!$complete -and $rollbackErrors.Count -eq 0 -and
                  (Test-Path -LiteralPath $pending)) {
            Remove-Item -Recurse -Force -LiteralPath $pending -ErrorAction SilentlyContinue
        }
    }
}

function Start-Launcher {
    if ($NoLaunch) { return }
    $launcher = Join-Path $InstallRoot "KharvoxLauncher.exe"
    if (!(Test-Path -LiteralPath $launcher)) { return }
    if (Get-Process -Name "KharvoxLauncher" -ErrorAction SilentlyContinue) { return }
    try {
        Start-Process -FilePath $launcher -WorkingDirectory $InstallRoot
        Write-Status "KHARVOX Launcher started." Green
    } catch {
        Write-Status "Update installed, but launcher could not be started: $($_.Exception.Message)" DarkYellow
    }
}

function Install-Full($run) {
    $payload = $null
    try {
        $payload = Download-And-Expand $FullArtifact
        Verify-Payload $payload $run "full"
        Wait-ForDoomExit
        Stop-Launcher
        Install-VerifiedPayload $payload $run "full"
    } finally {
        if ($payload -and $payload.Work) {
            Remove-Item -Recurse -Force -LiteralPath $payload.Work -ErrorAction SilentlyContinue
        }
    }
    Start-Launcher
}

function Install-Patch($run) {
    $payload = $null
    try {
        $payload = Download-And-Expand $PatchArtifact
        Verify-Payload $payload $run "patch"
        Wait-ForDoomExit
        Stop-Launcher
        Install-VerifiedPayload $payload $run "patch"
    } finally {
        if ($payload -and $payload.Work) {
            Remove-Item -Recurse -Force -LiteralPath $payload.Work -ErrorAction SilentlyContinue
        }
    }
    Start-Launcher
}

Write-Status "KHARVOX Custom Watcher" Cyan
Write-Status "Branch: $Branch"
Write-Status "Install: $InstallRoot"
Write-Status "First install uses the full artifact; later updates use the small patch." DarkGray

while ($true) {
    try {
        $run = Get-LatestSuccessfulRun
        if ($run) {
            $launcher = Join-Path $InstallRoot "KharvoxLauncher.exe"
            $layer = Join-Path $InstallRoot "KharvoxLayer.dll"
            $installed = (Test-Path -LiteralPath $launcher) -and (Test-Path -LiteralPath $layer)
            $state = Get-State

            if (-not $installed) {
                Write-Status "No complete local build found. Installing latest successful full build..." Yellow
                Install-Full $run
            } elseif (!$state -or [string]$state.commit -ne [string]$run.head_sha) {
                Write-Status "New successful build detected. Applying fast patch..." Cyan
                Install-Patch $run
            }
        }
    } catch {
        Write-Status "Watcher update failed: $($_.Exception.Message)" Red
    }

    Start-Sleep -Seconds ([Math]::Max(10, $PollSeconds))
}
