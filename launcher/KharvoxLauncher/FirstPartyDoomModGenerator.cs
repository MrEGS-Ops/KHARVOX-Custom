using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace KharvoxLauncher;

// Uses only the player's unmodified, installed DOOM 2016 v5 resource archive.
// No Python, Store execution aliases, Python downloads or game-file writes.
internal static class FirstPartyDoomModGenerator
{
    private const string Ai = "generated/decls/aiglobalsettings/default.decl;aiGlobalSettings";
    private const string Gib = "generated/decls/gorebehavior/gorebehavior/firearm/8_gauge.decl;goreBehavior";
    private const string AiArchive = "KHARVOX-Aggressive-Demons.zip";
    private const string GibArchive = "KHARVOX-Enhanced-Gibs.zip";
    private static readonly UTF8Encoding Utf8Strict = new UTF8Encoding(false, true);
    private static readonly Regex Number = new Regex(@"[+-]?(?:\d+(?:\.\d*)?|\.\d+)", RegexOptions.CultureInvariant);

    internal static bool CanOffer(string? gameDirectory) =>
        !string.IsNullOrWhiteSpace(gameDirectory)
        && File.Exists(Path.Combine(gameDirectory, "DOOMx64vk.exe"))
        && File.Exists(Path.Combine(gameDirectory, "base", "gameresources.index"))
        && File.Exists(Path.Combine(gameDirectory, "base", "gameresources.resources"));

    internal static Task<string> GenerateAsync(string gameDirectory)
    {
        if (!CanOffer(gameDirectory))
            throw new InvalidOperationException(
                "Choose a DOOM 2016 installation with base/gameresources.index "
                + "and base/gameresources.resources. No resources were modified.");
        return Task.Run(() => GenerateFromGame(gameDirectory, DoomUserMods.PackagedFolder));
    }

