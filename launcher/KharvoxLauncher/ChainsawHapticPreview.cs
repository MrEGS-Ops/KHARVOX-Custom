using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace KharvoxLauncher;

// One-shot, bounded IPC to the existing DOOM OpenXR session.
// Never opens a second VR session or changes the saved game configuration.
internal static class ChainsawHapticPreview
{
    internal const string Header = "KHARVOX_HAPTIC_PREVIEW_V1";
    internal static string RequestPath => RuntimeStorage.LogPath("haptic-lab-preview.request");
    internal static string AckPath => RuntimeStorage.LogPath("haptic-lab-preview.ack");

    internal static string Serialize(ChainsawHapticProfile profile,
        string id, int hand, bool cutting, long createdFileTimeUtc)
    {
        if (hand < 0 || hand > 2) throw new ArgumentOutOfRangeException(nameof(hand));
        if (id.Length != 32 || !System.Linq.Enumerable.All(id, c =>
            (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            throw new ArgumentException("Invalid haptic preview ID.", nameof(id));
        profile.Validate();
        return string.Join(" ", new[]
        {
            Header, createdFileTimeUtc.ToString(CultureInfo.InvariantCulture),
            id, hand.ToString(CultureInfo.InvariantCulture),
            cutting ? "1" : "0",
            profile.IdlePercent.ToString(CultureInfo.InvariantCulture),
            profile.CuttingPercent.ToString(CultureInfo.InvariantCulture),
            profile.PulseRate.ToString(CultureInfo.InvariantCulture),
            profile.PulseDurationMs.ToString(CultureInfo.InvariantCulture),
            profile.RoughnessPercent.ToString(CultureInfo.InvariantCulture),
            profile.MotionSensitivityPercent.ToString(CultureInfo.InvariantCulture)
        }) + Environment.NewLine;
    }

    internal static string Send(ChainsawHapticProfile profile, int hand, bool cutting)
    {
        var id = Guid.NewGuid().ToString("N");
        var command = Serialize(profile, id, hand, cutting, DateTime.UtcNow.ToFileTimeUtc());
        var path = RequestPath;
        var temporary = path + "." + id + ".tmp";
        try
        {
            try { File.Delete(AckPath); } catch (IOException) { }
            File.WriteAllText(temporary, command, Encoding.ASCII);
            if (File.Exists(path)) File.Replace(temporary, path, null);
            else File.Move(temporary, path);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
        return id;
    }

    internal static bool Acknowledged(string id, int hand)
    {
        try
        {
            var response = File.ReadAllText(AckPath).Trim().Split(' ');
            return response.Length == 3 && response[0] == "KHARVOX_HAPTIC_ACK_V1"
                && response[1] == id
                && response[2] == hand.ToString(CultureInfo.InvariantCulture);
        }
        catch (IOException) { return false; }
        catch (UnauthorizedAccessException) { return false; }
    }

    internal static void RunSelfTest(string directory)
    {
        var p = ChainsawHapticProfile.Preset("Jackhammer");
        var id = "abcdef0123456789abcdef0123456789";
        var line = Serialize(p, id, 0, true, 134203393000000000L);
        var fields = line.Trim().Split(' ');
        if (fields.Length != 11 || fields[0] != Header || fields[2] != id
            || fields[3] != "0" || fields[4] != "1"
            || fields[7] != p.PulseRate.ToString(CultureInfo.InvariantCulture))
            throw new InvalidDataException("Haptic preview request format changed.");
        if (!Serialize(p, id, 2, false, 134203393000000000L).Contains(" " + id + " 2 0 "))
            throw new InvalidDataException("Haptic Lab Stop command format changed.");
    }
}
