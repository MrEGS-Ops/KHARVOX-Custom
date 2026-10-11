using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace KharvoxLauncher;

// Original miniature 1990s-style marine portrait. No copied game sprites.
// Click once or twice to grin; click a third time to launch the micro arena.
internal sealed class DoomFaceEasterEgg : Control
{
    private readonly Timer timer = new Timer { Interval = 165 };
    private int ticks, clicks, gaze, grinTicks;
    private DateTime lastClick;
    public DoomFaceEasterEgg()
    {
        AccessibleName = "Doom marine head Easter egg. Three clicks starts a micro arena.";
        Cursor = Cursors.Hand;
        BackColor = Color.Black;
        Size = new Size(54, 34);
        SetStyle(ControlStyles.AllPaintingInWmPaint
            | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        timer.Tick += (_, _) =>
        {
            ticks++;
            if (grinTicks > 0) grinTicks--;
            if (ticks % 10 == 0) gaze = ((ticks / 10) % 4) switch
            {
                0 => -1, 1 => 0, 2 => 1, _ => 0
            };
            Invalidate();
        };
        timer.Start();
    }
    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);
        if (e.Button != MouseButtons.Left) return;
        if ((DateTime.UtcNow - lastClick).TotalSeconds > 12) clicks = 0;
        lastClick = DateTime.UtcNow;
        clicks++;
        grinTicks = 5;
        Invalidate();
        if (clicks < 3) return;
        clicks = 0;
        using var arena = new DoomMiniArena();
        arena.ShowDialog(FindForm());
    }
    protected override void Dispose(bool disposing)
    {
        if (disposing) { timer.Stop(); timer.Dispose(); }
        base.Dispose(disposing);
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        g.Clear(BackColor);
        g.SmoothingMode = SmoothingMode.None;
        var scale = Math.Min((Width - 3) / 44f, (Height - 2) / 32f);
        g.TranslateTransform((Width - scale * 44) / 2, (Height - scale * 32) / 2);
        g.ScaleTransform(scale, scale);
        using var dark = new SolidBrush(Color.FromArgb(26, 25, 20));
        using var outline = new SolidBrush(Color.FromArgb(8, 10, 9));
        using var helmet = new SolidBrush(Color.FromArgb(85, 89, 65));
        using var metal = new SolidBrush(Color.FromArgb(122, 130, 93));
        using var shadow = new SolidBrush(Color.FromArgb(107, 74, 56));
        using var skin = new SolidBrush(Color.FromArgb(190, 135, 92));
        using var light = new SolidBrush(Color.FromArgb(223, 169, 117));
        using var eye = new SolidBrush(Color.FromArgb(225, 211, 170));
        using var pupil = new SolidBrush(Color.FromArgb(21, 29, 26));
        using var lip = new SolidBrush(Color.FromArgb(51, 31, 27));
        using var teeth = new SolidBrush(Color.FromArgb(228, 213, 171));
        // Chunky helmet silhouette and unmistakable shadowed face visor.
        g.FillPolygon(outline, new[] {
            new Point(5,10), new Point(8,3), new Point(15,0),
            new Point(30,0), new Point(38,4), new Point(41,14),
            new Point(38,26), new Point(31,32), new Point(12,32),
            new Point(5,25) });
        g.FillPolygon(helmet, new[] {
            new Point(8,11),new Point(12,4),new Point(18,2),
            new Point(30,2),new Point(36,6),new Point(38,14),
            new Point(32,13),new Point(29,8),new Point(16,8),
            new Point(13,13),new Point(7,16) });
        g.FillRectangle(metal, 12, 5, 20, 3);
        g.FillRectangle(dark, 7, 13, 6, 12);
        g.FillRectangle(dark, 33, 13, 6, 12);
        g.FillPolygon(shadow, new[] {
            new Point(12,11), new Point(34,11), new Point(36,22),
            new Point(30,30), new Point(15,30), new Point(9,22) });
        g.FillPolygon(skin, new[] {
            new Point(13,12), new Point(32,12), new Point(35,21),
            new Point(29,29), new Point(16,29), new Point(10,21) });
        g.FillRectangle(light, 14, 13, 17, 5);
        // Thick, angry dark brow: pupils swivel while the mouth rests.
        g.FillPolygon(outline, new[] {
            new Point(12,13),new Point(21,15),new Point(21,17),
            new Point(12,17) });
        g.FillPolygon(outline, new[] {
            new Point(24,15),new Point(33,13),new Point(33,17),
            new Point(24,17) });
        g.FillRectangle(eye, 14, 17, 7, 4);
        g.FillRectangle(eye, 24, 17, 7, 4);
        g.FillRectangle(pupil, 16 + gaze, 17, 3, 4);
        g.FillRectangle(pupil, 26 + gaze, 17, 3, 4);
        g.FillRectangle(shadow, 21, 20, 4, 4);
        g.FillRectangle(outline, 20, 23, 6, 2);
        g.FillRectangle(shadow, 13, 22, 3, 3);
        g.FillRectangle(shadow, 31, 22, 3, 3);
        if (grinTicks > 0)
        {
            g.FillRectangle(lip, 14, 25, 18, 5);
            g.FillRectangle(teeth, 16, 26, 14, 3);
            for (int x = 19; x < 30; x += 4)
                g.FillRectangle(shadow, x, 26, 1, 3);
        }
        else
        {
            g.FillRectangle(lip, 17, 26, 12, 3);
            g.FillRectangle(teeth, 18, 26, 10, 1);
        }
        g.FillRectangle(outline, 12, 29, 6, 2);
        g.FillRectangle(outline, 27, 29, 6, 2);
    }
}

