using System.Web.Script.Serialization;

namespace KharvoxLauncher;

internal sealed class CustomModSettings
{
    internal const int CurrentSchema = 1;

    public int Schema { get; set; } = CurrentSchema;
    public bool DisableHud { get; set; }
    public bool DisableWeaponWheel { get; set; }
    public bool GaussChargeSlowMovement { get; set; }
    public bool BackOfHandHud { get; set; }
    public bool HandFocusedRs { get; set; }
    public bool DirectionalDash { get; set; }
    public bool BehindHeadWeaponWheel { get; set; }
    public bool BehindHeadWheelHandSelection { get; set; } = false;
    public bool PhysicalCrouch { get; set; }
    public bool RevengeDemon { get; set; }
    public bool DynamicShoulderHolster { get; set; }
    public bool PhysicalGrenadeThrow { get; set; }
    public bool MotionGloryKillSpeed { get; set; }
    public bool PhysicalChainsawGestures { get; set; }

    public bool SupervisorRequired => RevengeDemon || PhysicalChainsawGestures;
}

internal static class CustomModSettingsStore
{
    private static readonly JavaScriptSerializer Serializer = new();

    internal static string DefaultPath =>
        Path.Combine(AppContext.BaseDirectory, "kharvox-custom-mods.json");

    internal static CustomModSettings Load(string? path = null)
    {
        path ??= DefaultPath;
        try
        {
            if (!File.Exists(path)) return new CustomModSettings();
            var value = Serializer.Deserialize<CustomModSettings>(File.ReadAllText(path));
            if (value is null || value.Schema != CustomModSettings.CurrentSchema)
                throw new InvalidDataException("Invalid custom-mod settings schema.");
            return value;
        }
        catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException
            || ex is InvalidDataException || ex is ArgumentException
            || ex is InvalidOperationException)
        {
            throw new InvalidDataException(
                "KHARVOX VR mod settings are unreadable. The existing file has not been changed."
                + Environment.NewLine + "Repair or restore: " + path, ex);
        }
    }

    internal static void Save(CustomModSettings value, string? path = null)
    {
        path ??= DefaultPath;
        value.Schema = CustomModSettings.CurrentSchema;
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
        var temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temp, Serializer.Serialize(value));
            if (File.Exists(path)) File.Replace(temp, path, null);
            else File.Move(temp, path);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }

    internal static int RunSelfTest()
    {
        var root = Path.Combine(Path.GetTempPath(),
            "KHARVOX-Custom-Settings-Test-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            var path = Path.Combine(root, "custom-mods.json");
            Save(new CustomModSettings { PhysicalCrouch = true }, path);
            if (!Load(path).PhysicalCrouch)
                throw new InvalidDataException("Valid custom mod option was lost.");
            File.WriteAllText(path, "{BROKEN");
            var refused = false;
            try { Load(path); }
            catch (InvalidDataException) { refused = true; }
            if (!refused || File.ReadAllText(path) != "{BROKEN")
                throw new InvalidDataException(
                    "Damaged custom mod settings must fail without resetting the file.");
            Console.WriteLine("KHARVOX custom VR mod settings safety tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("Custom VR mod config self-test failed: " + error);
            return 1;
        }
        finally
        {
            try { Directory.Delete(root, true); } catch { }
        }
    }
}
