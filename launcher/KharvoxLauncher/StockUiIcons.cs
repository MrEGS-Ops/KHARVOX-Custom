using System;
using System.Drawing;
using System.Reflection;

namespace KharvoxLauncher;

// Static PNG resources derived from Microsoft's Fluent UI System Icons (MIT).
// Standard image assets, not hand-drawn Windows Forms Paint callbacks.
internal static class StockUiIcons
{
    internal static readonly Image Refresh = Load("Kharvox.Icons.Refresh");
    internal static readonly Image RefreshHover = Load("Kharvox.Icons.RefreshHover");
    internal static readonly Image Question = Load("Kharvox.Icons.Question");
    internal static readonly Image Check = Load("Kharvox.Icons.Check");
    internal static readonly Image Cross = Load("Kharvox.Icons.Cross");

    private static Image Load(string name)
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name)
            ?? throw new InvalidOperationException("Missing stock UI icon: " + name);
        using var original = Image.FromStream(stream);
        return new Bitmap(original);
    }
}