    private static string GenerateFromGame(string gameDir, string outputDir)
    {
        // Validate BOTH input declarations before creating any output ZIP.
        var inputs = new[] {
            (resource: Ai, filename: AiArchive),
            (resource: Gib, filename: GibArchive)
        };
        var pending = new List<(string resource, string filename, byte[] content)>();
        foreach (var input in inputs) {
            var source = ReadOriginal(gameDir, input.resource);
            byte[] original = source.data;
            string text = Decode(original);
            bool ai = input.resource == Ai;
            string result = ai
                ? Aggressive(text, 6, "0.5")
                : EnhancedGibs(text, 2, 5, 48);
            if (ContainsCrLf(original))
                result = result.Replace("\r\n", "\n").Replace("\n", "\r\n");
            var encoded = Utf8Strict.GetBytes(result);
            if (original.Length >= 3 && original[0] == 0xEF
                && original[1] == 0xBB && original[2] == 0xBF)
                encoded = new byte[] { 0xEF, 0xBB, 0xBF }.Concat(encoded).ToArray();
            if (original.SequenceEqual(encoded))
                throw new InvalidDataException("Values do not change original declaration: " + input.resource);
            // Use the verified index path, not a guessed ZIP identifier.
            pending.Add((source.resourceName, input.filename, encoded));
        }

        Directory.CreateDirectory(outputDir);
        var reports = new List<string>();
        foreach (var item in pending) {
            string target = Path.Combine(outputDir, item.filename);
            string temp = Path.Combine(outputDir, ".kharvox-mod-" + Guid.NewGuid().ToString("N") + ".zip");
            try {
                using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None))
                using (var zip = new ZipArchive(stream, ZipArchiveMode.Create, false)) {
                    var entry = zip.CreateEntry(item.resource, CompressionLevel.Optimal);
                    entry.LastWriteTime = new DateTimeOffset(2026, 1, 1, 0, 0, 0, TimeSpan.Zero);
                    using (var writer = entry.Open())
                        writer.Write(item.content, 0, item.content.Length);
                }
                using (var stream = File.OpenRead(temp))
                using (var zip = new ZipArchive(stream, ZipArchiveMode.Read)) {
                    if (zip.Entries.Count != 1 || zip.Entries[0].FullName != item.resource)
                        throw new InvalidDataException("Generated resource ZIP structure invalid");
                    using (var entryStream = zip.Entries[0].Open())
                    using (var copied = new MemoryStream()) {
                        entryStream.CopyTo(copied);
                        if (!item.content.SequenceEqual(copied.ToArray()))
                            throw new InvalidDataException("Generated resource ZIP verification failed");
                    }
                }
                // Never truncate an old working mod if creation fails.
                if (File.Exists(target))
                    File.Replace(temp, target, null);
                else
                    File.Move(temp, target);
                reports.Add("Created " + target);
            } finally {
                if (File.Exists(temp)) File.Delete(temp);
            }
        }
        return string.Join(Environment.NewLine, reports)
            + Environment.NewLine + "Unselected by default; enable verified mods manually.";
    }

    private static string Aggressive(string text, int simultaneous, string cooldown)
    {
        if (!Regex.IsMatch(text, @"\btokenData\s*=\s*\{")
            || !Regex.IsMatch(text, @"\bdifficulty_Entries\s*=\s*\{"))
            throw new InvalidDataException("Unrecognized original AI token structure");
        int secondsCount, attackersCount;
        text = EditScalar(text, "secondsBetweenUses", cooldown, out secondsCount);
        text = EditScalar(text, "maxSimultaneousUsers", simultaneous.ToString(), out attackersCount);
        if (secondsCount != attackersCount)
            throw new InvalidDataException("AI cooldown and simultaneous attacker fields differ");
        return text;
    }

    private static string EnhancedGibs(string text, int minimum, int maximum, int impulse)
    {
        if (!Regex.IsMatch(text, @"\bmultiGibInfo\s*=\s*\{"))
            throw new InvalidDataException("Unrecognized original gore structure");
        int n;
        text = EditScalar(text, "minLimbsToRemove", minimum.ToString(), out n);
        text = EditScalar(text, "maxLimbsToRemove", maximum.ToString(), out n);
        text = EditScalar(text, "gibImpulse", impulse.ToString(), out n);
        return text;
    }

    private static string EditScalar(string text, string key, string value, out int count)
    {
        string pattern = @"^([ \t]*" + Regex.Escape(key)
            + @"[ \t]*=[ \t]*)(" + Number.ToString() + @")([ \t]*;)";
        int changes = 0;
        string edited = Regex.Replace(text, pattern, match => {
            ++changes;
            return match.Groups[1].Value + value + match.Groups[3].Value;
        }, RegexOptions.Multiline | RegexOptions.CultureInvariant);
        if (changes == 0)
            throw new InvalidDataException("Original declaration is missing " + key);
        count = changes;
        return edited;
    }

    private static bool ContainsCrLf(byte[] bytes)
    {
        for (int i = 1; i < bytes.Length; ++i)
            if (bytes[i - 1] == 13 && bytes[i] == 10) return true;
        return false;
    }

    private static string Decode(byte[] data)
    {
        if (data.Length < 3 || data.Length > 2_000_000)
            throw new InvalidDataException("Unexpected declaration size");
        int bom = data.Length >= 3 && data[0] == 0xEF
            && data[1] == 0xBB && data[2] == 0xBF ? 3 : 0;
        return Utf8Strict.GetString(data, bom, data.Length - bom);
    }

    // Only a semicolon-free spelling of the SAME declaration is a safe alias.
    // Never substitute an unrelated resource because its category happens to match.
    private static bool EquivalentIndexName(string indexed, string expected)
    {
        var expectedParts = expected.Split(new[] { ';' }, 2);
        var indexedParts = indexed.Replace('\\', '/').Split(new[] { ';' }, 2);
        return string.Equals(indexedParts[0], expectedParts[0], StringComparison.OrdinalIgnoreCase)
            && (indexedParts.Length == 1
                || (expectedParts.Length == 2
                    && string.Equals(indexedParts[1], expectedParts[1], StringComparison.OrdinalIgnoreCase)));
    }

    private static (byte[] data, string resourceName) ReadOriginal(string game, string relative)
    {
        string baseDir = Path.Combine(game, "base");
        string indexPath = Path.Combine(baseDir, "gameresources.index");
        string resourcesPath = Path.Combine(baseDir, "gameresources.resources");
        long offset = -1;
        int plain = 0, stored = 0;
        string resolvedName = "";
        var exact = new List<(string name, long offset, int plain, int stored, byte patch)>();
        var aliases = new List<(string name, long offset, int plain, int stored, byte patch)>();
        var candidates = new List<string>();
        int indexCount = 0;
        using (var stream = new FileStream(indexPath, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var reader = new BinaryReader(stream, Encoding.UTF8)) {
            if (Encoding.ASCII.GetString(reader.ReadBytes(4)) != "\x05SER")
                throw new InvalidDataException("Unrecognized original game resource index");
            uint reportedSize = ReadBigUInt32(reader);
            if (stream.Length < 36 || reportedSize != stream.Length - 32
                || reader.ReadBytes(24).Any(x => x != 0))
                throw new InvalidDataException("Malformed DOOM v5 resource index header");
            uint records = ReadBigUInt32(reader);
            if (records > 200000)
                throw new InvalidDataException("Invalid original resource record count");
            for (uint i = 0; i < records; ++i) {
                reader.ReadBytesExact(4);
                var category = ReadName(reader);
                var shortName = ReadName(reader);
                var name = ReadName(reader);
                long valueOffset = ReadBigInt64(reader);
                int valuePlain = ReadBigInt32(reader);
                int valueStored = ReadBigInt32(reader);
                ReadBigInt32(reader); // flags
                byte patch = reader.ReadByte();
                ++indexCount;
                var record = (name, valueOffset, valuePlain, valueStored, patch);
                if (string.Equals(name, relative, StringComparison.OrdinalIgnoreCase))
                    exact.Add(record);
                else if (EquivalentIndexName(name, relative))
                    aliases.Add(record);
                // A limited, read-only hint for resolving other installation layouts.
                if (name.IndexOf(relative.Contains("aiglobalsettings") ? "aiglobalsettings" : "8_gauge",
                        StringComparison.OrdinalIgnoreCase) >= 0 && candidates.Count < 6)
                    candidates.Add(category + " / " + shortName + " : " + name);
            }
            if (stream.Position != stream.Length)
                throw new InvalidDataException("Unexpected trailing DOOM resource index data");
            var matches = exact.Count > 0 ? exact : aliases;
            if (matches.Count != 1)
                throw new InvalidDataException(
                    "Cannot uniquely identify original declaration: " + relative
                    + Environment.NewLine + "Exact matches=" + exact.Count
                    + ", safe path aliases=" + aliases.Count + ", index entries=" + indexCount
                    + Environment.NewLine
                    + (candidates.Count == 0 ? "No similarly named entries found."
                        : "Nearby index entries:" + Environment.NewLine + string.Join(Environment.NewLine, candidates))
                    + Environment.NewLine + "Nothing was generated. To investigate, supply DOOM/base/gameresources.index (metadata only).");
            var match = matches[0];
            if (match.patch != 0)
                throw new InvalidDataException("Target declaration is patched; restore original files");
            resolvedName = match.name;
            offset = match.offset; plain = match.plain; stored = match.stored;
            if(exact.Count == 0)
                System.Diagnostics.Debug.WriteLine("KHARVOX original declaration resolved by exact base path: " + resolvedName);
        }
        if (plain < 3 || plain > 2_000_000 || stored < 1 || stored > 2_000_000)
            throw new InvalidDataException("Unexpected original resource size");
        using (var stream = new FileStream(resourcesPath, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var reader = new BinaryReader(stream)) {
            if (Encoding.ASCII.GetString(reader.ReadBytes(4)) != "\x05SER")
                throw new InvalidDataException("Unrecognized original resource container");
            if (offset < 4 || offset > stream.Length - stored)
                throw new InvalidDataException("Original resource byte range invalid");
            stream.Position = offset;
            byte[] blob = reader.ReadBytes(stored);
            if (blob.Length != stored)
                throw new EndOfStreamException("Original resource truncated");
            byte[] data;
            if (plain == stored) data = blob;
            else {
                // DOOM 2016 v5 entries use raw DEFLATE (no zlib wrapper).
                using (var compressed = new MemoryStream(blob))
                using (var inflater = new DeflateStream(compressed, CompressionMode.Decompress))
                using (var extracted = new MemoryStream()) {
                    inflater.CopyTo(extracted);
                    if (extracted.Length > plain)
                        throw new InvalidDataException("Decompressed resource exceeds index length");
                    data = extracted.ToArray();
                }
            }
            if (data.Length != plain)
                throw new InvalidDataException("Original resource length mismatch");
            Decode(data);
            return (data, resolvedName);
        }
    }

    private static string ReadName(BinaryReader reader)
    {
        uint length = reader.ReadUInt32(); // DOOM string length is little-endian
        if (length > 16384)
            throw new InvalidDataException("Original resource name too long");
        byte[] data = reader.ReadBytes((int)length);
        if (data.Length != length)
            throw new EndOfStreamException("Truncated original resource name");
        return Utf8Strict.GetString(data);
    }
    private static uint ReadBigUInt32(BinaryReader reader)
    {
        var bytes = reader.ReadBytes(4);
        if (bytes.Length != 4) throw new EndOfStreamException();
        return (uint)(bytes[0] << 24 | bytes[1] << 16 | bytes[2] << 8 | bytes[3]);
    }
    private static int ReadBigInt32(BinaryReader reader) => unchecked((int)ReadBigUInt32(reader));
    private static long ReadBigInt64(BinaryReader reader)
    {
        var bytes = reader.ReadBytes(8);
        if (bytes.Length != 8) throw new EndOfStreamException();
        ulong result = 0;
        foreach (byte b in bytes) result = (result << 8) | b;
        return unchecked((long)result);
    }

    internal static bool RunSelfTest()
    {
        string aiText = "{\n tokenData = {\n difficulty_Entries = {\n"
            + " secondsBetweenUses = 12;\n maxSimultaneousUsers = 2;\n } } }\n";
        string gibText = "{\r\n multiGibInfo = {\r\n minLimbsToRemove = 1;\r\n"
            + " maxLimbsToRemove = 3;\r\n gibImpulse = 25;\r\n } }\r\n";
        string a = Aggressive(aiText, 6, "0.5");
        string b = EnhancedGibs(gibText, 2, 5, 48);
        if (!a.Contains("secondsBetweenUses = 0.5;")
            || !a.Contains("maxSimultaneousUsers = 6;")
            || !b.Contains("minLimbsToRemove = 2;")
            || !b.Contains("maxLimbsToRemove = 5;")
            || !b.Contains("gibImpulse = 48;"))
            return false;
        try { Aggressive("tokenData = {}", 6, "0.5"); return false; }
        catch (InvalidDataException) { }
        try { EnhancedGibs("edit = {}", 2, 5, 48); return false; }
        catch (InvalidDataException) { }
        if (CanOffer("") || CanOffer(Path.Combine(Path.GetTempPath(), "missing-doom")))
            return false;
        // End-to-end deterministic synthetic DOOM v5 index/archive fixture.
        // No Bethesda assets or locally installed Python are involved.
        string root = Path.Combine(Path.GetTempPath(), "kharvox-native-mods-" + Guid.NewGuid().ToString("N"));
        try {
            string game = Path.Combine(root, "game");
            string baseDir = Path.Combine(game, "base");
            Directory.CreateDirectory(baseDir);
            File.WriteAllBytes(Path.Combine(game, "DOOMx64vk.exe"), new byte[] { 0 });
            byte[] sourceAi = Utf8Strict.GetBytes(aiText);
            byte[] sourceGib = Utf8Strict.GetBytes(gibText);
            byte[] packedAi;
            using (var compressed = new MemoryStream()) {
                using (var deflater = new DeflateStream(compressed, CompressionLevel.Optimal, true))
                    deflater.Write(sourceAi, 0, sourceAi.Length);
                packedAi = compressed.ToArray();
            }
            // Fixture must exercise both compressed and uncompressed records.
            using (var resources = new FileStream(Path.Combine(baseDir,"gameresources.resources"),
                       FileMode.CreateNew)) {
                resources.Write(new byte[] { 5, 83, 69, 82 }, 0, 4);
                resources.Write(packedAi, 0, packedAi.Length);
                resources.Write(sourceGib, 0, sourceGib.Length);
            }
            using (var entries = new MemoryStream())
            using (var eb = new BinaryWriter(entries, Encoding.UTF8, true)) {
                WriteFixtureRecord(eb, Ai, 4, sourceAi.Length, packedAi.Length);
                WriteFixtureRecord(eb, Gib, 4 + packedAi.Length, sourceGib.Length, sourceGib.Length);
                eb.Flush();
                using (var indexStream = File.Create(Path.Combine(baseDir,"gameresources.index")))
                using (var header = new BinaryWriter(indexStream)) {
                    header.Write(new byte[] { 5, 83, 69, 82 });
                    WriteFixtureBig(header, checked((uint)(4 + entries.Length)));
                    header.Write(new byte[24]);
                    WriteFixtureBig(header, (uint)2);
                    header.Write(entries.ToArray());
                }
            }
            if (!CanOffer(game)) return false;
            string outDir = Path.Combine(root, "out");
            string report = GenerateFromGame(game, outDir);
            if (!report.Contains(AiArchive) || !report.Contains(GibArchive)) return false;
            using (var zip = ZipFile.OpenRead(Path.Combine(outDir, AiArchive)))
            using (var entry = zip.Entries.Single().Open())
            using (var copy = new MemoryStream()) {
                entry.CopyTo(copy);
                if (!Utf8Strict.GetString(copy.ToArray()).Contains("maxSimultaneousUsers = 6;"))
                    return false;
            }
            using (var zip = ZipFile.OpenRead(Path.Combine(outDir, GibArchive)))
            using (var entry = zip.Entries.Single().Open())
            using (var copy = new MemoryStream()) {
                entry.CopyTo(copy);
                if (!Utf8Strict.GetString(copy.ToArray()).Contains("gibImpulse = 48;"))
                    return false;
            }
            return true;
        } finally {
            if (Directory.Exists(root)) Directory.Delete(root,true);
        }
    }

    private static void WriteFixtureRecord(BinaryWriter writer, string name, long offset, int plain, int stored)
    {
        writer.Write(new byte[] { 0, 0, 0, 1 });
        WriteFixtureName(writer, "original"); WriteFixtureName(writer, "default");
        WriteFixtureName(writer, name);
        foreach (byte b in BitConverter.GetBytes(offset).Reverse()) writer.Write(b);
        WriteFixtureBig(writer, unchecked((uint)plain));
        WriteFixtureBig(writer, unchecked((uint)stored));
        WriteFixtureBig(writer, 0u);
        writer.Write((byte)0);
    }
    private static void WriteFixtureName(BinaryWriter writer,string text)
    {
        var raw=Utf8Strict.GetBytes(text);
        writer.Write((uint)raw.Length); // little endian
        writer.Write(raw);
    }
    private static void WriteFixtureBig(BinaryWriter writer,uint value)
    {
        writer.Write((byte)(value>>24));
        writer.Write((byte)(value>>16));
        writer.Write((byte)(value>>8));
        writer.Write((byte)value);
    }
}

internal static class KharvoxBinaryReaderExtensions
{
    internal static void ReadBytesExact(this BinaryReader reader, int count)
    {
        if (reader.ReadBytes(count).Length != count) throw new EndOfStreamException();
    }
}
