using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace KharvoxLauncher;

// All easter-egg work is launcher-only. Nothing here hooks or launches DOOM 2016.
// An optional user-created, outline-processed E1M1 GIF can supply genuine footage;
// the built-in wireframe vignette is an asset-free fallback.
internal sealed class DoomEasterEgg : IDisposable
{
    private readonly System.Windows.Forms.Timer timer = new() { Interval = 80 };
    private readonly Stopwatch clock = new();
    private readonly List<Control> surfaces = new();
    private Image? animation;
    private int frameCount;
    private bool disposed;
    private const int DurationMs = 26000;
    internal bool Playing { get; private set; }

    internal DoomEasterEgg()
    {
        var paths = new[]
        {
            Path.Combine(AppContext.BaseDirectory, "assets", "easteregg", "e1m1_outline.gif"),
            Path.Combine(AppContext.BaseDirectory, "e1m1_outline.gif")
        };
        foreach (var path in paths)
        {
            if (!File.Exists(path)) continue;
            try
            {
                // Keep the GIF decoder alive for the lifetime of the window.
                // The file is optional, user-supplied content, never bundled.
                animation = Image.FromFile(path);
                frameCount = animation.GetFrameCount(FrameDimension.Time);
                break;
            }
            catch (Exception)
            {
                animation?.Dispose();
                animation = null;
            }
        }
        timer.Tick += (_, _) =>
        {
            if (disposed || !Playing) return;
            if (clock.ElapsedMilliseconds >= DurationMs)
            {
                Stop();
                return;
            }
            Redraw();
        };
    }

    internal void Register(Control control)
    {
        surfaces.Add(control);
        control.Disposed += (_, _) => surfaces.Remove(control);
    }

    internal void Start()
    {
        if (disposed) return;
        clock.Restart();
        Playing = true;
        timer.Start();
        Redraw();
    }

    internal void Stop()
    {
        if (!Playing) return;
        Playing = false;
        timer.Stop();
        clock.Reset();
        Redraw();
    }

    private void Redraw()
    {
        foreach (var control in surfaces.ToArray())
            if (!control.IsDisposed && control.Visible) control.Invalidate();
    }

    internal void Paint(Graphics graphics, Rectangle area)
    {
        if (!Playing || area.Width < 10 || area.Height < 10) return;
        var t = clock.Elapsed.TotalSeconds;
        var fade = Math.Min(1.0, Math.Min(t / 0.75, (26 - t) / 2.5));
        if (fade <= 0) return;

        var state = graphics.Save();
        graphics.SetClip(area);
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        if (animation is not null && frameCount > 0)
            PaintFootage(graphics, area, t, (float)fade);
        else
            PaintFallback(graphics, area, t, (float)fade);
        graphics.Restore(state);
    }

    private void PaintFootage(Graphics g, Rectangle area, double t, float fade)
    {
        if (animation is null) return;
        try
        {
            // GIF white edges become translucent highlights; its black
            // background becomes fully transparent. A 1px dark offset creates
            // the "pressed into charcoal" / embossed watermark effect.
            var index = (int)(t * 12) % frameCount;
            animation.SelectActiveFrame(FrameDimension.Time, index);
            var src = new Rectangle(0, 0, animation.Width, animation.Height);
            using var shadowAttributes = new ImageAttributes();
            shadowAttributes.SetColorMatrix(OutlineMatrix(
                Color.FromArgb(19, 19, 22), 0.5f * fade));
            using var highlightAttributes = new ImageAttributes();
            highlightAttributes.SetColorMatrix(OutlineMatrix(
                Color.FromArgb(41, 41, 45), 0.65f * fade));
            g.DrawImage(animation, new Rectangle(area.X + 1, area.Y + 1,
                    area.Width, area.Height), src.X, src.Y, src.Width, src.Height,
                GraphicsUnit.Pixel, shadowAttributes);
            g.DrawImage(animation, area, src.X, src.Y, src.Width, src.Height,
                GraphicsUnit.Pixel, highlightAttributes);
        }
        catch (Exception)
        {
            // A malformed/unsupported animation must not take down the launcher.
            PaintFallback(g, area, t, fade);
        }
    }

