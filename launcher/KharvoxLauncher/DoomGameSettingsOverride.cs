using System.Text;
using System.Text.RegularExpressions;

namespace KharvoxLauncher;

// Edits only plain-text DOOMConfig.cfg game-option entries. Never modify
// campaign saves or the original KHARVOX installation.
internal static class DoomGameSettingsOverride
{
    private static readonly Regex TutorialLine = new Regex(
        @"(?m)^(?<prefix>[ \t]*(?:(?:set|seta)[ \t]+)?g_setting_tutorials[ \t]+)""?[01]""?(?<tail>[ \t]*(?://[^\r\n]*)?\r?)$",
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    internal static string UpdatedContent(string original)
    {
        var newline = original.Contains("\r\n") ? "\r\n" : "\n";
        var changed = TutorialLine.Replace(original,
            m => m.Groups["prefix"].Value + "\"0\"" + m.Groups["tail"].Value);
        if (!TutorialLine.IsMatch(original))
            changed = changed.TrimEnd('\r', '\n') + newline + "g_setting_tutorials \"0\"" + newline;

        return changed;
    }

    internal static bool TryApply(string configPath, out string result)
    {
        result = "not attempted";
        try
        {
            var info = new FileInfo(configPath);
            if (!info.Exists || info.Length > 2_000_000)
            {
                result = "config absent or unexpectedly large";
                return false;
            }
            var original = File.ReadAllText(configPath);
            var updated = UpdatedContent(original);
            if (original == updated)
            {
                result = "settings lines already correct (runtime not yet verified)";
                return true;
            }

            // Keep a separate, immutable backup from the first settings edit.
            var backup = configPath + ".ripshift-settings-original.bak";
            if (!File.Exists(backup)) File.Copy(configPath, backup);
            var temp = configPath + ".ripshift-settings.tmp";
            try
            {
                File.WriteAllText(temp, updated, new UTF8Encoding(false));
                File.Replace(temp, configPath, null);
            }
            finally
            {
                if (File.Exists(temp)) File.Delete(temp);
            }
            result = "DOOMConfig.cfg updated (runtime not yet verified)";
            return true;
        }
        catch (Exception e)
        {
            result = "settings not changed: " + e.GetType().Name;
            return false;
        }
    }
}
