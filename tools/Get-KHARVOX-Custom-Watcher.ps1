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
    $launchers = @(Get-Process -Name "KharvoxLauncher" -ErrorAction SilentlyContinue)
    foreach ($process in $launchers) {
        try {
            Write-Status "Closing KHARVOX Launcher before update..." DarkYellow
            if (-not $process.CloseMainWindow()) {
                $process.Kill()
            } elseif (-not $process.WaitForExit(5000)) {
                $process.Kill()
            }
        } catch {
            try { $process.Kill() } catch {}
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

function Verify-Payload($payload, $run, [string]$ExpectedKind) {
    $manifest = $payload.Manifest
    if ([string]$manifest.kind -ne $ExpectedKind) {
        throw "Artifact kind mismatch. Expected $ExpectedKind, got $($manifest.kind)"
    }
    if ([string]$manifest.commit -ne [string]$run.head_sha) {
        throw "Artifact commit $($manifest.commit) does not match latest successful run $($run.head_sha)"
    }

    foreach ($file in @($manifest.files)) {
        $relative = ([string]$file.path).Replace("/", "\")
        $path = Join-Path $payload.Root $relative
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Artifact file missing: $relative"
        }
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
        if ($hash -ne ([string]$file.sha256).ToLowerInvariant()) {
            throw "Hash mismatch: $relative"
        }
    }
}

function Copy-PayloadFiles($payload, [string]$Destination) {
    foreach ($file in @($payload.Manifest.files)) {
        $relative = ([string]$file.path).Replace("/", "\")
        $source = Join-Path $payload.Root $relative
        $target = Join-Path $Destination $relative
        $parent = Split-Path $target -Parent
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        Copy-Item -LiteralPath $source -Destination $target -Force
        try { Unblock-File -LiteralPath $target -ErrorAction SilentlyContinue } catch {}
    }

    foreach ($metadata in @("PATCH-MANIFEST.json","PATCH-BUILD.txt")) {
        $source = Join-Path $payload.Root $metadata
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $Destination $metadata) -Force
        }
    }
}

function Backup-PatchTargets($payload) {
    if (!(Test-Path -LiteralPath $InstallRoot)) { return }
    $backup = Join-Path $InstallRoot ".rollback\previous"
    Remove-Item -Recurse -Force $backup -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $backup | Out-Null

    foreach ($file in @($payload.Manifest.files)) {
        $relative = ([string]$file.path).Replace("/", "\")
        $source = Join-Path $InstallRoot $relative
        if (!(Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        $target = Join-Path $backup $relative
        $parent = Split-Path $target -Parent
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        Copy-Item -LiteralPath $source -Destination $target -Force
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
    Wait-ForDoomExit
    Stop-Launcher
    $payload = $null
    try {
        $payload = Download-And-Expand $FullArtifact
        Verify-Payload $payload $run "full"

        if (Test-Path -LiteralPath $InstallRoot) {
            Remove-Item -Recurse -Force $InstallRoot
        }
        New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
        Copy-PayloadFiles $payload $InstallRoot
        Save-State $run "full"
        Write-Status "Full KHARVOX build installed: run #$($run.run_number), $(([string]$run.head_sha).Substring(0,7))" Green
    } finally {
        if ($payload -and $payload.Work) {
            Remove-Item -Recurse -Force $payload.Work -ErrorAction SilentlyContinue
        }
    }
    Start-Launcher
}

function Install-Patch($run) {
    Wait-ForDoomExit
    Stop-Launcher
    $payload = $null
    try {
        $payload = Download-And-Expand $PatchArtifact
        Verify-Payload $payload $run "patch"
        Backup-PatchTargets $payload
        Copy-PayloadFiles $payload $InstallRoot
        Save-State $run "patch"
        Write-Status "Fast patch applied: run #$($run.run_number), $(([string]$run.head_sha).Substring(0,7))" Green
    } finally {
        if ($payload -and $payload.Work) {
            Remove-Item -Recurse -Force $payload.Work -ErrorAction SilentlyContinue
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
