using System.IO.Compression;
using System.Text;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Independent implementation of the DOOM (2016) resource container format.
// KHARVOX doesn't incorporate the source or binaries of any other mod loader.
// This layer builds replacement containers in KHARVOX's own workspace and
// deliberately does not touch the game installation.
internal static class KharvoxResourcePatcher
{
    private static readonly byte[] Signature = { 5, (byte)'S', (byte)'E', (byte)'R' };
    private const int MaxRecords = 200000;
    private const int MaxTextBytes = 16384;
    private const long MaxResourceBytes = 256L * 1024 * 1024;

    internal sealed class Record
    {
        internal int Id;
        internal string Type = "";
        internal string ShortName = "";
        internal string FileName = "";
        internal long Offset;
        internal int PlainSize;
        internal int StoredSize;
        internal int Flags;
        internal byte PatchNumber;
    }

    internal sealed class Result
    {
        internal int ResourceCount;
        internal int Replaced;
        internal int Added;
        internal int ConfigsMerged;
        internal string IndexPath = "";
        internal string PatchPath = "";
        internal string[] Skipped = Array.Empty<string>();
        internal string[] Conflicts = Array.Empty<string>();
    }

    private static int ReadBig32(BinaryReader reader)
    {
        var data = reader.ReadBytes(4);
        if (data.Length != 4) throw new InvalidDataException("Truncated DOOM index.");
        if (BitConverter.IsLittleEndian) Array.Reverse(data);
        return BitConverter.ToInt32(data, 0);
    }

    private static long ReadBig64(BinaryReader reader)
    {
        var data = reader.ReadBytes(8);
        if (data.Length != 8) throw new InvalidDataException("Truncated DOOM index.");
        if (BitConverter.IsLittleEndian) Array.Reverse(data);
        return BitConverter.ToInt64(data, 0);
    }

    private static void Big32(BinaryWriter writer, int value)
    {
        var data = BitConverter.GetBytes(value);
        if (BitConverter.IsLittleEndian) Array.Reverse(data);
        writer.Write(data);
    }

    private static void Big64(BinaryWriter writer, long value)
    {
        var data = BitConverter.GetBytes(value);
        if (BitConverter.IsLittleEndian) Array.Reverse(data);
        writer.Write(data);
    }

    private static string ReadName(BinaryReader reader)
    {
        var count = reader.ReadInt32();
        if (count < 0 || count > MaxTextBytes)
            throw new InvalidDataException("Invalid DOOM resource name length.");
        var data = reader.ReadBytes(count);
        if (data.Length != count)
            throw new InvalidDataException("Truncated DOOM resource name.");
        return new UTF8Encoding(false, true).GetString(data);
    }

    private static void WriteName(BinaryWriter writer, string value)
    {
        var bytes = new UTF8Encoding(false, true).GetBytes(value);
        if (bytes.Length > MaxTextBytes)
            throw new InvalidDataException("Resource name is too long.");
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    internal static List<Record> ReadIndex(string path)
    {
        using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var reader = new BinaryReader(stream, Encoding.UTF8))
        {
            if (!reader.ReadBytes(4).SequenceEqual(Signature))
                throw new InvalidDataException("Not a DOOM v5 resource index.");
            var length = ReadBig32(reader);
            if (stream.Length < 36 || length != stream.Length - 32)
                throw new InvalidDataException("Invalid DOOM index size.");
            if (reader.ReadBytes(24).Any(x => x != 0))
                throw new InvalidDataException("Unexpected DOOM index header.");
            var count = ReadBig32(reader);
            if (count < 0 || count > MaxRecords)
                throw new InvalidDataException("Unexpected number of DOOM resources.");
            var resources = new List<Record>(count);
            for (var i = 0; i < count; i++)
            {
                var item = new Record
                {
                    Id = ReadBig32(reader),
                    Type = ReadName(reader),
                    ShortName = ReadName(reader),
                    FileName = ReadName(reader),
                    Offset = ReadBig64(reader),
                    PlainSize = ReadBig32(reader),
                    StoredSize = ReadBig32(reader),
                    Flags = ReadBig32(reader),
                    PatchNumber = reader.ReadByte()
                };
                if (item.Offset < 0 || item.PlainSize < 0 || item.StoredSize < 0)
                    throw new InvalidDataException("Invalid resource byte range.");
                resources.Add(item);
            }
            if (stream.Position != stream.Length)
                throw new InvalidDataException("Unexpected trailing DOOM index content.");
            return resources;
        }
    }