    private static ColorMatrix OutlineMatrix(Color colour, float edgeOpacity)
    {
        // FFmpeg's grayscale GIF feeds the edge intensity in red. Using red
        // as output alpha makes empty/background pixels fully transparent.
        return new ColorMatrix(new[]
        {
            new[] { 0f, 0f, 0f, edgeOpacity, 0f },
            new[] { 0f, 0f, 0f, 0f, 0f },
            new[] { 0f, 0f, 0f, 0f, 0f },
            new[] { 0f, 0f, 0f, 0f, 0f },
            new[] { colour.R / 255f, colour.G / 255f,
                colour.B / 255f, 0f, 1f }
        });
    }

    private static void PaintFallback(Graphics g, Rectangle area, double t, float fade)
    {
        // Asset-free placeholder: a tiny moving corridor rendered as embossed
        // edges. This intentionally is NOT described as authentic E1M1.
        var w = area.Width;
        var h = area.Height;
        var cx = area.Left + w / 2f;
        var cy = area.Top + h * .48f;
        var bob = (float)Math.Sin(t * 6) * 3f;
        var drift = (float)Math.Sin(t * .46) * w * .17f;
        cx += drift;
        var stroke = (int)(7 * fade);
        using var highlight = new Pen(Color.FromArgb(30 + stroke, 30 + stroke, 33 + stroke), 1f);
        using var shadow = new Pen(Color.FromArgb(30 - (int)(4 * fade),
            30 - (int)(4 * fade), 33 - (int)(4 * fade)), 1f);
        var near = new RectangleF(area.Left - w * .1f, cy - h * .5f, w * 1.2f, h);
        var far = new RectangleF(cx - w * .14f, cy - h * .16f + bob, w * .28f, h * .32f);
        for (var i = 0; i < 5; i++)
        {
            var f = (float)((i + t * .62) % 5) / 5f;
            var easing = f * f;
            var left = far.Left + (near.Left - far.Left) * easing;
            var right = far.Right + (near.Right - far.Right) * easing;
            var top = far.Top + (near.Top - far.Top) * easing;
            var bottom = far.Bottom + (near.Bottom - far.Bottom) * easing;
            g.DrawRectangle(highlight, left, top, right - left, bottom - top);
            g.DrawLine(shadow, left + 1, top + 1, right + 1, top + 1);
        }
        g.DrawLine(highlight, far.Left, far.Top, near.Left, near.Top);
        g.DrawLine(highlight, far.Right, far.Top, near.Right, near.Top);
        g.DrawLine(highlight, far.Left, far.Bottom, near.Left, near.Bottom);
        g.DrawLine(highlight, far.Right, far.Bottom, near.Right, near.Bottom);
        // An approaching enemy silhouette and bobbing first-person weapon.
        var enemyX = cx + (float)Math.Sin(t * 1.2) * w * .075f;
        var enemyY = cy + (float)Math.Sin(t * 3) * 2f;
        var size = 7f + (float)(t % 7) * 1.8f;
        g.DrawLine(highlight, enemyX - size, enemyY - size, enemyX, enemyY - size * 1.6f);
        g.DrawLine(highlight, enemyX, enemyY - size * 1.6f, enemyX + size, enemyY - size);
        g.DrawEllipse(highlight, enemyX - size * .55f, enemyY - size,
            size * 1.1f, size * 1.3f);
        g.DrawLine(highlight, enemyX, enemyY + size * .3f, enemyX, enemyY + size * 2);
        g.DrawLine(highlight, enemyX, enemyY + size * .8f, enemyX - size, enemyY + size * 1.2f);
        g.DrawLine(highlight, enemyX, enemyY + size * .8f, enemyX + size, enemyY + size * 1.2f);
        var gunTop = area.Bottom - h * .27f + (float)Math.Sin(t * 8) * 2;
        g.DrawLines(highlight, new[]
        {
            new PointF(cx - w * .13f, area.Bottom + 4),
            new PointF(cx - w * .08f, gunTop + 18),
            new PointF(cx - w * .045f, gunTop),
            new PointF(cx + w * .045f, gunTop),
            new PointF(cx + w * .08f, gunTop + 18),
            new PointF(cx + w * .13f, area.Bottom + 4)
        });
    }

    public void Dispose()
    {
        if (disposed) return;
        Stop();
        disposed = true;
        timer.Dispose();
        animation?.Dispose();
    }
}

