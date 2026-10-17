using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Five exact discrete positions, fully owner-painted so the VR MODS row
// never inherits the bright white OS trackbar or combo-box chrome.
internal sealed class FlatGloryKillSlider : Control
{
    private int current = 1;
    public event EventHandler? ValueChanged;
    public int Value
    {
        get => current;
        set
        {
            var limited = Math.Max(1, Math.Min(5, value));
            if (current == limited) return;
            current = limited;
            Invalidate();
            ValueChanged?.Invoke(this, EventArgs.Empty);
        }
    }
    public FlatGloryKillSlider()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint
            | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer
            | ControlStyles.Selectable, true);
        Size = new Size(132, 28);
        MinimumSize = new Size(108, 24);
        BackColor = Color.FromArgb(30, 30, 33);
        ForeColor = Color.FromArgb(201, 201, 204);
        TabStop = true;
        AccessibleName = "Cinematic Kill Speed (1 native, 5 full speed)";
    }
    private Rectangle Track => new Rectangle(12, Height / 2 - 2,
        Math.Max(1, Width - 24), 4);
    private int Position(int number)
    {
        var t = Track;
        return t.Left + (number - 1) * t.Width / 4;
    }
    protected override void OnEnabledChanged(EventArgs e)
    {
        base.OnEnabledChanged(e);
        // This is an owner-painted control: Windows does not automatically
        // grey its track/thumb when Enabled becomes false.
        Invalidate();
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(BackColor);
        var track = Track;
        var enabled = Enabled;
        using (var background = new SolidBrush(enabled
            ? Color.FromArgb(76, 76, 80) : Color.FromArgb(48, 48, 50)))
            g.FillRectangle(background, track);
        using (var filled = new SolidBrush(enabled
            ? Color.FromArgb(157, 157, 160) : Color.FromArgb(65, 65, 68)))
            g.FillRectangle(filled, new Rectangle(track.X, track.Y,
                Math.Max(1, Position(Value) - track.Left), track.Height));
        // The disabled thumb remains in place to preserve the saved selection,
        // but is deliberately dimmer than even the inactive enabled stops.
        using var inactive = new SolidBrush(enabled
            ? Color.FromArgb(123, 123, 127) : Color.FromArgb(69, 69, 73));
        using var active = new SolidBrush(enabled
            ? Color.FromArgb(224, 224, 226) : Color.FromArgb(96, 96, 99));
        for (var n = 1; n <= 5; n++)
        {
            var cx = Position(n);
            var radius = n == Value ? 6 : 3;
            g.FillEllipse(n == Value ? active : inactive, cx - radius,
                Height / 2 - radius, 2 * radius, 2 * radius);
        }
        if (Focused && ShowFocusCues)
            ControlPaint.DrawFocusRectangle(g, ClientRectangle);
    }
    private void SelectPosition(int x)
    {
        var t = Track;
        var value = 1 + (int)Math.Round(Math.Max(0.0, Math.Min(1.0,
            (x - t.Left) / (double)Math.Max(1, t.Width))) * 4);
        Value = value;
    }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (!Enabled || e.Button != MouseButtons.Left) return;
        Focus();
        Capture = true;
        SelectPosition(e.X);
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (Enabled && Capture && e.Button == MouseButtons.Left) SelectPosition(e.X);
    }
    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        Capture = false;
    }
    protected override bool IsInputKey(Keys keyData)
    {
        var key = keyData & Keys.KeyCode;
        return key is Keys.Left or Keys.Right or Keys.Home or Keys.End
            || base.IsInputKey(keyData);
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        if (!Enabled) return;
        if (e.KeyCode == Keys.Left) Value--;
        else if (e.KeyCode == Keys.Right) Value++;
        else if (e.KeyCode == Keys.Home) Value = 1;
        else if (e.KeyCode == Keys.End) Value = 5;
        else return;
        e.Handled = true;
    }
}