/*****************************************************************************
 * redesign_tokens.hpp : design tokens for the VLC 3 (Qt 5.6) redesign
 *****************************************************************************
 * Generated from design/tokens.json. Starting point for
 * modules/gui/qt/util/theme.hpp. C++11 and Qt 5.6 compatible: plain QRgb
 * literals (0xAARRGGBB), no constexpr QColor.
 *
 * Sizes are device-independent pixels; Qt's high-DPI scaling
 * (AA_EnableHighDpiScaling, set in qt.cpp) handles the device ratio.
 *****************************************************************************/

#ifndef VLC_QT_REDESIGN_TOKENS_HPP_
#define VLC_QT_REDESIGN_TOKENS_HPP_

#include <QRgb>

namespace RedesignTokens
{
    /* ---- Accent (same in both themes) ---- */
    static const QRgb Accent          = 0xFFFF8A1F;
    static const QRgb AccentHover     = 0xFFFF9A3C;
    static const QRgb OnAccent        = 0xFF1C1206;
    static const QRgb AccentTint12    = 0x1FFF8A1F; /* 12 % */
    static const QRgb AccentTint14    = 0x24FF8A1F; /* 14 % */
    static const QRgb AccentBorder50  = 0x80FF8A1F;
    static const QRgb AccentFocusRing = 0x2EFF8A1F; /* 18 % */
    static const QRgb AccentKnobHalo  = 0x59FF8A1F; /* 35 % */

    /* ---- Status ---- */
    static const QRgb Error            = 0xFFFF5A4E;
    static const QRgb ErrorTint        = 0x24FF5A4E;
    static const QRgb WindowCloseHover = 0xFFC42B1C;

    /* ---- Video surfaces (both themes) ---- */
    static const QRgb VideoSurface  = 0xFF000000;
    static const QRgb PausedBadgeBg = 0x8C000000; /* 55 % */
    static const QRgb SubtitleBox   = 0xA6000000; /* 65 % */
    static const QRgb HeaderScrim   = 0x99000000; /* 60 %, fading to none below */

    struct Palette
    {
        QRgb page, chrome, bg, panel, surface, raised, hover, hover2;
        QRgb tx, tx2, mute, dim, accTx;
        QRgb ink;      /* base for alpha overlays: white (dark) / black (light) */
        QRgb glass;    /* floating bars */
        QRgb fxbg;     /* effects sheet */
        QRgb stripe1, stripe2; /* placeholder thumbnails */
        QRgb fill;     /* seek, volume and slider progress, switch and check box on */
        QRgb onFill;   /* knob and check mark on fill */
        QRgb boost;    /* volume above 100 % */
        QRgb danger;   /* destructive menu text */
        QRgb line;     /* outline of fields, selects, switches and check boxes */
    };

    static const Palette Dark = {
        0xFF0A0A09, 0xFF0E0D0C, 0xFF121110, 0xFF141312, 0xFF1A1917, 0xFF23211F, 0xFF2C2A27, 0xFF35322F,
        0xFFF2EFEA, 0xFFCFCAC3, 0xFFA39E96, 0xFF8D877F, 0xFFFFA552,
        0xFFFFFFFF,
        0xE01E1D1B, /* rgba(30,29,27,0.88): no-blur fallback (P6-8) */
        0xF71A1917, /* rgba(26,25,23,0.97) */
        0xFF22201E, 0xFF1C1A18,
        0xFFFF8A1F, 0xFF1C1206, 0xFFFF5A1F, 0xFFFF7468,
        0x5CFFFFFF  /* rgba(255,255,255,0.36) */
    };

    static const Palette Light = {
        0xFFD6D2CC, 0xFFEBE8E4, 0xFFF6F4F1, 0xFFF1EEEA, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFE6E2DD, 0xFFDBD6D0,
        0xFF1B1916, 0xFF3D3933, 0xFF5F5A53, 0xFF746E66, 0xFFB35300,
        0xFF000000,
        0xE0FAF9F7, /* rgba(250,249,247,0.88): no-blur fallback (P6-8) */
        0xF7FFFFFF,
        0xFFE7E3DD, 0xFFDFDAD3,
        0xFF9A4800, 0xFFFFFFFF, 0xFFD9400A, 0xFFB02515,
        0x73000000  /* rgba(0,0,0,0.45) */
    };

    /* Alpha (0..255) applied to Palette::ink for overlays */
    namespace InkAlpha
    {
        static const int Hairline  = 13;  /* 0.05 */
        static const int Divider   = 15;  /* 0.06 */
        static const int HoverFill = 18;  /* 0.07 */
        static const int OpenFill  = 23;  /* 0.09 */
        static const int Track     = 31;  /* 0.12 */
        static const int Outline   = 41;  /* 0.16 */
        static const int ToggleOff = 89;  /* 0.35 */
    }