// A tiny self-contained 20-second shooting gallery, not a copyrighted DOOM
// executable or a replacement for the actual game.
internal sealed class DoomMiniArena : Form
{
    private readonly Timer tick = new Timer { Interval = 35 };
    private int frame, hits;
    private float player = 0.5f;
    private readonly float[] enemies = { 0.17f, 0.36f, 0.62f, 0.84f };
    private readonly bool[] alive = { true, true, true, true };
    private bool left, right;
    public DoomMiniArena()
    {
        Text = "KHARVOX // MINI ARENA";
        ClientSize = new Size(500, 330);
        BackColor = Color.FromArgb(18, 18, 20);
        ForeColor = Color.Gainsboro;
        Font = new Font("Consolas", 10f);
        DoubleBuffered = true;
        StartPosition = FormStartPosition.CenterParent;
        KeyPreview = true;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        tick.Tick += (_, _) =>
        {
            frame++;
            if (left) player = Math.Max(0.08f, player - 0.019f);
            if (right) player = Math.Min(0.92f, player + 0.019f);
            if (frame >= 600) tick.Stop();
            Invalidate();
        };
        tick.Start();
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.A || e.KeyCode == Keys.Left) left = true;
        if (e.KeyCode == Keys.D || e.KeyCode == Keys.Right) right = true;
        if (e.KeyCode == Keys.Space) Fire();
        if (e.KeyCode == Keys.Escape) Close();
        if (e.KeyCode == Keys.R) Restart();
    }
    protected override void OnKeyUp(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.A || e.KeyCode == Keys.Left) left = false;
        if (e.KeyCode == Keys.D || e.KeyCode == Keys.Right) right = false;
    }
    private void Restart()
    {
        frame = hits = 0;
        player = 0.5f;
        for (var i = 0; i < alive.Length; i++) alive[i] = true;
        tick.Start();
        Invalidate();
    }
    private void Fire()
    {
        if (!tick.Enabled) return;
        int closest = -1;
        float distance = 0.105f;
        for (var i = 0; i < enemies.Length; i++)
        {
            var d = Math.Abs(player - enemies[i]);
            if (!alive[i] || d >= distance) continue;
            distance = d;
            closest = i;
        }
        if (closest >= 0) { alive[closest] = false; hits++; }
        if (hits == enemies.Length) tick.Stop();
        Invalidate();
    }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button == MouseButtons.Left) Fire();
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var g = e.Graphics;
        g.Clear(BackColor);
        var width = ClientSize.Width;
        var height = ClientSize.Height;
        using var wall = new SolidBrush(Color.FromArgb(75, 78, 75));
        using var floor = new SolidBrush(Color.FromArgb(48, 39, 33));
        using var monster = new SolidBrush(Color.FromArgb(157, 50, 40));
        using var eyes = new SolidBrush(Color.FromArgb(234, 203, 64));
        using var weapon = new SolidBrush(Color.FromArgb(100, 100, 98));
        g.FillRectangle(wall, 0, 38, width, 160);
        g.FillPolygon(floor, new[] { new Point(0, 198),
            new Point(width, 198), new Point(width, height), new Point(0, height) });
        for (int i = 0; i < enemies.Length; i++)
        {
            if (!alive[i]) continue;
            var px = (int)(enemies[i] * width);
            var py = 124 + (int)(Math.Sin(frame / 11.0 + i) * 6);
            g.FillEllipse(monster, px - 16, py - 17, 32, 36);
            g.FillRectangle(eyes, px - 9, py - 5, 6, 4);
            g.FillRectangle(eyes, px + 3, py - 5, 6, 4);
        }
        var gun = (int)(player * width);
        g.FillPolygon(weapon, new[] { new Point(gun - 30, height),
            new Point(gun - 19, height - 48), new Point(gun - 10, height - 60),
            new Point(gun + 10, height - 60), new Point(gun + 19, height - 48),
            new Point(gun + 30, height) });
        using var target = new Pen(Color.FromArgb(222, 207, 185), 1);
        g.DrawLine(target, gun - 11, 101, gun + 11, 101);
        g.DrawLine(target, gun, 90, gun, 112);
        TextRenderer.DrawText(g, "KHARVOX // DOOM-INSPIRED MINI ARENA",
            Font, new Rectangle(10, 8, width - 12, 23), Color.Gainsboro);
        var message = hits == enemies.Length ? "CLEARED!  R to replay · ESC to exit"
            : !tick.Enabled ? "TIME UP  ·  R to retry · ESC to exit"
            : "A/D move   SPACE or click shoot   ESC exit   KILLS " + hits + "/4";
        TextRenderer.DrawText(g, message, Font, new Rectangle(10, 201, width - 20, 25),
            Color.WhiteSmoke);
    }
    protected override void Dispose(bool disposing)
    {
        if (disposing) { tick.Stop(); tick.Dispose(); }
        base.Dispose(disposing);
    }
}
