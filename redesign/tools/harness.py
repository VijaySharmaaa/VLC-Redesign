"""Drive vlc.exe for screenshots and checks (Windows Python, needs pywinauto).

    py redesign/tools/harness.py baseline --vlc <package dir>\\vlc.exe --out <dir>
    py redesign/tools/harness.py names    --vlc <package dir>\\vlc.exe

VLC runs in its own portable mode: each session starts with an empty
`portable` folder next to vlc.exe, so the user's real %APPDATA%\\vlc settings
are never read or written. (Setting APPDATA does NOT work: VLC finds its
config dir with SHGetFolderPath, src/win32/dirs.c.) Player state is set through VLC's rc interface; keys go
through pywinauto (UIA), which also shows which controls have accessible names.
"""
import argparse
import msvcrt
import os
import shutil
import socket
import subprocess
import sys
import time

import re

import win32con
import win32gui
import win32process
from pywinauto import Application

HERE = os.path.dirname(os.path.abspath(__file__))
MEDIA = os.path.join(HERE, '..', '.local', 'media')
PACKAGES = os.path.abspath(os.path.join(HERE, '..', '..', '..'))  # D:\Projects\VLC: vlc-pkg*
# One lock for every process on this machine: TEMP differs per session, so the
# lock lives next to the packages it protects.
LOCK_PATH = os.path.join(PACKAGES, '.vlc-harness.lock')


def _free_port():
    """A port nobody listens on, so parallel sessions never talk to each other's VLC."""
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def _desktop_lock(timeout=1800):
    """Hold one machine-wide lock while a session drives the desktop: parallel
    sessions would steal each other's focus and keys. The OS drops it if we die."""
    f = open(LOCK_PATH, 'a+')
    deadline = time.time() + timeout
    while True:
        try:
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
            return f
        except OSError:
            if time.time() > deadline:
                raise RuntimeError('another harness session held the desktop for %d s' % timeout)
            time.sleep(1)


class Vlc:
    def __init__(self, exe, extra=()):
        exe = os.path.abspath(exe)
        # Only the project's own test packages: a portable folder must never be
        # created next to the user's installed VLC.
        if not os.path.normcase(exe).startswith(os.path.normcase(PACKAGES)):
            raise RuntimeError('refusing to run %s: not under %s' % (exe, PACKAGES))
        self.lock = _desktop_lock()
        self.profile = os.path.join(os.path.dirname(exe), 'portable')
        shutil.rmtree(self.profile, ignore_errors=True)     # every session starts clean
        os.makedirs(self.profile)
        self.port = _free_port()
        self.proc = subprocess.Popen(
            [exe, '--no-qt-privacy-ask', '--no-qt-updates-notif',
             '--extraintf=rc', '--rc-quiet', '--rc-host=127.0.0.1:%d' % self.port,
             *extra])
        try:
            self.sock = self._connect()
            self.app = Application(backend='uia').connect(process=self.proc.pid, timeout=20)
            self.main = self.app.top_window()
            self.main.wait('visible ready', timeout=20)
        except Exception:                 # e.g. a broken plugin: don't leave VLC or the lock behind
            self.proc.kill()
            self.lock.close()
            raise

    def _connect(self):
        deadline = time.time() + 20
        while time.time() < deadline:
            try:
                s = socket.create_connection(('127.0.0.1', self.port), timeout=2)
                s.settimeout(2)
                return s
            except OSError:
                time.sleep(0.3)
        raise RuntimeError('rc interface did not open on port %d' % self.port)

    def rc(self, cmd, wait=0.5):
        """Send one rc command and return whatever VLC printed back."""
        self.sock.sendall((cmd + '\n').encode())
        time.sleep(wait)
        out = b''
        try:
            while True:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                out += chunk
                if len(chunk) < 65536:
                    break
        except socket.timeout:
            pass
        out = out.decode(errors='replace').strip()
        # rc says "returned 0" even for commands it refused while paused (oldrc.c).
        if "Type 'pause' to continue.\r\n%s: returned" % cmd.split()[0] in out:
            raise RuntimeError('rc %r refused while paused: %r' % (cmd, out))
        return out

    def windows(self):
        """Visible top-level windows of this VLC (Qt's UIA tree nests dialogs, so use Win32)."""
        out = []
        def cb(h, _):
            if win32gui.IsWindowVisible(h) and win32process.GetWindowThreadProcessId(h)[1] == self.proc.pid:
                out.append(h)
        win32gui.EnumWindows(cb, None)
        return out

    def keys(self, k):
        # Never type into another app: keys go to whatever window is in front.
        # Focus returns asynchronously after a dialog closes, so ask until VLC has it.
        def ours():
            self.main.set_focus()
            fg = win32gui.GetForegroundWindow()
            return fg and win32process.GetWindowThreadProcessId(fg)[1] == self.proc.pid
        self.wait_for(ours, 'VLC to become the foreground window before sending ' + k, timeout=5)
        self.main.type_keys(k)

    def wait_for(self, cond, what, timeout=8):
        deadline = time.time() + timeout
        while time.time() < deadline:
            r = cond()
            if r:
                time.sleep(0.5)           # let the window finish painting
                return r
            time.sleep(0.2)
        raise RuntimeError('timed out waiting for ' + what)

    def dialog(self, keys, title):
        """Press keys and wait for a top-level window whose title matches."""
        find = lambda: next((h for h in self.windows()
                             if re.search(title, win32gui.GetWindowText(h))), None)
        self.keys(keys)
        return self.wait_for(find, 'window ' + title)

    def close(self, hwnd):
        win32gui.PostMessage(hwnd, win32con.WM_CLOSE, 0, 0)
        self.wait_for(lambda: hwnd not in self.windows(), 'window to close')

    def main_size(self):
        r = self.main.rectangle()
        return r.width(), r.height()

    def toggle_playlist(self):
        # Stock VLC resizes the window for the playlist; the redesign opens a
        # panel inside it. Either way the window's pixels change.
        before = self.main.capture_as_image().tobytes()
        self.keys('^l')                   # Ctrl+L is hard-coded in 3.0.x
        self.wait_for(lambda: self.main.capture_as_image().tobytes() != before,
                      'playlist to toggle')
        time.sleep(0.3)                   # the panel slides in

    def shot(self, out, title=''):
        cmd = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
               os.path.join(HERE, 'shot.ps1'), '-Out', out, '-ProcessId', str(self.proc.pid)]
        if title:
            cmd += ['-Title', title]
        print(subprocess.run(cmd, check=True, capture_output=True, text=True).stdout.strip())

    def quit(self):
        try:
            self.rc('quit', wait=0.2)
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.kill()
        finally:
            self.lock.close()             # closing the handle releases the desktop lock


