/*****************************************************************************
 * win_theme.hpp : Windows 10/11 title bar colours and popup corners
 *****************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_WIN_THEME_HPP_
#define VLC_QT_WIN_THEME_HPP_

#ifdef _WIN32

#include <windows.h>
#include <QLibrary>
#include <QRgb>

/* Everything here is loaded at runtime: the build targets XP (0x0502).
 * Each call returns E_NOTIMPL where Windows lacks the attribute. */
namespace WinTheme
{
    typedef HRESULT (WINAPI *SetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW *);

    enum
    {
        ATTR_DARK_1809      = 19,   /* undocumented, builds 17763..18984 */
        ATTR_DARK           = 20,   /* DWMWA_USE_IMMERSIVE_DARK_MODE, 18985+ */
        ATTR_CORNER         = 33,   /* DWMWA_WINDOW_CORNER_PREFERENCE, 22000+ */
        ATTR_CAPTION_COLOR  = 35,   /* DWMWA_CAPTION_COLOR, 22000+ */
        ATTR_TEXT_COLOR     = 36,   /* DWMWA_TEXT_COLOR, 22000+ */
        CORNER_ROUND        = 2,    /* DWMWCP_ROUND: 8 px */
        BUILD_WINDOWS_11    = 22000,
    };

    inline DWORD build()
    {
        static const DWORD b = []() -> DWORD {
            RtlGetVersionFn f = reinterpret_cast<RtlGetVersionFn>(
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
            OSVERSIONINFOW vi;
            ZeroMemory(&vi, sizeof(vi));
            vi.dwOSVersionInfoSize = sizeof(vi);
            return f && f(&vi) == 0 ? vi.dwBuildNumber : 0;
        }();
        return b;
    }

    inline SetAttrFn setAttr()
    {
        static const SetAttrFn f = reinterpret_cast<SetAttrFn>(
            QLibrary::resolve("dwmapi", "DwmSetWindowAttribute"));
        return f;
    }

    inline HRESULT setDword(HWND hwnd, DWORD attr, DWORD value, DWORD minBuild)
    {
        SetAttrFn f = setAttr();
        if (!f || !hwnd || build() < minBuild)
            return E_NOTIMPL;
        return f(hwnd, attr, &value, sizeof(value));
    }

    /* COLORREF is 0x00BBGGRR */
    inline DWORD colorRef(QRgb c)
    {
        return RGB(qRed(c), qGreen(c), qBlue(c));
    }

    inline HRESULT setDarkTitleBar(HWND hwnd, bool dark)
    {
        return setDword(hwnd, build() >= 18985 ? ATTR_DARK : ATTR_DARK_1809,
                        dark ? TRUE : FALSE, 17763);
    }

    /* Overrides an accent-coloured caption so it matches the window chrome. */
    inline HRESULT setCaptionColors(HWND hwnd, QRgb caption, QRgb text)
    {
        const HRESULT hr = setDword(hwnd, ATTR_CAPTION_COLOR, colorRef(caption), BUILD_WINDOWS_11);
        if (SUCCEEDED(hr))
            setDword(hwnd, ATTR_TEXT_COLOR, colorRef(text), BUILD_WINDOWS_11);
        return hr;
    }

    inline HRESULT setRoundCorners(HWND hwnd)
    {
        return setDword(hwnd, ATTR_CORNER, CORNER_ROUND, BUILD_WINDOWS_11);
    }

    /* A visible caption keeps its old colours until the frame is recalculated. */
    inline void refreshFrame(HWND hwnd)
    {
        if (IsWindowVisible(hwnd))
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                         | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

#endif /* _WIN32 */
#endif