internal sealed class DoomEggSurface : TableLayoutPanel
{
    private readonly DoomEasterEgg egg;

    internal DoomEggSurface(DoomEasterEgg egg)
    {
        this.egg = egg;
        DoubleBuffered = true;
        egg.Register(this);
    }

    protected override void OnPaintBackground(PaintEventArgs e)
    {
        base.OnPaintBackground(e);
        egg.Paint(e.Graphics, ClientRectangle);
    }
}

internal sealed class DoomFaceControl : Control
{
    private readonly DoomEasterEgg egg;
    private readonly System.Windows.Forms.Timer idle = new() { Interval = 300 };
    private DateTime lastClick = DateTime.MinValue;
    private DateTime grinUntil = DateTime.MinValue;
    private int clicks;
    private int phase;
    private readonly Bitmap? forward, left, right, grin;

    internal DoomFaceControl(DoomEasterEgg egg)
    {
        this.egg = egg;
        AccessibleName = "Doomguy";
        AccessibleDescription = "Decorative developer easter egg";
        SetStyle(ControlStyles.AllPaintingInWmPaint |
            ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        Cursor = Cursors.Default; // Do not advertise that the portrait is clickable.
        BackColor = Color.Black;
        var faces = DoomFaceWad.TryLoad();
        if (faces is not null)
        {
            faces.TryGetValue("STFST00", out forward);
            faces.TryGetValue("STFTL00", out left);
            faces.TryGetValue("STFTR00", out right);
            faces.TryGetValue("STFEVL0", out grin);
        }
        idle.Tick += (_, _) =>
        {
            if (!Visible || FindForm()?.Visible != true) return;
            phase = (phase + 1) % 12;
            Invalidate();
        };
        VisibleChanged += (_, _) =>
        {
            if (Visible) idle.Start();
            else idle.Stop();
        };
        idle.Start();
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button != MouseButtons.Left) return;
        var now = DateTime.UtcNow;
        if ((now - lastClick).TotalSeconds > 4) clicks = 0;
        lastClick = now;
        grinUntil = now.AddMilliseconds(400);
        if (egg.Playing)
        {
            egg.Stop();
            clicks = 0;
        }
        else if (++clicks == 3)
        {
            clicks = 0;
            egg.Start();
        }
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.Clear(BackColor);
        var isGrin = DateTime.UtcNow < grinUntil;
        var bitmap = isGrin ? grin : phase < 4 ? forward : phase < 8 ? left : right;
        bitmap ??= forward;
        if (bitmap is not null)
        {
            e.Graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
            e.Graphics.PixelOffsetMode = PixelOffsetMode.Half;
            var width = Math.Min(ClientSize.Width - 2, 28);
            var height = Math.Min(ClientSize.Height - 2, 28);
            e.Graphics.DrawImage(bitmap, new Rectangle((Width - width) / 2,
                (Height - height) / 2, width, height));
            return;
        }
        // No IWAD supplied: a small original pixel-face stand-in, not id's art.
        using var hair = new SolidBrush(Color.FromArgb(58, 49, 43));
        using var skin = new SolidBrush(Color.FromArgb(132, 105, 83));
        using var dark = new SolidBrush(Color.FromArgb(31, 29, 29));
        using var eye = new SolidBrush(Color.FromArgb(185, 177, 154));
        var scale = Math.Min(Width / 24f, Height / 24f);
        void Pixel(Brush b, int x, int y, int width, int height) =>
            e.Graphics.FillRectangle(b, (Width - 24 * scale) / 2 + x * scale,
                (Height - 24 * scale) / 2 + y * scale, width * scale, height * scale);
        Pixel(hair, 4, 2, 16, 17);
        Pixel(skin, 5, 6, 14, 14);
        Pixel(hair, 6, 3, 12, 5);
        var look = phase < 4 ? 0 : phase < 8 ? -1 : 1;
        Pixel(dark, 7, 10, 4, 3);
        Pixel(dark, 14, 10, 4, 3);
        Pixel(eye, 8 + look, 11, 2, 1);
        Pixel(eye, 15 + look, 11, 2, 1);
        Pixel(dark, 11, 14, 3, 2);
        Pixel(dark, 7, 18, 11, isGrin ? 4 : 2);
        if (isGrin)
            for (var x = 8; x <= 16; x += 2) Pixel(eye, x, 19, 1, 2);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            idle.Dispose();
            foreach (var bmp in new[] { forward, left, right, grin })
                bmp?.Dispose();
        }
        base.Dispose(disposing);
    }
}