def media(name):
    return os.path.abspath(os.path.join(MEDIA, name))


def baseline(args):
    os.makedirs(args.out, exist_ok=True)
    p = lambda n: os.path.join(args.out, n)
    v = Vlc(args.vlc)
    try:
        v.shot(p('01-main-idle.png'))
        idle = v.main_size()
        v.rc('add ' + media('test-720p.mp4'), wait=2)
        v.wait_for(lambda: v.main_size() != idle, 'the video to resize the window')
        v.rc('seek 10', wait=1)
        v.rc('pause', wait=1)
        v.shot(p('02-main-video-paused.png'))
        v.toggle_playlist()
        v.shot(p('03-playlist.png'))
        v.toggle_playlist()
        h = v.dialog('^e', 'Adjustments and Effects')
        v.shot(p('04-effects.png'), 'Adjustments and Effects')
        v.close(h)
        h = v.dialog('^+o', 'Open Media')       # Ctrl+O is the native file picker
        v.shot(p('05-open.png'), 'Open Media')
        v.close(h)
        h = v.dialog('^p', 'Preferences')
        v.shot(p('06-prefs.png'), 'Preferences')
        v.close(h)
        v.rc('pause')                     # resume: rc refuses playlist commands while paused
        v.rc('clear')
        v.rc('add ' + media('test-tone.mp3'), wait=2)
        v.wait_for(lambda: 'Test tone' in v.main.window_text(), 'the audio file to load')
        v.shot(p('07-main-audio.png'))
    finally:
        v.quit()


def names(args):
    """List every control in the main window with its accessible name."""
    v = Vlc(args.vlc)
    try:
        unnamed = 0
        for c in v.main.descendants():
            info = c.element_info
            if info.control_type in ('Button', 'Slider', 'MenuItem', 'Edit', 'ComboBox'):
                name = info.name or ''
                unnamed += not name
                print('%-10s %r' % (info.control_type, name))
        print('controls without a name:', unnamed)
    finally:
        v.quit()


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['baseline', 'names'])
    ap.add_argument('--vlc', required=True)
    ap.add_argument('--out', default=os.path.join(HERE, '..', '.local', 'baseline'))
    a = ap.parse_args()
    {'baseline': baseline, 'names': names}[a.cmd](a)
    sys.exit(0)
