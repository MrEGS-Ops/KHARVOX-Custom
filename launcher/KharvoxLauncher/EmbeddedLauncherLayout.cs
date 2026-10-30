using System;
using System.Drawing;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Modified-launcher-only companion. The original Cactus MainForm.cs remains
// unchanged. Once this window is embedded in KHARVOX Mods, adapt its existing
// TableLayoutPanel to the available area instead of showing both scrollbars.
internal static class EmbeddedLauncherLayout
{
    private const int GwlStyle = -16;
    private const int WsChild = 0x40000000;

    [DllImport("user32.dll")]
    private static extern int GetWindowLong(IntPtr hwnd, int index);

    [DllImport("user32.dll")]
    private static extern IntPtr GetParent(IntPtr hwnd);

    private static readonly BindingFlags PrivateInstance =
        BindingFlags.Instance | BindingFlags.NonPublic;

    internal static float FitFactor(Size naturalSize, Size availableSize)
    {
        if (naturalSize.Width <= 0 || naturalSize.Height <= 0 ||
            availableSize.Width <= 0 || availableSize.Height <= 0)
            return 1.0f;
        return Math.Min(1.0f, Math.Min(
            (float)availableSize.Width / naturalSize.Width,
            (float)availableSize.Height / naturalSize.Height));
    }

    internal static bool SelfTest()
    {
        return Math.Abs(FitFactor(new Size(548, 904), new Size(606, 946)) - 1f) < .001f &&
            FitFactor(new Size(685, 1130), new Size(606, 946)) < .85f &&
            FitFactor(new Size(685, 1130), new Size(606, 946)) > .80f &&
            Math.Abs(FitFactor(new Size(0, 0), new Size(606, 946)) - 1f) < .001f;
    }

    internal static void Watch(MainForm launcher)
    {
        var poll = new Timer { Interval = 200 };
        poll.Tick += (_, _) =>
        {
            if (launcher.IsDisposed || !launcher.IsHandleCreated) return;
            // Do nothing to normal standalone windows; only the child HWND
            // embedded in the standalone Mods application's right-hand pane.
            if ((GetWindowLong(launcher.Handle, GwlStyle) & WsChild) == 0 ||
                GetParent(launcher.Handle) == IntPtr.Zero) return;
            poll.Stop();
            Apply(launcher);
        };
        launcher.Shown += (_, _) => poll.Start();
        launcher.FormClosed += (_, _) =>
        {
            poll.Stop();
            poll.Dispose();
        };
    }

    private static void Apply(MainForm launcher)
    {
        var viewport = typeof(MainForm).GetField("viewport", PrivateInstance)
            ?.GetValue(launcher) as Panel;
        var content = typeof(MainForm).GetField("launcherContent", PrivateInstance)
            ?.GetValue(launcher) as TableLayoutPanel;
        if (viewport == null || content == null) return;

        viewport.SuspendLayout();
        content.SuspendLayout();
        try
        {
            viewport.AutoScrollPosition = Point.Empty;
            var available = viewport.ClientSize;
            // Native WinForms DPI scaling may enlarge the original 548x904
            // content even though the reparented form itself is 606x946.
            var natural = new Size(
                Math.Max(content.MinimumSize.Width, content.PreferredSize.Width),
                Math.Max(content.MinimumSize.Height, content.PreferredSize.Height));
            var factor = FitFactor(natural, available);
            if (factor < .999f)
                content.Scale(new SizeF(factor, factor));
            // With a verified fit factor, let the original Cactus layout
            // fill the embedded viewport rather than extend and scroll.
            content.MinimumSize = Size.Empty;
            content.AutoSize = false;
            content.Dock = DockStyle.Fill;
            content.Location = Point.Empty;
            viewport.AutoScrollMinSize = Size.Empty;
            viewport.AutoScrollMargin = Size.Empty;
            viewport.AutoScroll = false;
        }
        finally
        {
            content.ResumeLayout(true);
            viewport.ResumeLayout(true);
        }
    }
}