internal static class DoomFaceWad
{
    // Reads only the status-face sprites and palette from the user's own IWAD.
    // The project does not include or redistribute original DOOM graphics.
    internal static Dictionary<string, Bitmap>? TryLoad()
    {
        var dir = AppContext.BaseDirectory;
        var candidates = new[]
        {
            Path.Combine(dir, "assets", "easteregg", "DOOM1.WAD"),
            Path.Combine(dir, "assets", "easteregg", "DOOM.WAD"),
            Path.Combine(dir, "DOOM1.WAD"),
            Path.Combine(dir, "DOOM.WAD")
        };
        foreach (var file in candidates)
        {
            if (!File.Exists(file)) continue;
            try
            {
                using var stream = File.OpenRead(file);
                using var reader = new BinaryReader(stream);
                if (stream.Length < 12) continue;
                var header = new string(reader.ReadChars(4));
                if (header != "IWAD" && header != "PWAD") continue;
                var count = reader.ReadInt32();
                var offset = reader.ReadInt32();
                if (count < 1 || count > 10000 ||
                    offset < 12 || (long)offset + count * 16L > stream.Length) continue;
                var lumps = new Dictionary<string, (int start, int size)>(StringComparer.OrdinalIgnoreCase);
                stream.Position = offset;
                for (var i = 0; i < count; i++)
                {
                    var start = reader.ReadInt32();
                    var size = reader.ReadInt32();
                    var name = System.Text.Encoding.ASCII.GetString(reader.ReadBytes(8)).TrimEnd('\0');
                    if (start >= 0 && size >= 0 && (long)start + size <= stream.Length)
                        lumps[name] = (start, size);
                }
                if (!lumps.TryGetValue("PLAYPAL", out var palette) || palette.size < 768) continue;
                stream.Position = palette.start;
                var colors = reader.ReadBytes(768);
                var result = new Dictionary<string, Bitmap>(StringComparer.OrdinalIgnoreCase);
                foreach (var name in new[] { "STFST00", "STFTL00", "STFTR00", "STFEVL0" })
                {
                    if (!lumps.TryGetValue(name, out var lump) || lump.size < 12) continue;
                    try
                    {
                        var bmp = DecodePatch(reader, stream, lump.start, lump.size, colors);
                        if (bmp is not null) result[name] = bmp;
                    }
                    catch (Exception) { /* Corrupt optional sprite: retain the rest. */ }
                }
                if (result.Count > 0) return result;
            }
            catch (Exception) { /* Optional artwork must never break startup. */ }
        }
        return null;
    }

    private static Bitmap? DecodePatch(BinaryReader reader, Stream stream, int start, int size, byte[] palette)
    {
        stream.Position = start;
        var width = reader.ReadUInt16();
        var height = reader.ReadUInt16();
        reader.ReadInt16(); reader.ReadInt16(); // sprite offsets
        if (width == 0 || width > 256 || height == 0 || height > 256 ||
            8 + width * 4 > size) return null;
        var offsets = new int[width];
        for (var x = 0; x < width; x++) offsets[x] = reader.ReadInt32();
        var bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        try
        {
            for (var x = 0; x < width; x++)
            {
                if (offsets[x] < 8 || offsets[x] >= size) continue;
                stream.Position = start + offsets[x];
                for (var posts = 0; posts < height + 32; posts++)
                {
                    if (stream.Position >= start + size) break;
                    var y = reader.ReadByte();
                    if (y == 255) break;
                    var length = reader.ReadByte();
                    reader.ReadByte(); // padding
                    if (stream.Position + length + 1 > start + size) break;
                    for (var p = 0; p < length; p++)
                    {
                        var index = reader.ReadByte() * 3;
                        if (y + p < height)
                            bmp.SetPixel(x, y + p, Color.FromArgb(palette[index],
                                palette[index + 1], palette[index + 2]));
                    }
                    reader.ReadByte(); // padding
                }
            }
            return bmp;
        }
        catch
        {
            bmp.Dispose();
            return null;
        }
    }
}
