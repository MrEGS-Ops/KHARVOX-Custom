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
            byte[] original = ReadOriginal(gameDir, input.resource);
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
            pending.Add((input.resource, input.filename, encoded));
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

    private static byte[] ReadOriginal(string game, string relative)
    {
        string baseDir = Path.Combine(game, "base");
        string indexPath = Path.Combine(baseDir, "gameresources.index");
        string resourcesPath = Path.Combine(baseDir, "gameresources.resources");
        long offset = -1;
        int plain = 0, stored = 0, found = 0;
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
                ReadName(reader); ReadName(reader);
                var name = ReadName(reader);
                long valueOffset = ReadBigInt64(reader);
                int valuePlain = ReadBigInt32(reader);
                int valueStored = ReadBigInt32(reader);
                ReadBigInt32(reader); // flags
                byte patch = reader.ReadByte();
                if (!string.Equals(name, relative, StringComparison.OrdinalIgnoreCase)) continue;
                ++found;
                if (patch != 0)
                    throw new InvalidDataException("Target declaration is patched; restore original files");
                offset = valueOffset; plain = valuePlain; stored = valueStored;
            }
            if (stream.Position != stream.Length || found != 1)
                throw new InvalidDataException("Original resource absent/ambiguous in game index: " + relative);
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
            return data;
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
        return !CanOffer("") && !CanOffer(Path.Combine(Path.GetTempPath(), "missing-doom"));
    }
}

internal static class KharvoxBinaryReaderExtensions
{
    internal static void ReadBytesExact(this BinaryReader reader, int count)
    {
        if (reader.ReadBytes(count).Length != count) throw new EndOfStreamException();
    }
}
