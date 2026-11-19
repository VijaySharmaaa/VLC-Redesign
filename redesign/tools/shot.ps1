# Capture one top-level window of a process to a PNG, including its frame.
# Usage: powershell -ExecutionPolicy Bypass -File shot.ps1 -Out main.png [-Process vlc | -ProcessId N] [-Title "regex"]
# Prefer -ProcessId when another VLC (e.g. the installed one) may be running.
# Without -Title it takes the process's main window; with -Title, the first visible
# top-level window of that process whose title matches (dialogs, the playlist, etc.).
# -Overlays also shows the process's own top-level windows above it (the redesign's
# frameless overlays: paused badge, toasts, drawers...). Their areas come from the
# screen, and only while no other app's window is in front there (else it fails).
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Process = 'vlc',
    [int]$ProcessId = 0,
    [string]$Title = '',
    [switch]$Overlays
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class Win {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    public static List<IntPtr> TopLevel(uint pid) {
        var list = new List<IntPtr>();
        EnumWindows((h, p) => { uint w; GetWindowThreadProcessId(h, out w);
            if (w == pid && IsWindowVisible(h)) list.Add(h); return true; }, IntPtr.Zero);
        return list;
    }
    public static string Text(IntPtr h) { var s = new System.Text.StringBuilder(512); GetWindowText(h, s, 512); return s.ToString(); }
    [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int attr, out int v, int size);
    static bool Shown(IntPtr h) {    // visible and not cloaked (e.g. a suspended store app)
        int c; return IsWindowVisible(h) && !(DwmGetWindowAttribute(h, 14, out c, 4) == 0 && c != 0);
    }
    public static Bitmap Grab(IntPtr h, out RECT r) {
        GetWindowRect(h, out r);
        int w = r.R - r.L, ht = r.B - r.T;
        if (w <= 0 || ht <= 0) return null;
        var bmp = new Bitmap(w, ht);
        using (var g = Graphics.FromImage(bmp)) {
            IntPtr dc = g.GetHdc();
            // 2 = PW_RENDERFULLCONTENT (Win 8.1+): also captures DirectX/video content.
            bool ok = PrintWindow(h, dc, 2);
            g.ReleaseHdc(dc);
            if (!ok) { bmp.Dispose(); throw new Exception("PrintWindow failed"); }
        }
        return bmp;
    }
    // Throws if a shown window of another process is above `main` and overlaps r.
    static void Mine(IntPtr main, uint pid, Rectangle r) {
        bool foreign = false;
        EnumWindows((h, p) => {    // top of the z-order first
            if (h == main) return false;
            uint w; GetWindowThreadProcessId(h, out w); RECT o;
            foreign = w != pid && Shown(h) && GetWindowRect(h, out o)
                && r.IntersectsWith(Rectangle.FromLTRB(o.L, o.T, o.R, o.B));
            return !foreign; }, IntPtr.Zero);
        if (foreign) throw new Exception("another app's window covers the test VLC: not capturing");
    }
    // The main window with the process's own windows above it (overlays, toasts,
    // drawers). PrintWindow loses the alpha of translucent (layered) windows, so
    // each overlay's area is copied from the screen, but only while no window of
    // another process is above the main window there: checked before and after
    // the copy. The main window's rounded corners stay PrintWindow pixels.
    public static Bitmap Composite(IntPtr main, uint pid) {
        RECT mr; var bmp = Grab(main, out mr);
        if (bmp == null) return null;
        var m = Rectangle.FromLTRB(mr.L, mr.T, mr.R, mr.B);
        const int corner = 8;      // Windows 11 window corner radius
        using (var g = Graphics.FromImage(bmp)) {
            g.ExcludeClip(new Rectangle(0, 0, corner, corner));
            g.ExcludeClip(new Rectangle(m.Width - corner, 0, corner, corner));
            g.ExcludeClip(new Rectangle(0, m.Height - corner, corner, corner));
            g.ExcludeClip(new Rectangle(m.Width - corner, m.Height - corner, corner, corner));
            foreach (var h in TopLevel(pid)) {
                if (h == main) break;              // only windows above the main window
                RECT o; GetWindowRect(h, out o);
                var r = Rectangle.Intersect(m, Rectangle.FromLTRB(o.L, o.T, o.R, o.B));
                if (r.IsEmpty) continue;
                using (var part = new Bitmap(r.Width, r.Height)) {
                    Mine(main, pid, r);
                    using (var pg = Graphics.FromImage(part)) pg.CopyFromScreen(r.Location, Point.Empty, r.Size);
                    Mine(main, pid, r);    // nothing came in front during the copy
                    g.DrawImageUnscaled(part, r.X - m.X, r.Y - m.Y);
                }
            }
        }
        return bmp;
    }
}
'@
[Win]::SetProcessDPIAware() | Out-Null

$procs = if ($ProcessId) { @(Get-Process -Id $ProcessId) } else { @(Get-Process -Name $Process) }
if ($procs.Count -eq 0) { throw "no process '$Process'" }
$hwnd = [IntPtr]::Zero
foreach ($p in $procs) {
    if ($Title -eq '') { if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break } }
    else {
        foreach ($h in [Win]::TopLevel([uint32]$p.Id)) {
            if ([Win]::Text($h) -match $Title) { $hwnd = $h; break }
        }
        if ($hwnd -ne [IntPtr]::Zero) { break }
    }
}
if ($hwnd -eq [IntPtr]::Zero) { throw "no matching window (process '$Process', title '$Title')" }

$r = New-Object Win+RECT
$owner = [uint32]0; [Win]::GetWindowThreadProcessId($hwnd, [ref]$owner) | Out-Null
$bmp = if ($Overlays) { [Win]::Composite($hwnd, $owner) } else { [Win]::Grab($hwnd, [ref]$r) }
if ($bmp -eq $null) { throw "window has no size (minimized?)" }
$w = $bmp.Width; $h = $bmp.Height
$bmp.Save([IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"$Out ${w}x$h '$([Win]::Text($hwnd))'"
