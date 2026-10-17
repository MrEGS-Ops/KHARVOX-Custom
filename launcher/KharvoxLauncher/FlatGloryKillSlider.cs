using System;
using System.Drawing;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Discrete − [number] + control. Retain Value / ValueChanged compatibility
// with existing mod config, load/save and cross-mod grey-out code.
internal sealed class FlatGloryKillSlider : Control
{
    private int current = 1;
    public int Minimum { get; set; } = 1;
    public int Maximum { get; set; } = 5;
    public event EventHandler? ValueChanged;
    public int Value
    {
        get => current;
        set
        {
            var limited = Math.Max(Minimum, Math.Min(Maximum, value));
            if (current == limited) return;
            current = limited;
            Invalidate();
            ValueChanged?.Invoke(this, EventArgs.Empty);
        }
    }
    public FlatGloryKillSlider()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint
            | ControlStyles.OptimizedDoubleBuffer | ControlStyles.Selectable, true);
        Size = new Size(160, 29);
        MinimumSize = new Size(114, 25);
        BackColor = Color.FromArgb(30, 30, 33);
        ForeColor = Color.WhiteSmoke;
        AccessibleName = "Cinematic Kill Speed minus number plus";
        TabStop = true;
    }
    private Rectangle Minus => new(0, 0, 45, Height - 1);
    private Rectangle Plus => new(Width - 45, 0, 45, Height - 1);
    private Rectangle Number => new(45, 0, Math.Max(1, Width - 90), Height - 1);
    internal static int ChangedValueForClick(int x, int width, int value,
        int minimum = 1, int maximum = 5)
    {
        var bounded = Math.Max(minimum, Math.Min(maximum, value));
        if (x < 0 || x >= width) return bounded;
        if (x < 45) return Math.Max(minimum, bounded - 1);
        if (x >= width - 45) return Math.Min(maximum, bounded + 1);
        return bounded;
    }
    protected override void OnEnabledChanged(EventArgs e)
    {
        base.OnEnabledChanged(e);
        Invalidate();
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        g.Clear(BackColor);
        using var button = new SolidBrush(Enabled
            ? Color.FromArgb(65, 65, 70) : Color.FromArgb(39, 39, 42));
        using var center = new SolidBrush(Enabled
            ? Color.FromArgb(26, 26, 29) : Color.FromArgb(35, 35, 38));
        using var text = new SolidBrush(Enabled
            ? Color.WhiteSmoke : Color.FromArgb(96, 96, 99));
        using var border = new Pen(Enabled
            ? Color.FromArgb(125, 125, 130) : Color.FromArgb(58, 58, 61));
        g.FillRectangle(button, Minus);
        g.FillRectangle(button, Plus);
        g.FillRectangle(center, Number);
        g.DrawRectangle(border, 0, 0, Width - 1, Height - 1);
        g.DrawLine(border, 45, 0, 45, Height - 1);
        g.DrawLine(border, Width - 45, 0, Width - 45, Height - 1);
        using var font = new Font(Font.FontFamily, 11f, FontStyle.Bold);
        using var aligned = new StringFormat { Alignment = StringAlignment.Center,
            LineAlignment = StringAlignment.Center };
        g.DrawString("−", font, text, Minus, aligned);
        g.DrawString(Value.ToString(), font, text, Number, aligned);
        g.DrawString("+", font, text, Plus, aligned);
        if (Focused && ShowFocusCues)
            ControlPaint.DrawFocusRectangle(g, ClientRectangle);
    }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (!Enabled || e.Button != MouseButtons.Left) return;
        Focus();
        Value = ChangedValueForClick(e.X, Width, Value, Minimum, Maximum);
    }
    protected override bool IsInputKey(Keys data)
    {
        var key = data & Keys.KeyCode;
        return key is Keys.Left or Keys.Right or Keys.Home or Keys.End
            || base.IsInputKey(data);
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        if (!Enabled) return;
        switch (e.KeyCode)
        {
            case Keys.Left: Value--; break;
            case Keys.Right: Value++; break;
            case Keys.Home: Value = Minimum; break;
            case Keys.End: Value = Maximum; break;
            default: return;
        }
        e.Handled = true;
    }
}
