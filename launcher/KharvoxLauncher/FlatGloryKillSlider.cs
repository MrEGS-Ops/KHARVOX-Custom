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
        Size = new Size(174, 28);
        MinimumSize = new Size(150, 24);
        BackColor = Color.FromArgb(30, 30, 33);
        ForeColor = Color.FromArgb(201, 201, 204);
        TabStop = true;
        AccessibleName = "Glory Kill speed slider (1 native, 5 fastest)";
    }
    private Rectangle Track => new Rectangle(12, Height / 2 - 2,
        Math.Max(1, Width - 24), 4);
    private int Position(int number)
    {
        var t = Track;
        return t.Left + (number - 1) * t.Width / 4;
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(BackColor);
        var track = Track;
        using (var background = new SolidBrush(Color.FromArgb(76, 76, 80)))
            g.FillRectangle(background, track);
        using (var filled = new SolidBrush(Color.FromArgb(157, 157, 160)))
            g.FillRectangle(filled, new Rectangle(track.X, track.Y,
                Math.Max(1, Position(Value) - track.Left), track.Height));
        // Five always-visible physical stops with a larger active thumb.
        using var inactive = new SolidBrush(Color.FromArgb(123, 123, 127));
        using var active = new SolidBrush(Color.FromArgb(224, 224, 226));
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
        if (e.Button != MouseButtons.Left) return;
        Focus();
        Capture = true;
        SelectPosition(e.X);
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (Capture && e.Button == MouseButtons.Left) SelectPosition(e.X);
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
        if (e.KeyCode == Keys.Left) Value--;
        else if (e.KeyCode == Keys.Right) Value++;
        else if (e.KeyCode == Keys.Home) Value = 1;
        else if (e.KeyCode == Keys.End) Value = 5;
        else return;
        e.Handled = true;
    }
}