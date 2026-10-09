using System.Globalization;
using System.Text;
using System.Web.Script.Serialization;

namespace KharvoxLauncher;

// Conservative three-way merge for small, human-readable config resources.
// A mod's value is a change only when it differs from the original value.
// Unknown formats and overlapping changes are never silently resolved.
internal static class KharvoxSimpleConfigMerge
{
    private const int MaxTextBytes = 2 * 1024 * 1024;
    private static readonly object Missing = new();

    private static bool Equivalent(object? a, object? b)
    {
        if (ReferenceEquals(a, b)) return true;
        if (a is null || b is null) return false;
        if (a is Dictionary<string, object> da && b is Dictionary<string, object> db)
            return da.Count == db.Count &&
                da.All(p => db.TryGetValue(p.Key, out var v) && Equivalent(p.Value, v));
        if (a is object[] aa && b is object[] ba)
            return aa.Length == ba.Length &&
                aa.Zip(ba, (x, y) => Equivalent(x, y)).All(x => x);
        if (a is string sa && b is string sb)
            return string.Equals(sa, sb, StringComparison.Ordinal);
        if (a is bool ab && b is bool bb) return ab == bb;
        if (IsNumber(a) && IsNumber(b))
            return Convert.ToDecimal(a, CultureInfo.InvariantCulture) ==
                   Convert.ToDecimal(b, CultureInfo.InvariantCulture);
        return a.Equals(b);
    }

    private static bool IsNumber(object value) =>
        value is int or long or double or decimal or float;

    private static object? At(Dictionary<string, object>? values, string key) =>
        values is not null && values.TryGetValue(key, out var val) ? val : Missing;

    private static bool TryCombine(object? baseline, object? left, object? right,
        string at, out object? merged, out string reason)
    {
        reason = "";
        merged = null;
        if (Equivalent(left, right)) { merged = left; return true; }
        if (Equivalent(left, baseline)) { merged = right; return true; }
        if (Equivalent(right, baseline)) { merged = left; return true; }

        var baseObj = baseline as Dictionary<string, object>;
        var leftObj = left as Dictionary<string, object>;
        var rightObj = right as Dictionary<string, object>;
        if (leftObj is not null && rightObj is not null &&
            (baseObj is not null || ReferenceEquals(baseline, Missing)))
        {
            var result = new Dictionary<string, object>(StringComparer.Ordinal);
            foreach (var key in (baseObj?.Keys ?? Enumerable.Empty<string>())
                     .Concat(leftObj.Keys).Concat(rightObj.Keys)
                     .Distinct(StringComparer.Ordinal)
                     .OrderBy(x => x, StringComparer.Ordinal))
            {
                if (!TryCombine(At(baseObj, key), At(leftObj, key),
                        At(rightObj, key), at + "." + key, out var v, out reason))
                    return false;
                if (!ReferenceEquals(v, Missing)) result.Add(key, v!);
            }
            merged = result;
            return true;
        }

        reason = "Both mods change " + at + " differently";
        return false;
    }

    private static Dictionary<string, object> ParseKeyValue(string content)
    {
        var result = new Dictionary<string, object>(StringComparer.OrdinalIgnoreCase);
        var lineNumber = 0;
        foreach (var line in content.Replace("\r\n", "\n").Split('\n'))
        {
            lineNumber++;
            if (string.IsNullOrWhiteSpace(line)) continue;
            var at = line.IndexOf('=');
            if (at < 1 || at == line.Length - 1)
                throw new FormatException("Unsupported config syntax on line " + lineNumber);
            var key = line.Substring(0, at).Trim();
            if (key.Length == 0 || key[0] == '#' || key[0] == ';'
                || key.Contains('[') || key.Contains(']') || key.Any(char.IsWhiteSpace)
                || result.ContainsKey(key))
                throw new FormatException("Unsupported or duplicate setting on line " + lineNumber);
            result.Add(key, line.Substring(at + 1).Trim());
        }
        return result;
    }

    private static string SerializeKeyValue(Dictionary<string, object> settings) =>
        string.Join("\n", settings.OrderBy(x => x.Key, StringComparer.OrdinalIgnoreCase)
            .Select(x => x.Key + "=" + (string)x.Value)) + "\n";

