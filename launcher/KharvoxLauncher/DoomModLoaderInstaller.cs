using System.IO.Compression;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;

namespace KharvoxLauncher;

// Downloaded on demand; third-party binaries are never packaged in KHARVOX builds.
internal static class DoomModLoaderInstaller
{
    internal const string Version = "0.6";
    internal const string SourceUrl =
        "https://github.com/ZwipZwapZapony/DOOMModLoader/releases/download/v0.6/DOOMModLoader-Windows-x64.zip";
    internal const string SourcePage =
        "https://github.com/ZwipZwapZapony/DOOMModLoader/releases/tag/v0.6";
    // Published digest of the official Windows-x64 asset (GitHub release metadata).
    private const string ExpectedSha256 =
        "34d113deaf4afa0d04ff996bddf3cd512d4c30924df931ddaa414fe1d104e6a1";
    private const long MaxDownloadBytes = 32L * 1024 * 1024;
    private const long MaxUnpackedBytes = 128L * 1024 * 1024;

    internal static string InstallDirectory =>
        Path.Combine(AppContext.BaseDirectory, "tools", "doommodloader");

    internal static string ExecutablePath =>
        Path.Combine(InstallDirectory, "DOOMModLoader.exe");

    internal static bool IsInstalled =>
        File.Exists(ExecutablePath)
        && File.Exists(Path.Combine(InstallDirectory, "KHARVOX-SOURCE.txt"))
        && File.ReadAllText(Path.Combine(InstallDirectory, "KHARVOX-SOURCE.txt"))
            .Contains(ExpectedSha256);

    internal sealed class InstallProgress
    {
        public string Message { get; }
        public int Percent { get; }

        public InstallProgress(string message, int percent)
        {
            Message = message;
            Percent = Math.Max(0, Math.Min(100, percent));
        }
    }

