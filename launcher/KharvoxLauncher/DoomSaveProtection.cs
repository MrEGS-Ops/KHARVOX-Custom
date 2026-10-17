using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Safe, non-destructive DOOM (2016) save snapshots. No edits are performed
// while DOOM is running. Save folders are copied together so developer-mode
// and normal game profiles cannot silently overwrite one another.
internal static class DoomSaveProtection
{
    private const int Schema = 1;
    private static readonly string[] SaveNames = { "savegame", "savegame.user" };

    internal sealed class Snapshot
    {
        public int Schema { get; set; } = 1;
        public string Id { get; set; } = "";
        public string Label { get; set; } = "";
        public string Source { get; set; } = "";
        public string Mode { get; set; } = "";
        public string CreatedUtc { get; set; } = "";
        public Dictionary<string, string> Hashes { get; set; } = new();
        public override string ToString() => CreatedUtc + "   " + Label
            + "   (" + Hashes.Count + " files)";
    }
    internal sealed class SessionState
    {
        public int Schema { get; set; } = 1;
        public string Source { get; set; } = "";
        public string LastMode { get; set; } = "normal";
        public string CleanSnapshotId { get; set; } = "";
    }

    internal static string StorageRoot => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "KHARVOX", "DOOM-2016", "save-protection");
    private static string SnapshotRoot => Path.Combine(StorageRoot, "snapshots");
    private static string StatePath => Path.Combine(StorageRoot, "session.json");
    private static string OverridePath => Path.Combine(StorageRoot, "save-location.txt");

    // Windows Known Folder Saved Games, which may be redirected to OneDrive.
    // Fallback is for systems where that shell folder cannot be queried.
    [DllImport("shell32.dll")]
    private static extern int SHGetKnownFolderPath(
        [MarshalAs(UnmanagedType.LPStruct)] Guid id, uint flags,
        IntPtr accessToken, out IntPtr path);
    private static readonly Guid SavedGamesFolder = new(
        "4C5C32FF-BB9D-43B0-BF4A-A9C016AAB8C5");

    internal static string DefaultSaveRoot()
    {
        if (Environment.OSVersion.Platform == PlatformID.Win32NT)
        {
            IntPtr pointer = IntPtr.Zero;
            try
            {
                if (SHGetKnownFolderPath(SavedGamesFolder, 0, IntPtr.Zero,
                        out pointer) == 0 && pointer != IntPtr.Zero)
                    return Path.Combine(Marshal.PtrToStringUni(pointer)!,
                        "id Software", "DOOM", "base");
            }
            catch (DllNotFoundException) { }
            catch (EntryPointNotFoundException) { }
            finally { if (pointer != IntPtr.Zero) Marshal.FreeCoTaskMem(pointer); }
        }
        return Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "Saved Games", "id Software", "DOOM", "base");
    }

    internal static string SaveRoot()
    {
        if (!File.Exists(OverridePath)) return DefaultSaveRoot();
        var source = File.ReadAllText(OverridePath).Trim();
        if (string.IsNullOrWhiteSpace(source) || !Path.IsPathRooted(source))
            throw new InvalidDataException("Saved Games folder override is invalid.");
        return Path.GetFullPath(source);
    }

    internal static void SetSaveRoot(string path)
    {
        EnsureGameClosed();
        var root = Path.GetFullPath(path);
        if (!HasSaveData(root))
            throw new InvalidOperationException(
                "This folder does not contain DOOM 2016 savegame / savegame.user data.");
        // Never silently point old backups at a different user's save folder.
        var state = ReadState();
        if (state.Source.Length > 0 && !SamePath(state.Source, root))
            throw new InvalidOperationException(
                "This KHARVOX backup library belongs to a different save location. "
                + "Restore from that location or use a separate Windows user profile.");
        Directory.CreateDirectory(StorageRoot);
        AtomicWrite(OverridePath, root);
    }

    internal static bool SelectedResourceMods(string? gameDirectory)
    {
        var selected = DoomUserMods.LoadSelections();
        return DoomUserMods.Scan(gameDirectory).Any(mod => selected.Contains(mod.Id));
    }

    internal static bool IsGameRunning()
    {
        foreach (var name in new[] { "DOOMx64vk", "DOOMx64" })
        {
            using var processes = new ProcessGroup(Process.GetProcessesByName(name));
            if (processes.Items.Length != 0) return true;
        }
        return false;
    }
    private sealed class ProcessGroup : IDisposable
    {
        internal readonly System.Diagnostics.Process[] Items;
        internal ProcessGroup(System.Diagnostics.Process[] items) => Items = items;
        public void Dispose() { foreach (var item in Items) item.Dispose(); }
    }
    internal static void EnsureGameClosed()
    {
        if (IsGameRunning())
            throw new InvalidOperationException(
                "Close DOOM completely before backing up or restoring save games.");
    }

    private static bool SamePath(string a, string b) =>
        string.Equals(Path.GetFullPath(a).TrimEnd('\\', '/'),
            Path.GetFullPath(b).TrimEnd('\\', '/'), StringComparison.OrdinalIgnoreCase);
    private static JavaScriptSerializer Json() => new() { MaxJsonLength = 32 * 1024 * 1024 };

    private static T LoadJson<T>(string file)
    {
        var value = Json().Deserialize<T>(File.ReadAllText(file));
        if (value is null) throw new InvalidDataException("Invalid save backup metadata: " + file);
        return value;
    }
    private static void AtomicWrite(string path, string value)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temp, value, new UTF8Encoding(false));
            if (File.Exists(path)) File.Replace(temp, path, null);
            else File.Move(temp, path);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }
    private static SessionState ReadState()
    {
        if (!File.Exists(StatePath)) return new SessionState();
        var value = LoadJson<SessionState>(StatePath);
        if (value.Schema != Schema) throw new InvalidDataException(
            "Unrecognised save protection version; existing backups are preserved.");
        return value;
    }
    private static void SaveState(SessionState state) =>
        AtomicWrite(StatePath, Json().Serialize(state));

    private static void VerifySource(SessionState state, string source)
    {
        if (!string.IsNullOrWhiteSpace(state.Source) && !SamePath(state.Source, source))
            throw new InvalidOperationException(
                "DOOM save folder changed. KHARVOX will not apply another profile's backups.");
    }
    private static void RejectLink(string path)
    {
        if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidOperationException(
                "Refusing to follow a symbolic link or junction in game saves: " + path);
    }
    private static IEnumerable<string> SaveFiles(string source)
    {
        foreach (var name in SaveNames)
        {
            var root = Path.Combine(source, name);
            if (!Directory.Exists(root)) continue;
            RejectLink(root);
            var pending = new Stack<string>();
            pending.Push(root);
            while (pending.Count != 0)
            {
                var dir = pending.Pop();
                RejectLink(dir);
                foreach (var file in Directory.GetFiles(dir))
                {
                    RejectLink(file);
                    yield return file;
                }
                foreach (var sub in Directory.GetDirectories(dir))
                {
                    RejectLink(sub);
                    pending.Push(sub);
                }
            }
        }
    }
    internal static bool HasSaveData(string root) =>
        Directory.Exists(root) && SaveFiles(root).Any();

    private static string Hash(string file)
    {
        using var input = File.OpenRead(file);
        using var sha = SHA256.Create();
        return BitConverter.ToString(sha.ComputeHash(input)).Replace("-", "")
            .ToLowerInvariant();
    }
    private static Dictionary<string, string> FileHashes(string source)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in SaveFiles(source))
            result.Add(file.Substring(source.TrimEnd('\\', '/').Length + 1)
                .Replace('\\', '/'), Hash(file));
        return result;
    }
    private static void CopySaves(string source, string destination)
    {
        foreach (var name in SaveNames)
        {
            var original = Path.Combine(source, name);
            if (!Directory.Exists(original)) continue;
            var targetRoot = Path.Combine(destination, name);
            Directory.CreateDirectory(targetRoot);
        }
        foreach (var file in SaveFiles(source))
        {
            var relative = file.Substring(source.TrimEnd('\\', '/').Length + 1);
            var target = Path.Combine(destination, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(file, target, false);
        }
    }
    private static void CheckHashes(Snapshot info, string snapshotFolder)
    {
        if (info.Schema != Schema || info.Hashes is null || info.Hashes.Count == 0)
            throw new InvalidDataException("Save backup is empty or has an invalid manifest.");
        var actual = FileHashes(snapshotFolder);
        if (actual.Count != info.Hashes.Count || actual.Any(item =>
            !info.Hashes.TryGetValue(item.Key, out var expected)
            || !string.Equals(expected, item.Value, StringComparison.OrdinalIgnoreCase)))
            throw new InvalidDataException(
                "Backup verification failed; original saves were NOT modified.");
    }
    private static string SnapshotFolder(string id)
    {
        if (string.IsNullOrWhiteSpace(id)
            || id != Path.GetFileName(id) || id.Contains(".."))
            throw new InvalidDataException("Invalid save backup identifier.");
        return Path.Combine(SnapshotRoot, id);
    }
    private static Snapshot ReadSnapshot(string id)
    {
        var folder = SnapshotFolder(id);
        RejectLink(folder);
        var info = LoadJson<Snapshot>(Path.Combine(folder, "snapshot.json"));
        if (info.Id != id || info.Schema != Schema)
            throw new InvalidDataException("Mismatched backup manifest.");
        return info;
    }
    internal static Snapshot Capture(string label, string mode, string? sourceOverride = null)
    {
        EnsureGameClosed();
        var source = Path.GetFullPath(sourceOverride ?? SaveRoot());
        if (!HasSaveData(source))
            throw new InvalidOperationException(
                "No DOOM (2016) save files found at " + source
                + ". Start DOOM normally once, or select the correct save folder.");
        var id = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff")
            + "-" + Guid.NewGuid().ToString("N").Substring(0, 10);
        var temp = Path.Combine(SnapshotRoot, ".copy-" + Guid.NewGuid().ToString("N"));
        var final = SnapshotFolder(id);
        var info = new Snapshot { Id = id, Label = label, Mode = mode,
            Source = source, CreatedUtc = DateTime.UtcNow.ToString("u") };
        Directory.CreateDirectory(SnapshotRoot);
        try
        {
            Directory.CreateDirectory(temp);
            CopySaves(source, temp);
            info.Hashes = FileHashes(temp);
            CheckHashes(info, temp);
            File.WriteAllText(Path.Combine(temp, "snapshot.json"), Json().Serialize(info));
            Directory.Move(temp, final);
            return info;
        }
        finally { if (Directory.Exists(temp)) Directory.Delete(temp, true); }
    }

    internal static IReadOnlyList<Snapshot> ListSnapshots()
    {
        if (!Directory.Exists(SnapshotRoot)) return Array.Empty<Snapshot>();
        var snapshots = new List<Snapshot>();
        foreach (var folder in Directory.GetDirectories(SnapshotRoot))
        {
            if (Path.GetFileName(folder).StartsWith(".copy-", StringComparison.Ordinal)
                || (File.GetAttributes(folder) & FileAttributes.ReparsePoint) != 0)
                continue;
            try { snapshots.Add(ReadSnapshot(Path.GetFileName(folder))); }
            catch (Exception) { /* A damaged snapshot is never offered for restore. */ }
        }
        return snapshots.OrderByDescending(s => s.Id).ToArray();
    }

    internal static Snapshot? CleanBaseline()
    {
        var state = ReadState();
        if (string.IsNullOrWhiteSpace(state.CleanSnapshotId)) return null;
        var clean = ReadSnapshot(state.CleanSnapshotId);
        VerifySource(state, SaveRoot());
        if (!SamePath(clean.Source, SaveRoot()))
            throw new InvalidOperationException("Backup belongs to a different save location.");
        return clean;
    }
    internal static bool ReturningFromMods()
    {
        var state = ReadState();
        return state.LastMode == "modded" && CleanBaseline() is not null;
    }

    // Called inside the runner's exclusive game-launch gate BEFORE DOOMModLoader
    // stages resources or DOOM launches. Failure cancels launch, not protection.
    internal static void BeforeLaunch(bool resourceModsSelected)
    {
        EnsureGameClosed();
        var source = SaveRoot();
        var state = ReadState();
        VerifySource(state, source);
        if (!HasSaveData(source))
        {
            if (resourceModsSelected)
                throw new InvalidOperationException(
                    "KHARVOX cannot protect the clean save because no campaign saves exist yet. "
                    + "Run DOOM without user mods once before enabling them.");
            return;
        }
        if (resourceModsSelected)
        {
            if (state.LastMode != "modded" || string.IsNullOrWhiteSpace(state.CleanSnapshotId))
                state.CleanSnapshotId = Capture("CLEAN BASELINE before resource mods", "normal").Id;
            else
                Capture("Before modded launch", "modded");
            state.LastMode = "modded";
        }
        else
        {
            Capture(state.LastMode == "modded"
                ? "Normal launch while modded saves are still present"
                : "Before normal launch", state.LastMode);
            // Do not clear pending return-to-clean until an actual restore.
            if (state.LastMode != "modded") state.LastMode = "normal";
        }
        state.Source = source;
        SaveState(state);
    }

    // Pure folder transaction: callable with disposable test saves, never
    // requires the user's real DOOM profile or global backup library.
    private static void ReplaceLiveFolders(string source, string snapshotFolder, Snapshot selected)
    {
        var stage = Path.Combine(source, ".kharvox-restore-" + Guid.NewGuid().ToString("N"));
        var old = Path.Combine(source, ".kharvox-old-" + Guid.NewGuid().ToString("N"));
        bool success = false;
        var movedOriginals = new List<string>();
        var installed = new List<string>();
        try
        {
            CopySaves(snapshotFolder, stage);
            CheckHashes(selected, stage);
            Directory.CreateDirectory(old);
            foreach (var name in SaveNames)
            {
                var live = Path.Combine(source, name);
                if (!Directory.Exists(live)) continue;
                Directory.Move(live, Path.Combine(old, name));
                movedOriginals.Add(name);
            }
            foreach (var name in SaveNames)
            {
                var staged = Path.Combine(stage, name);
                if (!Directory.Exists(staged)) continue;
                Directory.Move(staged, Path.Combine(source, name));
                installed.Add(name);
            }
            success = true;
        }
        catch (Exception error)
        {
            try
            {
                // Revert ONLY the paths we actually moved. A failed first
                // rename must never cause us to move intact live saves away.
                foreach (var name in installed)
                {
                    var live = Path.Combine(source, name);
                    if (Directory.Exists(live))
                        Directory.Move(live, Path.Combine(old, "failed-" + name));
                }
                foreach (var name in movedOriginals)
                {
                    var previous = Path.Combine(old, name);
                    if (Directory.Exists(previous))
                        Directory.Move(previous, Path.Combine(source, name));
                }
            }
            catch (Exception rollbackError)
            {
                throw new InvalidOperationException(
                    "Restore failed AND folder rollback was incomplete. "
                    + "Do not start DOOM. Recovered folders may remain at " + old,
                    new AggregateException(error, rollbackError));
            }
            throw new InvalidOperationException(
                "Restore failed; live files were rolled back.", error);
        }
        finally
        {
            if (success)
            {
                // Cleanup errors must not conceal a successfully restored save.
                try { if (Directory.Exists(old)) Directory.Delete(old, true); }
                catch { }
                try { if (Directory.Exists(stage)) Directory.Delete(stage, true); }
                catch { }
            }
            // Failed restore retains .kharvox-stage / .kharvox-old folders
            // for forensic recovery, plus the verified rescue snapshot.
        }
    }

    internal static Snapshot Restore(string id)
    {
        EnsureGameClosed();
        var source = SaveRoot();
        var state = ReadState();
        VerifySource(state, source);
        var selected = ReadSnapshot(id);
        if (!SamePath(selected.Source, source))
            throw new InvalidOperationException("Cannot restore a different profile's save.");
        CheckHashes(selected, SnapshotFolder(id)); // Verify BEFORE touching live saves.
        var rescue = Capture("SAFETY COPY before restore", state.LastMode);
        try
        {
            ReplaceLiveFolders(source, SnapshotFolder(id), selected);
        }
        catch (Exception error)
        {
            throw new InvalidOperationException(
                "Restore failed. Your untouched pre-restore safety snapshot: "
                + rescue.Id + ". DOOM was not launched.", error);
        }
        state.LastMode = selected.Mode == "normal" ? "normal" : "modded";
        state.Source = source;
        SaveState(state);
        return rescue;
    }

    internal static int RunSelfTest()
    {
        var temp = Path.Combine(Path.GetTempPath(), "KHARVOX-save-test-"
            + Guid.NewGuid().ToString("N"));
        try
        {
            var live = Path.Combine(temp, "base");
            var main = Path.Combine(live, "savegame", "PROFILE");
            var extra = Path.Combine(live, "savegame.user", "profile-a");
            Directory.CreateDirectory(main);
            Directory.CreateDirectory(extra);
            File.WriteAllText(Path.Combine(main, "profile.bin"), "clean");
            File.WriteAllText(Path.Combine(extra, "checkpoint.dat"), "clean checkpoint");
            // Test copy/hash logic without ever touching real save paths.
            var target = Path.Combine(temp, "copy");
            CopySaves(live, target);
            var expected = new Snapshot { Hashes = FileHashes(live) };
            CheckHashes(expected, target);
            File.WriteAllText(Path.Combine(extra, "checkpoint.dat"), "modded");
            var rejected = false;
            try { CheckHashes(expected, live); }
            catch (InvalidDataException) { rejected = true; }
            if (!rejected) throw new InvalidDataException("Hash verification missed changed save.");
            // Full dry-run restoration in an isolated temporary directory.
            // Two save trees must be replaced, not merely one autosave.
            File.WriteAllText(Path.Combine(main, "profile.bin"), "modded profile");
            ReplaceLiveFolders(live, target, expected);
            CheckHashes(expected, live);
            if (File.ReadAllText(Path.Combine(main, "profile.bin")) != "clean"
                || File.ReadAllText(Path.Combine(extra, "checkpoint.dat")) != "clean checkpoint")
                throw new InvalidDataException("Restored save folder content was incorrect.");
            // Corrupt a stored snapshot: verifying BEFORE transaction starts
            // must prevent modification of the currently active save.
            File.WriteAllText(Path.Combine(target, "savegame", "PROFILE", "profile.bin"), "tampered");
            File.WriteAllText(Path.Combine(main, "profile.bin"), "my new progress");
            rejected = false;
            try { ReplaceLiveFolders(live, target, expected); }
            catch (InvalidDataException) { rejected = true; }
            if (!rejected || File.ReadAllText(Path.Combine(main, "profile.bin")) != "my new progress")
                throw new InvalidDataException("Tampered snapshot overwrote live saves.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("KHARVOX save protection self-test failed: " + error);
            return 1;
        }
        finally { try { Directory.Delete(temp, true); } catch { } }
    }
}
