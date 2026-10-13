using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Original KHARVOX-drawn skull key, not a mismatched marine portrait.
// Decorative Easter egg: clicking briefly lights the red skull and key.
internal sealed class SkullKeyEasterEgg : Control
{
    private readonly System.Windows.Forms.Timer glowTimer = new() { Interval = 110 };
    private int glowTicks;
    private bool hovering;

    public SkullKeyEasterEgg()
    {
        AccessibleName = "DOOM-inspired skull key Easter egg";
        Size = new Size(88, 30);
        Cursor = Cursors.Hand;
        BackColor = Color.Black;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint
            | ControlStyles.OptimizedDoubleBuffer, true);
        glowTimer.Tick += (_, _) =>
        {
            if (glowTicks > 0) glowTicks--;
            if (glowTicks == 0) glowTimer.Stop();
            Invalidate();
        };
    }

    protected override void OnMouseEnter(EventArgs e)
    {
        base.OnMouseEnter(e);
        hovering = true;
        Invalidate();
    }

    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);
        hovering = false;
        Invalidate();
    }

    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);
        if (e.Button != MouseButtons.Left) return;
        glowTicks = 8;
        glowTimer.Start();
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        g.Clear(BackColor);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.Half;
        var active = glowTicks > 0;
        var red = active ? Color.FromArgb(255, 89, 67)
            : hovering ? Color.FromArgb(224, 59, 51)
            : Color.FromArgb(153, 42, 38);
        var highlight = active ? Color.FromArgb(255, 206, 163)
            : Color.FromArgb(193, 135, 103);
        using var redBrush = new SolidBrush(red);
        using var highlightBrush = new SolidBrush(highlight);
        using var shadowBrush = new SolidBrush(Color.FromArgb(30, 15, 17));
        using var outline = new Pen(Color.FromArgb(68, 54, 55), 1.2f);
        var cy = Height / 2f;

        // Metal key stem with three notches, subtly beveled.
        g.FillRectangle(redBrush, 39, cy - 4, 33, 8);
        g.FillRectangle(redBrush, 58, cy + 1, 6, 10);
        g.FillRectangle(redBrush, 69, cy + 1, 6, 7);
        g.FillRectangle(highlightBrush, 43, cy - 3, 26, 1.7f);
        g.FillRectangle(shadowBrush, 44, cy + 1, 24, 1.7f);

        // The bow of the key is an intentionally chunky pixel-era skull.
        using var skull = new GraphicsPath();
        skull.AddPolygon(new[]
        {
            new PointF(17, cy - 12), new PointF(27, cy - 13),
            new PointF(36, cy - 9), new PointF(41, cy - 2),
            new PointF(39, cy + 5), new PointF(34, cy + 10),
            new PointF(33, cy + 12), new PointF(19, cy + 12),
            new PointF(18, cy + 9), new PointF(13, cy + 6),
            new PointF(10, cy - 2), new PointF(13, cy - 9)
        });
        g.FillPath(redBrush, skull);
        g.DrawPath(outline, skull);
        g.FillEllipse(highlightBrush, 16, cy - 10, 12, 4);
        // Deep eye sockets, fractured nose and jaw teeth.
        g.FillPolygon(shadowBrush, new[]
        {
            new PointF(16, cy - 3), new PointF(25, cy - 4),
            new PointF(22, cy + 3), new PointF(18, cy + 3)
        });
        g.FillPolygon(shadowBrush, new[]
        {
            new PointF(29, cy - 4), new PointF(37, cy - 2),
            new PointF(33, cy + 3), new PointF(27, cy + 2)
        });
        g.FillPolygon(shadowBrush, new[]
        {
            new PointF(25, cy + 2), new PointF(29, cy + 2),
            new PointF(27, cy + 6)
        });
        for (var x = 22; x < 34; x += 4)
            g.FillRectangle(shadowBrush, x, cy + 8, 2, 4);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) glowTimer.Dispose();
        base.Dispose(disposing);
    }
}
