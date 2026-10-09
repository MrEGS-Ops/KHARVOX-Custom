using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Labels reflect the user's judgement, never a claim of automatic compatibility.
// IDs are derived from the effective launcher options, built-in mods and the
// selected DOOM resource mod content. Order of mods does not change the identity.
internal static class KharvoxConfigMarks
{
    internal enum Verdict { Unmarked = 0, Good = 1, Bad = 2 }
    private const int CurrentSchema = 1;
    private const int MaxMarks = 2000;

    internal sealed class Entry
    {
        public string Key { get; set; } = "";
        public Verdict Mark { get; set; }
        public string UpdatedUtc { get; set; } = "";
    }

    internal sealed class Data
    {
        public int Schema { get; set; } = CurrentSchema;
        public List<Entry> Entries { get; set; } = new();
    }

    internal static string DefaultPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "KHARVOX", "known-configurations.json");

    internal static string Fingerprint(
        KharvoxLaunchOptions launch, CustomModSettings mods,
        IEnumerable<KeyValuePair<string, string>> selectedDoomMods)
    {
        var fields = new SortedDictionary<string, string>(StringComparer.Ordinal);
        foreach (var property in typeof(KharvoxLaunchOptions).GetProperties())
        {
            var raw = property.GetValue(launch);
            fields["launch." + property.Name] = FormatValue(raw);
        }
        foreach (var property in typeof(CustomModSettings).GetProperties())
        {
            if (property.Name == nameof(CustomModSettings.Schema)
                || property.Name == nameof(CustomModSettings.SupervisorRequired))
                continue;
            fields["mod." + property.Name] = FormatValue(property.GetValue(mods));
        }
        var selected = selectedDoomMods
            .OrderBy(s => s.Key, StringComparer.OrdinalIgnoreCase).ToArray();
        foreach (var selectedMod in selected)
            fields["doom." + selectedMod.Key.Trim().ToLowerInvariant()] =
                "sha256:" + selectedMod.Value;

        // Version this separately from the file schema. Adding or changing
        // gameplay fields should never misidentify an old marked combination.
        // No selected resource mods: keep old ratings valid. Existing ratings
        // with resource mods MUST NOT be mistaken for content-aware ratings.
        var canonical = new StringBuilder(selected.Length == 0
            ? "KHARVOX-CONFIG-ID-v1\n" : "KHARVOX-CONFIG-ID-v2\n");
        foreach (var pair in fields)
            canonical.Append(pair.Key.Length).Append(':').Append(pair.Key)
                .Append('=').Append(pair.Value.Length).Append(':')
                .Append(pair.Value).Append('\n');
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(
                Encoding.UTF8.GetBytes(canonical.ToString()))).Replace("-", "");
    }

    private static string FormatValue(object? value) =>
        value is null ? "<null>" :
        value is bool flag ? (flag ? "true" : "false") :
        value is IFormattable number ? number.ToString(null,
            System.Globalization.CultureInfo.InvariantCulture) :
        value.ToString() ?? "";

    private static Data Read(string path)
    {
        if (!File.Exists(path)) return new Data();
        try
        {
            var data = new JavaScriptSerializer().Deserialize<Data>(File.ReadAllText(path));
            if (data is null || data.Schema != CurrentSchema || data.Entries is null
                || data.Entries.Count > MaxMarks
                || data.Entries.Any(e => e is null || e.Key.Length != 64
                    || !e.Key.All(Uri.IsHexDigit)
                    || e.Mark is not (Verdict.Good or Verdict.Bad)))
                throw new InvalidDataException("Unrecognised configuration mark data.");
            return data;
        }
        catch (Exception error) when (error is ArgumentException ||
            error is InvalidOperationException || error is IOException ||
            error is UnauthorizedAccessException || error is InvalidDataException)
        {
            throw new InvalidDataException("KHARVOX cannot read configuration marks."
                + " The existing file has not been changed."
                + Environment.NewLine + "Inspect or restore: " + path, error);
        }
    }

    internal static Verdict Lookup(string key, string? path = null) =>
        Read(path ?? DefaultPath).Entries
            .LastOrDefault(item => string.Equals(item.Key, key,
                StringComparison.OrdinalIgnoreCase))?.Mark ?? Verdict.Unmarked;

    internal static void Set(string key, Verdict verdict, string? path = null)
    {
        path ??= DefaultPath;
        var data = Read(path);
        data.Entries.RemoveAll(item => string.Equals(item.Key, key,
            StringComparison.OrdinalIgnoreCase));
        if (verdict is Verdict.Good or Verdict.Bad)
        {
            data.Entries.Add(new Entry
            {
                Key = key,
                Mark = verdict,
                UpdatedUtc = DateTime.UtcNow.ToString("O",
                    System.Globalization.CultureInfo.InvariantCulture)
            });
        }
        if (data.Entries.Count > MaxMarks)
            data.Entries = data.Entries.Skip(data.Entries.Count - MaxMarks).ToList();

        var directory = Path.GetDirectoryName(path)!;
        Directory.CreateDirectory(directory);
        var temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temp, new JavaScriptSerializer().Serialize(data),
                new UTF8Encoding(false));
            if (File.Exists(path)) File.Replace(temp, path, null);
            else File.Move(temp, path);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }

    internal static int RunSelfTest()
    {
        var folder = Path.Combine(Path.GetTempPath(), "KHARVOX-ConfigMarks-"
            + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(folder);
            var file = Path.Combine(folder, "marks.json");
            var one = new string('A', 64);
            var two = new string('B', 64);
            if (Lookup(one, file) != Verdict.Unmarked)
                throw new InvalidDataException("Unmarked configuration was not recognised.");
            Set(one, Verdict.Bad, file);
            Set(two, Verdict.Good, file);
            if (Lookup(one, file) != Verdict.Bad || Lookup(two, file) != Verdict.Good)
                throw new InvalidDataException("User marks did not persist.");
            Set(one, Verdict.Good, file);
            if (Lookup(one, file) != Verdict.Good)
                throw new InvalidDataException("User could not reclassify a configuration.");
            Set(one, Verdict.Unmarked, file);
            if (Lookup(one, file) != Verdict.Unmarked || Lookup(two, file) != Verdict.Good)
                throw new InvalidDataException("Removing a mark affected another combination.");
            File.WriteAllText(file, "{broken");
            var failedSafely = false;
            try { Set(two, Verdict.Bad, file); }
            catch (InvalidDataException) { failedSafely = true; }
            if (!failedSafely || File.ReadAllText(file) != "{broken")
                throw new InvalidDataException("Corrupted mark file was overwritten.");

            Console.WriteLine("KHARVOX configuration marks persistence tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("Configuration marks test failed: " + error);
            return 1;
        }
        finally { try { Directory.Delete(folder, true); } catch { } }
    }
}