    internal static void WriteIndex(string destination, IReadOnlyList<Record> records)
    {
        if (records.Count > MaxRecords)
            throw new InvalidDataException("Resource count limit exceeded.");
        using (var stream = new FileStream(destination, FileMode.CreateNew,
                   FileAccess.ReadWrite, FileShare.None))
        using (var writer = new BinaryWriter(stream, Encoding.UTF8))
        {
            writer.Write(Signature);
            Big32(writer, 0);
            writer.Write(new byte[24]);
            Big32(writer, records.Count);
            foreach (var item in records)
            {
                Big32(writer, item.Id);
                WriteName(writer, item.Type);
                WriteName(writer, item.ShortName);
                WriteName(writer, item.FileName);
                Big64(writer, item.Offset);
                Big32(writer, item.PlainSize);
                Big32(writer, item.StoredSize);
                Big32(writer, item.Flags);
                writer.Write(item.PatchNumber);
            }
            var indexBytes = stream.Length - 32;
            if (indexBytes > int.MaxValue)
                throw new InvalidDataException("Index exceeds DOOM format limit.");
            stream.Position = 4;
            Big32(writer, checked((int)indexBytes));
        }
    }

    private sealed class InputResource
    {
        internal string Path = "";
        internal string Source = "";
        internal Func<Stream> Open = null!;
        internal long Size;
    }

    private static string Normalize(string value)
    {
        var canonical = value.Replace('\\', '/').TrimStart('/');
        if (string.IsNullOrWhiteSpace(canonical) || canonical.Contains(":")
            || canonical.Split('/').Any(part => part == "." || part == ".." || part.Length == 0))
            throw new InvalidDataException("Unsafe DOOM mod resource path: " + value);
        return canonical.ToLowerInvariant();
    }

