using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Uses the original Freedoom project's marine face art, BSD-3-Clause;
// packaged alongside assets/freedoom/COPYING.adoc.
// Clicking changes expression ONLY. The former mini-game is removed.
internal sealed class DoomFaceEasterEgg : Control
{
    private readonly System.Windows.Forms.Timer animationTimer =
        new System.Windows.Forms.Timer { Interval = 180 };
    private readonly Bitmap?[] faceFrames = new Bitmap?[4];
    private int ticks;
    private int grinUntil;
    public DoomFaceEasterEgg()
    {
        AccessibleName = "Freedoom marine face (click to grin)";
        Size = new Size(72, 48);
        Cursor = Cursors.Hand;
        BackColor = Color.Black;
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint
            | ControlStyles.OptimizedDoubleBuffer, true);

        var folder = Path.Combine(AppContext.BaseDirectory, "assets", "freedoom");
        var names = new[] { "stfst00.png", "stftl00.png", "stftr00.png", "stfevl0.png" };
        for (var i = 0; i < names.Length; i++)
        {
            var filename = Path.Combine(folder, names[i]);
            try
            {
                if (!File.Exists(filename)) continue;
                using var file = Image.FromFile(filename);
                faceFrames[i] = new Bitmap(file);
            }
            catch (Exception ex) when (ex is IOException
                || ex is UnauthorizedAccessException || ex is ArgumentException)
            {
                // Missing asset is harmless; it won't impact game launching.
            }
        }
        animationTimer.Tick += (_, _) =>
        {
            ticks++;
            if (grinUntil > 0) grinUntil--;
            Invalidate();
        };
        animationTimer.Start();
    }
    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);
        if (e.Button != MouseButtons.Left) return;
        grinUntil = 6;
        Invalidate();
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.Clear(BackColor);
        var sequence = (ticks / 10) % 4;
        var selected = grinUntil > 0 ? 3 : sequence == 1 ? 1 : sequence == 3 ? 2 : 0;
        var art = faceFrames[selected] ?? faceFrames[0];
        if (art is null) return;
        e.Graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
        e.Graphics.PixelOffsetMode = PixelOffsetMode.Half;
        e.Graphics.DrawImage(art, new Rectangle(
            (Width - 72) / 2, (Height - 48) / 2, 72, 48));
    }
    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            animationTimer.Stop();
            animationTimer.Dispose();
            foreach (var frame in faceFrames) frame?.Dispose();
        }
        base.Dispose(disposing);
    }
}