    /* ---- Typography (px) ---- */
    namespace Type
    {
        static const char * const UiFamilies   = "Segoe UI Variable Text,Segoe UI";
        static const char * const MonoFamilies = "JetBrains Mono,Cascadia Mono,Consolas";
        static const int Display = 48, PageTitle = 26, PanelTitle = 16, Body = 13, Meta = 12, Overline = 11, Mono = 12;
        static const int NavTitle = 22;       /* "Preferences" over its sections */
        static const int TitleTracking = 99;  /* % letter spacing: -0.01em */
    }

    /* ---- Metrics (px) ---- */
    namespace Size
    {
        static const int TitleBar = 40, CaptionButtonW = 46;
        static const int MenuTitleH = 28, MenuItemH = 30, MenuMinW = 290;
        static const int SidebarItemH = 36, SidebarOpenBtnH = 38, SidebarRailBelow = 1200;
        /* Preferences in the sidebar (DESIGN_SPEC 8): the back row, the
         * heading and the sections, the active one's accent bar */
        static const int SidebarBackH = 32, SidebarBackIconGap = 10, SidebarBackBelow = 6;
        static const int SidebarTitlePadTop = 4, SidebarTitlePadBottom = 14, SidebarSectionGap = 2;
        static const int SidebarBarW = 3, SidebarBarInset = 9, SidebarBarRadius = 2;
        static const int IconButton = 36, IconGlyph = 22;
        static const int PlayDeck = 44, PlayNowBar = 36, PausedBadge = 76;
        static const int NowPlayingBarH = 76;
        static const int SeekTrack = 4, SeekTrackHover = 6, SeekKnob = 14, SeekKnobHover = 16;
        static const int VolumeW = 140, VolumeMaxPercent = 125;
        static const int PlaylistRowH = 40, PlaylistL = 360, PlaylistM = 320, PlaylistS = 300;
        static const int ButtonH = 34, InputH = 34, UrlInputH = 38, SelectH = 32;
        static const int ToggleW = 40, ToggleH = 20, ChipH = 28;
        static const int RadiusXs = 4, RadiusSm = 5, RadiusMd = 6, RadiusLg = 8, RadiusXl = 10, RadiusTile = 12;
    }

    /* ---- Playlist drawer and location bar ---- */
    namespace Playlist
    {
        static const int DrawerShadow = 50, DrawerShadowAlpha = 115; /* -20px 0 50px rgba(0,0,0,.45) */
        static const QRgb DrawerShadowColor = QRgb(DrawerShadowAlpha) << 24;  /* black at that alpha */
        static const int LocationHoverInsetY = 2;
    }

    /* ---- Breakpoints by window width ---- */
    namespace Breakpoint
    {
        static const int L = 1600, M = 1200, S = 960, XSMinW = 800, XSMinH = 600;
    }

    /* ---- Motion (ms). Easing: cubic-bezier(0.2,0.8,0.2,1) -> QEasingCurve::BezierSpline
     *      with points (0.2,0.8) (0.2,1) (1,1). Enter decelerates; exit is instant. ---- */
    namespace Motion
    {
        static const int MenuDrop = 140, Popover = 160, ContextMenu = 120;
        static const int DialogBackdrop = 180, Dialog = 200, PanelSlide = 220;
        static const int ViewChange = 240, Toast = 220, ToastHold = 2400;
        static const int PausedBadge = 180, MiniPlayer = 240, DropTarget = 150;
        static const int SeekTick = 250, VolumeFill = 120, EqBand = 150, Toggle = 150, PlayPress = 100;
        static const int ImmersiveIdle = 2500, ImmersiveFade = 300;
        static const int SeekLoading = 2000, SeekLoadingDelay = 500;
        static const int SlideOffset = 28, RiseOffset = 8, DropOffset = 6, ToastOffset = 12;
    }

    /* ---- Player header over the top of the video (px) ---- */
    namespace VideoOverlay
    {
        static const int HeaderPadY = 16, HeaderPadX = 18, HeaderGap = 12;
        static const int HeaderBack = 34, HeaderBackGlyph = 20;
        static const int HeaderBackAlpha = 20, HeaderBackHoverAlpha = 41; /* white 0.08, 0.16 */
        static const int HeaderTitle = 15;        /* semibold; the meta is Type::Meta */
    }

    /* ---- Dialogs (px) ---- */
    namespace Dialogs
    {
        static const int AboutLogo = 64, AboutW = 440;
    }

    /* ---- Preferences page (px) ---- */
    namespace Prefs
    {
        static const int ContentMaxW = 720;   /* the prototype's max-width */
        static const int LabelFieldGap = 14;  /* label to the field beside it, advanced forms */
        static const int SpinW = 88;          /* a number field, sized for a few digits */
    }
}

#endif