    private static IEnumerable<InputResource> FromMods(IEnumerable<string> selectedMods)
    {
        foreach (var input in selectedMods)
        {
            if (File.Exists(input))
            {
                if (!input.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Selected mod is not a ZIP: " + input);
                if ((File.GetAttributes(input) & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Linked mod archives are unsupported.");
                // Keep the ZIP open while enumerating, but never extract it to disk.
                using (var zip = ZipFile.OpenRead(input))
                {
                    foreach (var entry in zip.Entries)
                    {
                        if (entry.FullName.EndsWith("/", StringComparison.Ordinal))
                            continue;
                        if (entry.Length > MaxResourceBytes)
                            throw new InvalidDataException("Resource exceeds the size limit.");
                        var canonical = Normalize(entry.FullName);
                        // The enumerator keeps this ZIP open while Build consumes
                        // each yielded entry. Stream compressed data straight into
                        // the patch: no memory-sized copy and no staged ZIP files.
                        yield return new InputResource
                        {
                            Path = canonical,
                            Source = input,
                            Size = entry.Length,
                            Open = () => entry.Open()
                        };
                    }
                }
            }
            else if (Directory.Exists(input))
            {
                var root = Path.GetFullPath(input);
                if ((File.GetAttributes(root) & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Linked mod directories are unsupported.");
                foreach (var file in WalkFiles(root))
                {
                    var relative = file.Substring(root.TrimEnd('\\', '/').Length + 1);
                    var canonical = Normalize(relative);
                    var info = new FileInfo(file);
                    if (info.Length > MaxResourceBytes)
                        throw new InvalidDataException("Resource exceeds the size limit.");
                    yield return new InputResource
                    {
                        Path = canonical,
                        Source = input,
                        Size = info.Length,
                        Open = () => File.OpenRead(file)
                    };
                }
            }
            else throw new FileNotFoundException("Selected mod could not be found: " + input);
        }
    }

    private static IEnumerable<string> WalkFiles(string directory)
    {
        var pending = new Stack<string>();
        pending.Push(directory);
        while (pending.Count > 0)
        {
            var current = pending.Pop();
            if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Junctions in mod directories are unsupported.");
            foreach (var file in Directory.GetFiles(current).OrderBy(x => x,
                         StringComparer.OrdinalIgnoreCase))
            {
                if ((File.GetAttributes(file) & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Linked files in mods are unsupported.");
                yield return file;
            }
            foreach (var sub in Directory.GetDirectories(current).OrderByDescending(x => x,
                         StringComparer.OrdinalIgnoreCase))
                pending.Push(sub);
        }
    }

    private static bool TryInferDecl(string path, IReadOnlyList<Record> existing,
        out string type, out string shortName)
    {
        type = "";
        shortName = "";
        const string start = "generated/decls/";
        if (!path.StartsWith(start, StringComparison.Ordinal) || !path.EndsWith(".decl",
                StringComparison.Ordinal)) return false;
        var relative = path.Substring(start.Length);
        var slash = relative.IndexOf('/');
        if (slash <= 0 || slash == relative.Length - 1) return false;
        var folderType = relative.Substring(0, slash);
        var example = existing.FirstOrDefault(item =>
            item.Type.Equals(folderType, StringComparison.OrdinalIgnoreCase));
        if (example is null) return false;
        type = example.Type;
        shortName = relative.Substring(slash + 1);
        shortName = shortName.Substring(0, shortName.Length - ".decl".Length);
        return true;
    }

    private const int MaxMergeConfigSize = 2 * 1024 * 1024;

    private static bool IsMergeableConfig(string path)
    {
        var extension = Path.GetExtension(path);
        return extension.Equals(".json", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".ini", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".cfg", StringComparison.OrdinalIgnoreCase);
    }

    private static byte[] ReadBounded(Stream stream, long expectedSize)
    {
        if (expectedSize < 0 || expectedSize > MaxMergeConfigSize)
            throw new InvalidDataException("Config is too large for safe automatic merging.");
        using (stream)
        using (var buffer = new MemoryStream())
        {
            stream.CopyTo(buffer);
            if (buffer.Length != expectedSize || buffer.Length > MaxMergeConfigSize)
                throw new InvalidDataException("Config resource content length mismatch.");
            return buffer.ToArray();
        }
    }

    private static byte[] ReadOriginalConfig(string indexPath, Record entry)
    {
        if (entry.PlainSize > MaxMergeConfigSize || entry.StoredSize > MaxMergeConfigSize)
            throw new NotSupportedException("Original config too large for merging.");

        var parent = Path.GetDirectoryName(Path.GetFullPath(indexPath))!;
        var stem = Path.GetFileNameWithoutExtension(indexPath);
        if (stem.Length >= 4 && stem[stem.Length - 4] == '_'
            && stem.Substring(stem.Length - 3).All(char.IsDigit))
            stem = stem.Substring(0, stem.Length - 4);

        string dataName;
        if (entry.PatchNumber == 0) dataName = stem + ".resources";
        else if (entry.PatchNumber == 1) dataName = stem + ".patch";
        else dataName = stem + "_" + entry.PatchNumber.ToString("D3") + ".patch";
        var dataPath = Path.Combine(parent, dataName);
        using (var stream = File.OpenRead(dataPath))
        {
            var marker = new byte[4];
            if (stream.Read(marker, 0, marker.Length) != marker.Length
                || !marker.SequenceEqual(Signature)
                || entry.Offset < 4
                || entry.Offset > stream.Length
                || entry.StoredSize > stream.Length - entry.Offset)
                throw new InvalidDataException("Original DOOM config data reference is invalid.");

            stream.Position = entry.Offset;
            var stored = new byte[entry.StoredSize];
            if (stream.Read(stored, 0, stored.Length) != stored.Length)
                throw new InvalidDataException("Original config is truncated.");
            if (entry.PlainSize == entry.StoredSize) return stored;
            using (var input = new MemoryStream(stored, writable: false))
            using (var decompressor = new DeflateStream(input, CompressionMode.Decompress))
                return ReadBounded(decompressor, entry.PlainSize);
        }
    }

    private static Record CopyRecord(Record value) => new Record
    {
        Id = value.Id,
        Type = value.Type,
        ShortName = value.ShortName,
        FileName = value.FileName,
        Offset = value.Offset,
        PlainSize = value.PlainSize,
        StoredSize = value.StoredSize,
        Flags = value.Flags,
        PatchNumber = value.PatchNumber
    };

    private static void WriteChangedResource(Stream target, Record[] entries,
        byte outputPatchNumber, byte[] data)
    {
        var aligned = (target.Position + 15) & ~15L;
        while (target.Position < aligned) target.WriteByte(0);
        var offset = target.Position;
        target.Write(data, 0, data.Length);
        foreach (var item in entries)
        {
            item.Offset = data.Length == 0 ? 0 : offset;
            item.PlainSize = data.Length;
            item.StoredSize = data.Length;
            item.PatchNumber = outputPatchNumber;
            if (data.Length == 0) item.FileName = "";
        }
    }

    internal static Result Build(string originalIndex, IEnumerable<string> selectedMods,
        byte outputPatchNumber, string outputIndex, string outputData,
        Action<string>? update = null)
    {
        if (outputPatchNumber < 1)
            throw new ArgumentOutOfRangeException(nameof(outputPatchNumber));
        if (File.Exists(outputIndex) || File.Exists(outputData))
            throw new IOException("Destination files already exist.");
        var originals = ReadIndex(originalIndex);
        var originalLookup = originals.Where(x => x.FileName.Length != 0)
            .GroupBy(x => x.FileName.Replace('\\', '/').ToLowerInvariant())
            .ToDictionary(g => g.Key, g => g.First(), StringComparer.Ordinal);
        var records = originals.Select(CopyRecord).ToList();
        var lookups = records.Where(x => x.FileName.Length != 0)
            .GroupBy(x => x.FileName.Replace('\\', '/').ToLowerInvariant())
            .ToDictionary(g => g.Key, g => g.ToArray(), StringComparer.Ordinal);
        // Never choose an implicit winner based on mod enumeration order.
        // The user should disable an overlapping mod or explicitly resolve it.
        var seen = new Dictionary<string, string>(StringComparer.Ordinal);
        var mergedConfig = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        var result = new Result { ResourceCount = records.Count,
            IndexPath = outputIndex, PatchPath = outputData };
        var skipped = new List<string>();
        var conflicts = new List<string>();
        try
        {
            using (var target = new FileStream(outputData, FileMode.CreateNew, FileAccess.Write))
            {
                target.Write(Signature, 0, Signature.Length);
                foreach (var resource in FromMods(selectedMods))
                {
                    if (seen.TryGetValue(resource.Path, out var firstSource))
                    {
                        string reason;
                        if (IsMergeableConfig(resource.Path)
                            && mergedConfig.TryGetValue(resource.Path, out var previouslyApplied)
                            && originalLookup.TryGetValue(resource.Path, out var original))
                        {
                            var baseline = ReadOriginalConfig(originalIndex, original);
                            var incoming = ReadBounded(resource.Open(), resource.Size);
                            if (KharvoxSimpleConfigMerge.TryMerge(resource.Path,
                                    baseline, previouslyApplied, incoming,
                                    out var merged, out reason))
                            {
                                WriteChangedResource(target, lookups[resource.Path],
                                    outputPatchNumber, merged);
                                mergedConfig[resource.Path] = merged;
                                result.ConfigsMerged++;
                                update?.Invoke("Merged simple config " + resource.Path);
                                continue;
                            }
                        }
                        else reason = "Unsupported resource type or no original config available.";

                        throw new InvalidOperationException(
                            "DOOM mod conflict: two enabled mods modify the same resource."
                            + Environment.NewLine + "Resource: " + resource.Path
                            + Environment.NewLine + "Mod 1: " + firstSource
                            + Environment.NewLine + "Mod 2: " + resource.Source
                            + Environment.NewLine + "Reason: " + reason
                            + Environment.NewLine + "Uncheck one of these mods in KHARVOX."
                            + Environment.NewLine + "No file was installed or overwritten.");
                    }
                    seen.Add(resource.Path, resource.Source);
                    if (resource.Path == "mod.decl"
                        || resource.Path == "fileids.txt"
                        || resource.Path.StartsWith("video/", StringComparison.Ordinal)
                        || resource.Path.StartsWith("generated/binaryfile/", StringComparison.Ordinal))
                    {
                        // Special resources need explicit handling; never quietly
                        // treat them as normal files and corrupt the game.
                        skipped.Add(resource.Path + " (special resource unsupported)");
                        continue;
                    }
                    if (!lookups.TryGetValue(resource.Path, out var matches))
                    {
                        if (!TryInferDecl(resource.Path, records, out var type, out var name))
                        {
                            skipped.Add(resource.Path + " (new resource type unknown)");
                            continue;
                        }
                        var record = new Record
                        {
                            Id = records.Count, Type = type, ShortName = name,
                            FileName = resource.Path
                        };
                        records.Add(record);
                        lookups.Add(resource.Path, new[] { record });
                        matches = new[] { record };
                        result.Added++;
                    }
                    else result.Replaced += matches.Length;

                    if (IsMergeableConfig(resource.Path)
                        && resource.Size <= MaxMergeConfigSize)
                    {
                        // Keep small structured config data for a possible
                        // three-way merge; ordinary resources still stream.
                        var bytes = ReadBounded(resource.Open(), resource.Size);
                        WriteChangedResource(target, matches, outputPatchNumber, bytes);
                        mergedConfig[resource.Path] = bytes;
                    }
                    else
                    {
                        var aligned = (target.Position + 15) & ~15L;
                        while (target.Position < aligned) target.WriteByte(0);
                        var offset = target.Position;
                        using (var input = resource.Open()) input.CopyTo(target);
                        var written = target.Position - offset;
                        if (written != resource.Size || written > int.MaxValue)
                            throw new InvalidDataException("Mod resource size mismatch.");
                        foreach (var record in matches)
                        {
                            record.Offset = written == 0 ? 0 : offset;
                            record.PlainSize = (int)written;
                            record.StoredSize = (int)written;
                            record.PatchNumber = outputPatchNumber;
                            if (written == 0) record.FileName = "";
                        }
                    }
                    update?.Invoke("Patched " + resource.Path);
                }
            }
            if (skipped.Count != 0)
                throw new NotSupportedException(
                    "Some mod resources are not yet supported by KHARVOX Resource Patcher:"
                    + Environment.NewLine + string.Join(Environment.NewLine, skipped.Take(16)));
            WriteIndex(outputIndex, records);
            result.ResourceCount = records.Count;
            result.Skipped = skipped.ToArray();
            result.Conflicts = conflicts.ToArray();
            return result;
        }
        catch
        {
            try { File.Delete(outputIndex); } catch { }
            try { File.Delete(outputData); } catch { }
            throw;
        }
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "Kharvox-Resource-Patcher-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            var index = Path.Combine(root, "gameresources.index");
            var original = new List<Record>
            {
                new Record { Id = 11, Type = "entityDef",
                    ShortName = "ai/imp",
                    FileName = "generated/decls/entitydef/ai/imp.decl",
                    Offset = 16, PlainSize = 4, StoredSize = 4 },
                new Record { Id = 12, Type = "entityDef",
                    ShortName = "ai/imp",
                    FileName = "generated/decls/entitydef/ai/imp.decl",
                    Offset = 42, PlainSize = 4, StoredSize = 4 },
                new Record { Id = 13, Type = "entityDef",
                    ShortName = "ai/other",
                    FileName = "generated/decls/entitydef/ai/other.decl",
                    Offset = 64, PlainSize = 9, StoredSize = 6 }
            };
            WriteIndex(index, original);
            var parsed = ReadIndex(index);
            if (parsed.Count != original.Count || parsed[2].StoredSize != 6)
                throw new InvalidDataException("DOOM v5 index round-trip failed.");

            var folder = Path.Combine(root, "MyMod");
            var declDir = Path.Combine(folder, "generated", "decls", "entitydef", "ai");
            Directory.CreateDirectory(declDir);
            File.WriteAllText(Path.Combine(declDir, "imp.decl"), "new-imp");
            File.WriteAllText(Path.Combine(declDir, "summoner.decl"), "new-summoner");
            var outputIndex = Path.Combine(root, "gameresources.pindex");
            var outputData = Path.Combine(root, "gameresources.patch");
            var result = Build(index, new[] { folder }, 1, outputIndex, outputData);
            var updated = ReadIndex(outputIndex);
            var imps = updated.Where(x => x.ShortName == "ai/imp").ToArray();
            if (result.Replaced != 2 || result.Added != 1
                || imps.Length != 2 || imps.Any(x => x.PatchNumber != 1
                    || x.PlainSize != 7 || x.Offset % 16 != 0)
                || updated.Count != 4)
                throw new InvalidDataException("Replacement/duplicate/new-decl output failed.");

            using (var patch = File.OpenRead(outputData))
            {
                var marker = new byte[4];
                if (patch.Read(marker, 0, 4) != 4 || !marker.SequenceEqual(Signature))
                    throw new InvalidDataException("Patch signature invalid.");
                patch.Position = imps[0].Offset;
                var bytes = new byte[7];
                if (patch.Read(bytes, 0, 7) != 7 ||
                    Encoding.UTF8.GetString(bytes) != "new-imp")
                    throw new InvalidDataException("Patch resource bytes invalid.");
            }

            // A compressed ZIP is consumed directly, without extracting or
            // duplicating it into KHARVOX's staging folder.
            var zipPath = Path.Combine(root, "CompressedMod.zip");
            using (var archive = ZipFile.Open(zipPath, ZipArchiveMode.Create))
            {
                var entry = archive.CreateEntry(
                    "generated/decls/entitydef/ai/imp.decl", CompressionLevel.Optimal);
                using (var writer = new StreamWriter(entry.Open(), new UTF8Encoding(false)))
                    writer.Write("zip-imp");
            }
            var zipIndex = Path.Combine(root, "ziptest.pindex");
            var zipData = Path.Combine(root, "ziptest.patch");
            var zipResult = Build(index, new[] { zipPath }, 2, zipIndex, zipData);
            var zipEntries = ReadIndex(zipIndex)
                .Where(x => x.ShortName == "ai/imp").ToArray();
            if (zipResult.Replaced != 2 || zipEntries.Length != 2
                || zipEntries.Any(x => x.PatchNumber != 2 || x.PlainSize != 7))
                throw new InvalidDataException("Compressed ZIP patch failed.");
            using (var stream = File.OpenRead(zipData))
            {
                stream.Position = zipEntries[0].Offset;
                var bytes = new byte[7];
                if (stream.Read(bytes, 0, 7) != 7
                    || Encoding.UTF8.GetString(bytes) != "zip-imp")
                    throw new InvalidDataException("Compressed ZIP data does not match.");
            }

            // Never silently pick a winner when two selected mods replace
            // the same resource. They must be resolved by the user, not order.
            var overlapRejected = false;
            var overlapIndex = Path.Combine(root, "overlap.pindex");
            var overlapData = Path.Combine(root, "overlap.patch");
            try { Build(index, new[] { folder, zipPath }, 1, overlapIndex, overlapData); }
            catch (InvalidOperationException error)
            {
                overlapRejected = error.Message.Contains("Mod 1:")
                    && error.Message.Contains("Mod 2:")
                    && error.Message.Contains("ai/imp.decl");
            }
            if (!overlapRejected || File.Exists(overlapIndex) || File.Exists(overlapData))
                throw new InvalidDataException(
                    "Overlapping resource mods must stop with an explicit source-aware conflict.");

            // Two config mods touching separate fields in one existing game
            // resource must combine; conflicting changes to one field must stop.
            var jsonRoot = Path.Combine(root, "json-merge");
            var jsonIndex = Path.Combine(jsonRoot, "gameresources.index");
            var jsonBaseData = Path.Combine(jsonRoot, "gameresources.resources");
            Directory.CreateDirectory(jsonRoot);
            var baseline = Encoding.UTF8.GetBytes("{\"health\":100,\"armor\":50}");
            using (var baseStream = File.Create(jsonBaseData))
            {
                baseStream.Write(Signature, 0, Signature.Length);
                for (var i = 4; i < 16; i++) baseStream.WriteByte(0);
                baseStream.Write(baseline, 0, baseline.Length);
            }
            WriteIndex(jsonIndex, new[]
            {
                new Record
                {
                    Id = 21, Type = "file", ShortName = "config/player",
                    FileName = "config/player.json", Offset = 16,
                    PlainSize = baseline.Length, StoredSize = baseline.Length
                }
            });
            var healthMod = Path.Combine(root, "HealthMod");
            var armorMod = Path.Combine(root, "ArmorMod");
            var otherHealthMod = Path.Combine(root, "OtherHealthMod");
            foreach (var folderPath in new[] { healthMod, armorMod, otherHealthMod })
                Directory.CreateDirectory(Path.Combine(folderPath, "config"));
            File.WriteAllText(Path.Combine(healthMod, "config", "player.json"),
                "{\"health\":200,\"armor\":50}");
            File.WriteAllText(Path.Combine(armorMod, "config", "player.json"),
                "{\"health\":100,\"armor\":150}");
            File.WriteAllText(Path.Combine(otherHealthMod, "config", "player.json"),
                "{\"health\":300,\"armor\":50}");
            var mergedIndex = Path.Combine(root, "merged.pindex");
            var mergedPatch = Path.Combine(root, "merged.patch");
            var mergedResult = Build(jsonIndex, new[] { healthMod, armorMod },
                1, mergedIndex, mergedPatch);
            var mergedEntry = ReadIndex(mergedIndex).Single();
            using (var mergedStream = File.OpenRead(mergedPatch))
            {
                mergedStream.Position = mergedEntry.Offset;
                var config = new byte[mergedEntry.PlainSize];
                if (mergedStream.Read(config, 0, config.Length) != config.Length)
                    throw new InvalidDataException("Merged config patch is truncated.");
                var value = new JavaScriptSerializer().DeserializeObject(
                    Encoding.UTF8.GetString(config)) as Dictionary<string, object>;
                if (value is null || Convert.ToInt32(value["health"]) != 200
                    || Convert.ToInt32(value["armor"]) != 150
                    || mergedResult.ConfigsMerged != 1)
                    throw new InvalidDataException("Independent health/armor config merging failed.");
            }
            var conflictRejected = false;
            var conflictIndex = Path.Combine(root, "conflict.pindex");
            var conflictPatch = Path.Combine(root, "conflict.patch");
            try { Build(jsonIndex, new[] { healthMod, otherHealthMod }, 1,
                conflictIndex, conflictPatch); }
            catch (InvalidOperationException error)
            {
                conflictRejected = error.Message.Contains("$.health")
                    && error.Message.Contains("Mod 1:")
                    && error.Message.Contains("Mod 2:");
            }
            if (!conflictRejected || File.Exists(conflictIndex) || File.Exists(conflictPatch))
                throw new InvalidDataException("Same-field health override must fail without partial output.");

            var unsupported = Path.Combine(root, "Unsupported");
            Directory.CreateDirectory(unsupported);
            File.WriteAllText(Path.Combine(unsupported, "mod.decl"), "test");
            var rejected = false;
            try { Build(index, new[] { unsupported }, 1,
                Path.Combine(root, "bad.pindex"), Path.Combine(root, "bad.patch")); }
            catch (NotSupportedException) { rejected = true; }
            if (!rejected || File.Exists(Path.Combine(root, "bad.patch")))
                throw new InvalidDataException("Unsupported resource must fail cleanly.");

            Console.WriteLine("KHARVOX resource index/patch round-trip tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("KHARVOX resource patcher test failed: " + error);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
