# Capture one top-level window of a process to a PNG, including its frame.
# Usage: powershell -ExecutionPolicy Bypass -File shot.ps1 -Out main.png [-Process vlc | -ProcessId N] [-Title "regex"]
# Prefer -ProcessId when another VLC (e.g. the installed one) may be running.
# Without -Title it takes the process's main window; with -Title, the first visible
# top-level window of that process whose title matches (dialogs, the playlist, etc.).
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Process = 'vlc',
    [int]$ProcessId = 0,
    [string]$Title = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
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
[Win]::GetWindowRect($hwnd, [ref]$r) | Out-Null
$w = $r.R - $r.L; $h = $r.B - $r.T
if ($w -le 0 -or $h -le 0) { throw "window has no size (minimized?)" }
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
# 2 = PW_RENDERFULLCONTENT (Win 8.1+): also captures DirectX/video content.
$ok = [Win]::PrintWindow($hwnd, $dc, 2)
$g.ReleaseHdc($dc); $g.Dispose()
if (-not $ok) { $bmp.Dispose(); throw "PrintWindow failed" }
$bmp.Save([IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"$Out ${w}x$h '$([Win]::Text($hwnd))'"
