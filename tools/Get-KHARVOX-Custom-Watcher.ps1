param(
    [string]$Repository = "MrEGS-Ops/KHARVOX-Custom",
    [string]$Branch = "clean/custom-core",
    [string]$InstallRoot = "",
    [int]$PollSeconds = 300,
    [switch]$NoLaunch,
    [switch]$SelfTest
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

# Render the actual remaining interval instead of an imprecise
# "next check in 5 minutes" notice. An explicitly supplied -PollSeconds
# controls both the real polling interval and the displayed countdown.
function Format-NextCheckCountdown([int]$RemainingSeconds) {
    $remaining = [Math]::Max(0, $RemainingSeconds)
    $minutes = [int][Math]::Floor($remaining / 60)
    $seconds = $remaining % 60
    return ("Next check in {0:00}:{1:00} left" -f $minutes, $seconds)
}

function Wait-UntilNextCheck([int]$Seconds) {
    $wait = [Math]::Max(10, $Seconds)
    $deadline = [DateTime]::UtcNow.AddSeconds($wait)
    $interactiveConsole = $false
    try { $interactiveConsole = -not [Console]::IsOutputRedirected } catch {}
    if (-not $interactiveConsole) {
        # Redirected logs should not be flooded with 300 nearly identical
        # lines or terminal carriage returns.
        Write-Status (Format-NextCheckCountdown $wait) DarkGray
        Start-Sleep -Seconds $wait
        return
    }
    $previousLength = 0
    try {
        while ($true) {
            $remaining = [Math]::Max(0,
                [int][Math]::Ceiling(($deadline - [DateTime]::UtcNow).TotalSeconds))
            $message = Format-NextCheckCountdown $remaining
            Write-Host -NoNewline (("`r" + $message).PadRight($previousLength + 1))
            $previousLength = $message.Length + 1
            if ($remaining -eq 0) { break }
            # Based on an absolute deadline, so time spent writing or a slow
            # console cannot cause the next GitHub check to drift.
            Start-Sleep -Milliseconds 250
        }
    } finally {
        # Clear the single status line before the next timestamped message.
        Write-Host -NoNewline ("`r" + (" " * $previousLength) + "`r")
    }
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
    # Do not mistake another KHARVOX installation for this one.
    $expected = [IO.Path]::GetFullPath($launcher)
    $instances = @(Get-CimInstance Win32_Process -Filter "Name = 'KharvoxLauncher.exe'" -ErrorAction Stop)
    foreach ($instance in $instances) {
        if (![string]::IsNullOrWhiteSpace([string]$instance.ExecutablePath) -and
            [string]::Equals([IO.Path]::GetFullPath([string]$instance.ExecutablePath),
                $expected, [StringComparison]::OrdinalIgnoreCase)) { return }
    }
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

function Invoke-WatcherSelfTest {
    if ((Format-NextCheckCountdown 300) -ne "Next check in 05:00 left" -or
        (Format-NextCheckCountdown 59) -ne "Next check in 00:59 left" -or
        (Format-NextCheckCountdown 0) -ne "Next check in 00:00 left" -or
        (Format-NextCheckCountdown -5) -ne "Next check in 00:00 left" -or
        (Format-NextCheckCountdown 3723) -ne "Next check in 62:03 left") {
        throw "Next-check countdown formatting regressed."
    }
    $temp = Join-Path ([IO.Path]::GetTempPath()) ("KHARVOX-Watcher-SelfTest-" + [Guid]::NewGuid().ToString("N"))
    $script:InstallRoot = Join-Path $temp "installation"
    $payloadRoot = Join-Path $temp "payload"
    try {
        New-Item -ItemType Directory -Force -Path $InstallRoot,$payloadRoot | Out-Null
        $userMod = Join-Path $InstallRoot "mods/doom/user/personal.zip"
        New-Item -ItemType Directory -Force -Path (Split-Path $userMod -Parent) | Out-Null
        [IO.File]::WriteAllText($userMod, "MY PERSONAL MOD")
        [IO.File]::WriteAllText((Join-Path $InstallRoot "KharvoxLauncher.exe"), "old-launcher")
        $manifestFiles = @()
        foreach ($name in @("KharvoxLauncher.exe", "KharvoxLayer.dll", "KharvoxSupervisor.exe")) {
            $source = Join-Path $payloadRoot $name
            [IO.File]::WriteAllText($source, "new-$name")
            $manifestFiles += [pscustomobject]@{
                path=$name
                size=(Get-Item -LiteralPath $source).Length
                sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()
            }
        }
        $run = [pscustomobject]@{ run_number=77; head_sha="1234567890123456789012345678901234567890" }
        $payload = [pscustomobject]@{
            Root=$payloadRoot
            Manifest=[pscustomobject]@{
                schema=1; kind="full"; run="77"; commit=$run.head_sha; files=$manifestFiles
            }
        }
        Verify-Payload $payload $run "full"
        Install-VerifiedPayload $payload $run "full"
        if ([IO.File]::ReadAllText($userMod) -ne "MY PERSONAL MOD" -or
            [IO.File]::ReadAllText((Join-Path $InstallRoot "KharvoxLauncher.exe")) -ne "new-KharvoxLauncher.exe") {
            throw "Full install failed to preserve user data or update launcher."
        }

        # Simulate a missing source during the second copy stage. The first
        # replacement must be reverted, not left as a mixed installation.
        [IO.File]::WriteAllText((Join-Path $payloadRoot "KharvoxLauncher.exe"), "broken-update")
        Remove-Item -LiteralPath (Join-Path $payloadRoot "KharvoxLayer.dll") -Force
        $failed = $false
        try { Install-VerifiedPayload $payload $run "patch" } catch { $failed = $true }
        if (!$failed -or
            [IO.File]::ReadAllText((Join-Path $InstallRoot "KharvoxLauncher.exe")) -ne "new-KharvoxLauncher.exe" -or
            [IO.File]::ReadAllText($userMod) -ne "MY PERSONAL MOD") {
            throw "Failed update didn't correctly roll back or lost user data."
        }

        $blocked = $false
        try { $null = Resolve-ValidatedChild $InstallRoot "../outside.txt" }
        catch { $blocked = $true }
        if (!$blocked) { throw "Unsafe manifest path escaped the installation root." }

        Write-Host "KHARVOX watcher preservation, rollback, and path self-tests passed."
    } finally {
        Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($SelfTest) {
    Invoke-WatcherSelfTest
    exit 0
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

    Wait-UntilNextCheck $PollSeconds
}
