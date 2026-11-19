#!/bin/bash
# Fast loop: rebuild only the Qt interface plugin and drop it into the runnable package.
# Run from WSL:  bash /mnt/d/Projects/VLC/vlc/redesign/tools/qt-rebuild.sh
# Needs a full build in $BUILD first (extras/package/win32/build.sh -a x86_64 -p).
set -euo pipefail
SRC=${SRC:-/mnt/d/Projects/VLC/vlc}          # where the code is edited (Windows disk)
BUILD=${BUILD:-$HOME/vlc}                    # build copy on WSL's own disk (much faster)
PKG=${PKG:-/mnt/d/Projects/VLC/vlc-pkg}      # runnable package on Windows
JOBS=${JOBS:-$(nproc)}                       # lower it when several builds run at once
IMAGE=registry.videolan.org/vlc-debian-win64-3.0:20260930101942

# -a keeps mtimes, so make only rebuilds what really changed.
rsync -a --delete "$SRC/modules/gui/qt/" "$BUILD/modules/gui/qt/"

docker run --rm -e JOBS="$JOBS" -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$BUILD":/src -w /src "$IMAGE" bash -c '
  make -C win64/modules -j"$JOBS" libqt_plugin.la &&
  x86_64-w64-mingw32-strip -o /tmp/libqt_plugin.dll win64/modules/.libs/libqt_plugin.dll &&
  cp /tmp/libqt_plugin.dll win64/libqt_plugin.stripped.dll'

cp "$BUILD/win64/libqt_plugin.stripped.dll" "$PKG/plugins/gui/libqt_plugin.dll" ||
    { echo "copy failed: close the vlc.exe running from $PKG (it locks the plugin)" >&2; exit 1; }
echo "updated $PKG/plugins/gui/libqt_plugin.dll"

# A changed plugin makes plugins.dat stale: every vlc.exe (also each library
# thumbnail probe) then loads that DLL and its Qt DLLs at start. The cross
# build skips vlc-cache-gen, so run the package's own copy through WSL interop.
# Without interop, on Windows: <pkg>\vlc-cache-gen.exe <pkg>\plugins
"$PKG/vlc-cache-gen.exe" "$(wslpath -w "$PKG/plugins")" ||
    echo "plugins.dat not refreshed: run $PKG/vlc-cache-gen.exe on its plugins folder" >&2
