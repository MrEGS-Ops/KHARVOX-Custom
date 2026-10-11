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
        Size = new Size(134, 28);
        MinimumSize = new Size(96, 24);
        BackColor = Color.FromArgb(30, 30, 33);
        ForeColor = Color.FromArgb(201, 201, 204);
        TabStop = true;
        AccessibleName = "Glory Kill speed slider (1 native, 5 fastest)";
    }
    private Rectangle Track => new Rectangle(9, Height / 2 - 2,
        Math.Max(1, Width - 29), 4);
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
        using (var ticks = new Pen(Color.FromArgb(128, 128, 132), 1.2f))
            for (var n = 1; n <= 5; n++)
                g.DrawLine(ticks, Position(n), Height / 2 + 5,
                    Position(n), Height / 2 + 8);
        var center = Position(Value);
        using (var thumb = new SolidBrush(Color.FromArgb(208, 208, 210)))
            g.FillEllipse(thumb, center - 5, Height / 2 - 6, 11, 11);
        using (var pen = new Pen(Color.FromArgb(51, 51, 54), 1f))
            g.DrawEllipse(pen, center - 5, Height / 2 - 6, 11, 11);
        var number = Value.ToString();
        TextRenderer.DrawText(g, number, new Font(Font, FontStyle.Bold),
            new Rectangle(Width - 22, 0, 20, Height), ForeColor,
            TextFormatFlags.VerticalCenter | TextFormatFlags.HorizontalCenter
            | TextFormatFlags.NoPadding);
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