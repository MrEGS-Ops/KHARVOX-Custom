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
    public bool BehindHeadWheelHandSelection { get; set; } = true;
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
            return value ?? new CustomModSettings();
        }
        catch
        {
            return new CustomModSettings();
        }
    }

    internal static void Save(CustomModSettings value, string? path = null)
    {
        path ??= DefaultPath;
        value.Schema = CustomModSettings.CurrentSchema;
        File.WriteAllText(path, Serializer.Serialize(value));
    }
}