    internal static bool TryMerge(string resourcePath, byte[] original,
        byte[] left, byte[] right, out byte[] result, out string reason)
    {
        result = Array.Empty<byte>();
        reason = "";
        if (original.Length > MaxTextBytes || left.Length > MaxTextBytes
            || right.Length > MaxTextBytes)
        {
            reason = "Config exceeds the safe automatic-merge size limit";
            return false;
        }

        var extension = Path.GetExtension(resourcePath);
        var json = extension.Equals(".json", StringComparison.OrdinalIgnoreCase);
        var keyValue = extension.Equals(".cfg", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".ini", StringComparison.OrdinalIgnoreCase);
        if (!json && !keyValue)
        {
            reason = "Resource format is not supported for safe merging";
            return false;
        }

        try
        {
            var utf8 = new UTF8Encoding(false, true);
            var plain = utf8.GetString(original);
            var a = utf8.GetString(left);
            var b = utf8.GetString(right);
            Dictionary<string, object> baseObj;
            Dictionary<string, object> leftObj;
            Dictionary<string, object> rightObj;
            if (json)
            {
                var serializer = new JavaScriptSerializer
                {
                    MaxJsonLength = MaxTextBytes,
                    RecursionLimit = 32
                };
                baseObj = serializer.DeserializeObject(plain) as Dictionary<string, object>
                    ?? throw new FormatException("Original JSON must be an object");
                leftObj = serializer.DeserializeObject(a) as Dictionary<string, object>
                    ?? throw new FormatException("First mod JSON must be an object");
                rightObj = serializer.DeserializeObject(b) as Dictionary<string, object>
                    ?? throw new FormatException("Second mod JSON must be an object");
                if (!TryCombine(baseObj, leftObj, rightObj, "$", out var merged,
                        out reason)) return false;
                result = utf8.GetBytes(serializer.Serialize(merged));
            }
            else
            {
                baseObj = ParseKeyValue(plain);
                leftObj = ParseKeyValue(a);
                rightObj = ParseKeyValue(b);
                if (!TryCombine(baseObj, leftObj, rightObj, "$", out var merged,
                        out reason)) return false;
                result = utf8.GetBytes(SerializeKeyValue(
                    (Dictionary<string, object>)merged!));
            }
            return true;
        }
        catch (Exception error) when (error is FormatException ||
            error is ArgumentException || error is InvalidOperationException ||
            error is OverflowException)
        {
            reason = "Cannot safely merge this config: " + error.Message;
            return false;
        }
    }

    internal static int RunSelfTest()
    {
        try
        {
            static byte[] Text(string s) => Encoding.UTF8.GetBytes(s);
            static void Check(bool condition, string reason)
            {
                if (!condition) throw new InvalidOperationException(reason);
            }

            Check(TryMerge("player.json", Text("{\"health\":100,\"armor\":50}"),
                Text("{\"health\":200,\"armor\":50}"),
                Text("{\"health\":100,\"armor\":150}"), out var combo, out _),
                "Independent JSON values should merge");
            var result = new JavaScriptSerializer().DeserializeObject(
                Encoding.UTF8.GetString(combo)) as Dictionary<string, object>;
            Check(result is not null && Convert.ToInt32(result["health"]) == 200
                && Convert.ToInt32(result["armor"]) == 150,
                "JSON merge should preserve both independent changes");
            Check(!TryMerge("player.json", Text("{\"health\":100}"),
                Text("{\"health\":200}"), Text("{\"health\":300}"),
                out _, out var collision) && collision.Contains("$.health"),
                "Conflicting JSON fields should require a decision");
            Check(TryMerge("player.ini", Text("health=100\narmor=50\n"),
                Text("health=200\narmor=50\n"),
                Text("health=100\narmor=150\n"), out var ini, out _)
                && Encoding.UTF8.GetString(ini).Contains("armor=150"),
                "Independent key-value configs should merge");
            Check(!TryMerge("mod.decl", Text("a"), Text("b"), Text("c"),
                out _, out _), "Unsupported formats must not merge");
            Check(!TryMerge("player.cfg", Text("health=100"),
                Text("health=200"), Text("health=300"), out _, out _),
                "Same-key CFG conflicts must fail");
            Check(!TryMerge("player.ini", Text("health=100"),
                Text("health=200"), Text("[unsupported]"), out _, out _),
                "Complex INI syntax must fail closed");
            Console.WriteLine("KHARVOX basic config merging and conflict detection passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("KHARVOX config merge test failed: " + error);
            return 1;
        }
    }
}