    internal static async Task InstallAsync(
        IProgress<InstallProgress> progress, CancellationToken cancellationToken)
    {
        if (IsInstalled)
        {
            progress.Report(new InstallProgress("DOOMModLoader " + Version + " is already installed.", 100));
            return;
        }

        var toolsDirectory = Path.Combine(AppContext.BaseDirectory, "tools");
        Directory.CreateDirectory(toolsDirectory);

        var staging = Path.Combine(toolsDirectory,
            ".doommodloader-install-" + Guid.NewGuid().ToString("N"));
        var backup = Path.Combine(toolsDirectory,
            ".doommodloader-backup-" + Guid.NewGuid().ToString("N"));
        var movedOriginal = false;
        var installedNew = false;

        Directory.CreateDirectory(staging);
        try
        {
            var downloadedZip = Path.Combine(staging, "download.tmp");
            progress.Report(new InstallProgress("Connecting to the official GitHub release...", 1));

            using (var client = new HttpClient(new HttpClientHandler
                   { AutomaticDecompression = DecompressionMethods.None }))
            {
                client.Timeout = TimeSpan.FromMinutes(3);
                using (var response = await client.GetAsync(
                    SourceUrl, HttpCompletionOption.ResponseHeadersRead, cancellationToken))
                {
                    response.EnsureSuccessStatusCode();
                    var expectedLength = response.Content.Headers.ContentLength;
                    if (expectedLength.HasValue && expectedLength.Value > MaxDownloadBytes)
                        throw new InvalidDataException("The download is larger than expected.");

                    using (var source = await response.Content.ReadAsStreamAsync())
                    using (var output = new FileStream(downloadedZip, FileMode.CreateNew,
                               FileAccess.Write, FileShare.None, 65536, true))
                    {
                        var buffer = new byte[65536];
                        long received = 0;
                        int count;
                        while ((count = await source.ReadAsync(
                            buffer, 0, buffer.Length, cancellationToken)) > 0)
                        {
                            cancellationToken.ThrowIfCancellationRequested();
                            received += count;
                            if (received > MaxDownloadBytes)
                                throw new InvalidDataException("The download exceeded the safety limit.");
                            await output.WriteAsync(buffer, 0, count, cancellationToken);

                            var percent = expectedLength.GetValueOrDefault() > 0
                                ? (int)Math.Min(90, received * 90 / expectedLength.Value)
                                : 0;
                            var message = expectedLength.GetValueOrDefault() > 0
                                ? "Downloading " + (received / 1024) + " / "
                                    + (expectedLength.Value / 1024) + " KB..."
                                : "Downloading " + (received / 1024) + " KB...";
                            progress.Report(new InstallProgress(message, percent));
                        }
                    }
                }
            }

            cancellationToken.ThrowIfCancellationRequested();
            progress.Report(new InstallProgress("Verifying official SHA-256 signature...", 92));
            using (var sha = SHA256.Create())
            using (var downloaded = File.OpenRead(downloadedZip))
            {
                var actual = BitConverter.ToString(sha.ComputeHash(downloaded))
                    .Replace("-", "").ToLowerInvariant();
                if (!string.Equals(actual, ExpectedSha256, StringComparison.Ordinal))
                    throw new InvalidDataException(
                        "Download SHA-256 mismatch. No files were installed.");
            }

            progress.Report(new InstallProgress("Unpacking into KHARVOX tools folder...", 94));
            var unpacked = Path.Combine(staging, "unpacked");
            Directory.CreateDirectory(unpacked);
            var unpackedRoot = Path.GetFullPath(unpacked)
                .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;

            using (var zip = ZipFile.OpenRead(downloadedZip))
            {
                if (zip.Entries.Count > 256)
                    throw new InvalidDataException("Unexpectedly large archive file list.");
                long unpackedBytes = 0;
                foreach (var entry in zip.Entries)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    var name = entry.FullName.Replace('/', Path.DirectorySeparatorChar)
                        .Replace('\\', Path.DirectorySeparatorChar);
                    var destination = Path.GetFullPath(Path.Combine(unpacked, name));
                    if (!destination.StartsWith(unpackedRoot, StringComparison.OrdinalIgnoreCase)
                        || name.IndexOf(':') >= 0)
                        throw new InvalidDataException("Unsafe path in DOOMModLoader archive.");

                    unpackedBytes += entry.Length;
                    if (unpackedBytes > MaxUnpackedBytes)
                        throw new InvalidDataException("Archive contents exceed the safety limit.");

                    if (name.EndsWith(Path.DirectorySeparatorChar.ToString(),
                            StringComparison.Ordinal))
                    {
                        Directory.CreateDirectory(destination);
                        continue;
                    }

                    Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                    using (var input = entry.Open())
                    using (var output = new FileStream(destination, FileMode.CreateNew,
                               FileAccess.Write, FileShare.None))
                        input.CopyTo(output);
                }
            }

            // Support a release whose ZIP wraps files in one enclosing directory.
            var executable = Directory.GetFiles(
                unpacked, "DOOMModLoader.exe", SearchOption.AllDirectories);
            if (executable.Length != 1)
                throw new InvalidDataException(
                    "The official release did not contain exactly one DOOMModLoader.exe.");

            var installedFiles = Path.GetDirectoryName(executable[0])!;
            var finalized = Path.Combine(staging, "finalized");
            Directory.Move(installedFiles, finalized);
            File.WriteAllText(Path.Combine(finalized, "KHARVOX-SOURCE.txt"),
                "DOOMModLoader v" + Version + Environment.NewLine
                + SourcePage + Environment.NewLine
                + "Downloaded archive SHA-256: " + ExpectedSha256 + Environment.NewLine);
            cancellationToken.ThrowIfCancellationRequested();
            progress.Report(new InstallProgress("Finishing installation...", 98));

            if (Directory.Exists(InstallDirectory))
            {
                Directory.Move(InstallDirectory, backup);
                movedOriginal = true;
            }

            try
            {
                Directory.Move(finalized, InstallDirectory);
                installedNew = true;
            }
            catch
            {
                if (movedOriginal && !Directory.Exists(InstallDirectory))
                {
                    Directory.Move(backup, InstallDirectory);
                    movedOriginal = false;
                }
                throw;
            }

            progress.Report(new InstallProgress(
                "DOOMModLoader installed in KHARVOX/tools/doommodloader.", 100));
        }
        finally
        {
            try { if (Directory.Exists(staging)) Directory.Delete(staging, true); }
            catch { /* Stale staging can be cleaned at a later launch. */ }
            if (installedNew && movedOriginal)
            {
                try { Directory.Delete(backup, true); }
                catch { /* Previous version retained as a safe backup. */ }
            }
        }
    }
}

