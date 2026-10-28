using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace KharvoxMods;

// The original ModDropWindow uses these two ordering helpers. Keep its
// order JSON BESIDE KharvoxMods.exe; never inside the ZIP import folder.
internal static class PortableDoomUserMods
{
    private static string OrderFile =>
        Path.Combine(AppState.DataDirectory, "mods-order.json");

    internal static string[] LoadUserOrder()
    {
        if (!File.Exists(OrderFile)) return Array.Empty<string>();
        try
        {
            return new JavaScriptSerializer().Deserialize<string[]>(
                File.ReadAllText(OrderFile)) ?? Array.Empty<string>();
        }
        catch (Exception ex) when (ex is IOException ||
            ex is UnauthorizedAccessException || ex is ArgumentException ||
            ex is InvalidOperationException)
        {
            return Array.Empty<string>();
        }
    }

    internal static void SaveUserOrder(IEnumerable<string> filenames)
    {
        var filtered = filenames.Where(name => !string.IsNullOrWhiteSpace(name) &&
                string.Equals(Path.GetFileName(name), name, StringComparison.Ordinal))
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        var tmp = OrderFile + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(tmp, new JavaScriptSerializer().Serialize(filtered));
            if (File.Exists(OrderFile)) File.Replace(tmp, OrderFile, null);
            else File.Move(tmp, OrderFile);
        }
        finally { if (File.Exists(tmp)) File.Delete(tmp); }
    }

    internal static string[] OrderUserPaths(IEnumerable<string> paths)
    {
        var positions = LoadUserOrder().Select((name, index) => (name, index))
            .GroupBy(item => item.name, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(group => group.Key, group => group.First().index,
                StringComparer.OrdinalIgnoreCase);
        return paths.OrderBy(path => positions.TryGetValue(Path.GetFileName(path),
                out var position) ? position : int.MaxValue)
            .ThenBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase).ToArray();
    }
}