// User-approved first-use download with cancellable progress.
internal sealed class DoomModLoaderInstallDialog : Form
{
    private readonly Label statusLabel;
    private readonly ProgressBar progressBar;
    private readonly Button actionButton;
    private CancellationTokenSource? cancellation;
    private bool running;
    internal bool Installed { get; private set; }

    internal DoomModLoaderInstallDialog()
    {
        Text = "KHARVOX — Install DOOMModLoader";
        ClientSize = new Size(520, 230);
        MinimumSize = new Size(480, 265);
        MaximumSize = new Size(800, 350);
        StartPosition = FormStartPosition.CenterParent;
        ShowInTaskbar = false;
        MaximizeBox = false;
        BackColor = Color.FromArgb(30, 30, 33);
        ForeColor = Color.White;
        Font = new Font("Segoe UI", 9F);

        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(18),
            ColumnCount = 1,
            RowCount = 5
        };
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        layout.Controls.Add(new Label
        {
            Dock = DockStyle.Fill,
            ForeColor = Color.Gainsboro,
            Text = "Download DOOMModLoader v0.6 (about 2.9 MB) from the official GitHub release."
                + Environment.NewLine
                + "Stored under KHARVOX\\tools\\doommodloader — not in the DOOM game folder."
        }, 0, 0);

        statusLabel = new Label
        {
            Dock = DockStyle.Fill, ForeColor = Color.Silver,
            Text = "No download will begin until you select Download & Install.",
            AutoEllipsis = true
        };
        layout.Controls.Add(statusLabel, 0, 1);

        progressBar = new ProgressBar { Dock = DockStyle.Fill, Minimum = 0, Maximum = 100 };
        layout.Controls.Add(progressBar, 0, 2);

        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false
        };
        actionButton = new Button
        {
            Text = "Download && Install",
            AutoSize = true,
            BackColor = Color.FromArgb(55, 55, 59),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        actionButton.Click += ActionClicked;
        buttons.Controls.Add(actionButton);
        var closeButton = new Button
        {
            Text = "Cancel", Width = 92,
            BackColor = Color.FromArgb(42, 42, 46),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        closeButton.Click += (_, _) =>
        {
            if (running) CancelDownload();
            else Close();
        };
        buttons.Controls.Add(closeButton);
        layout.Controls.Add(buttons, 0, 3);

        var credit = new LinkLabel
        {
            Dock = DockStyle.Fill,
            Text = "Official source: Zwip-Zwap Zapony / PowerBall253 — GitHub release",
            LinkColor = Color.LightSkyBlue,
            ActiveLinkColor = Color.White,
            TextAlign = ContentAlignment.MiddleLeft
        };
        credit.Click += (_, _) =>
        {
            try { System.Diagnostics.Process.Start(DoomModLoaderInstaller.SourcePage); }
            catch { }
        };
        layout.Controls.Add(credit, 0, 4);
        Controls.Add(layout);
    }

    private async void ActionClicked(object? sender, EventArgs e)
    {
        if (running) return;
        running = true;
        actionButton.Enabled = false;
        cancellation = new CancellationTokenSource();
        var progress = new Progress<DoomModLoaderInstaller.InstallProgress>(update =>
        {
            if (IsDisposed) return;
            progressBar.Value = update.Percent;
            statusLabel.Text = update.Message;
        });
        try
        {
            await DoomModLoaderInstaller.InstallAsync(progress, cancellation.Token);
            Installed = true;
            statusLabel.Text = "Installed successfully. Ready for future DOOM resource mods.";
            progressBar.Value = 100;
            actionButton.Text = "Done";
        }
        catch (OperationCanceledException)
        {
            statusLabel.Text = "Download cancelled. No new installation was left behind.";
            actionButton.Text = "Retry";
        }
        catch (Exception ex)
        {
            statusLabel.Text = "Installation failed: " + ex.Message;
            actionButton.Text = "Retry";
        }
        finally
        {
            running = false;
            actionButton.Enabled = !Installed;
            cancellation.Dispose();
            cancellation = null;
        }
    }

    private void CancelDownload()
    {
        if (cancellation is null) return;
        statusLabel.Text = "Cancelling and cleaning up...";
        cancellation.Cancel();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (running)
        {
            e.Cancel = true;
            CancelDownload();
            return;
        }
        base.OnFormClosing(e);
    }
}
