/*****************************************************************************
 * theme_tokens.hpp : design tokens of the redesigned interface
 *****************************************************************************
 * Copied from redesign/design/v3/redesign_tokens.hpp (generated from
 * redesign/design/tokens.json), plus the few tokens.json values the generated
 * stylesheet needs. The only place colours and sizes are written down: use
 * them through Theme (util/theme.hpp). C++11 and Qt 5.6 compatible: plain QRgb
 * literals (0xAARRGGBB), no constexpr QColor.
 *
 * Sizes are px at 96 dpi: widgets take them through Theme::px() or
 * Theme::metric(), which follow the logical dpi like the fonts do; Qt's
 * high-DPI scaling (AA_EnableHighDpiScaling, qt.cpp) adds the whole device
 * ratio. Line widths (FocusRing, Hairline) stay whole device-independent
 * pixels, like the stylesheet's borders.
 *
 * Deviation from tokens.json: the light theme's accTx is #9a4800, not
 * #b35300, so the active sidebar label (accTx on AccentTint12 over chrome)
 * reaches 4.8:1 instead of 3.8:1.
 *****************************************************************************/

#ifndef VLC_QT_THEME_TOKENS_HPP_
#define VLC_QT_THEME_TOKENS_HPP_

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
    static const QRgb VolumeBoost     = 0xFFFF5A1F;
    static const QRgb SeekKnobFill    = 0xFFFFFFFF; /* DESIGN_SPEC 3: white in both themes */

    /* ---- Status ---- */
    static const QRgb Error            = 0xFFFF5A4E;
    static const QRgb ErrorTint        = 0x24FF5A4E;
    static const QRgb DestructiveText  = 0xFFE5483C;
    static const QRgb WindowCloseHover = 0xFFC42B1C;
    static const QRgb WindowCloseGlyph = 0xFFFFFFFF; /* on WindowCloseHover, both themes */

    /* ---- Video surfaces (both themes) ---- */
    static const QRgb VideoSurface  = 0xFF000000;
    static const QRgb PausedBadgeBg = 0x8C000000; /* 55 % */
    static const QRgb SubtitleBox   = 0xA6000000; /* 65 % */

    struct Palette
    {
        QRgb page, chrome, bg, panel, surface, raised, hover, hover2;
        QRgb tx, tx2, mute, dim, accTx;
        QRgb ink;      /* base for alpha overlays: white (dark) / black (light) */
        QRgb glass;    /* floating bars */
        QRgb fxbg;     /* effects sheet */
        QRgb stripe1, stripe2; /* placeholder thumbnails */
    };

    static const Palette Dark = {
        0xFF0A0A09, 0xFF0E0D0C, 0xFF121110, 0xFF141312, 0xFF1A1917, 0xFF23211F, 0xFF2C2A27, 0xFF35322F,
        0xFFF2EFEA, 0xFFCFCAC3, 0xFFA39E96, 0xFF8D877F, 0xFFFFA552,
        0xFFFFFFFF,
        0xB81E1D1B, /* rgba(30,29,27,0.72) */
        0xF71A1917, /* rgba(26,25,23,0.97) */
        0xFF22201E, 0xFF1C1A18
    };

    static const Palette Light = {
        0xFFD6D2CC, 0xFFEBE8E4, 0xFFF6F4F1, 0xFFF1EEEA, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFE6E2DD, 0xFFDBD6D0,
        0xFF1B1916, 0xFF3D3933, 0xFF5F5A53, 0xFF746E66, 0xFF9A4800,
        0xFF000000,
        0xBDFAF9F7, /* rgba(250,249,247,0.74) */
        0xF7FFFFFF,
        0xFFE7E3DD, 0xFFDFDAD3
    };

    /* Alpha (0..255) applied to Palette::ink for overlays */
    namespace InkAlpha
    {
        static const int Hairline  = 13;  /* 0.05 */
        static const int Divider   = 15;  /* 0.06 */
        static const int HoverFill = 18;  /* 0.07 */
        static const int Border    = 20;  /* 0.08: menu and popover outline (DESIGN_SPEC 7) */
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
        static const int WeightRegular = 400, WeightSemibold = 600; /* CSS weights */
    }

    /* ---- Metrics (px) ---- */
    namespace Size
    {
        static const int TitleBar = 40, CaptionButtonW = 46;
        static const int TitleCone = 18, TitleConeW = 42; /* cone with 14 + 10 px padding */
        static const int CaptionGlyph = 18, CaptionGlyphSmall = 16; /* close and minimise; maximise */
        static const int TitlePadX = 16;
        static const int MenuTitleH = 28, MenuItemH = 30, MenuMinW = 290;
        static const int MenuTitlePadX = 9, MenuPanelPadding = 4, MenuIconCol = 22;
        static const int ButtonPadX = 16;
        static const int SidebarItemH = 36, SidebarOpenBtnH = 38, SidebarRailBelow = 1200;
        /* not in tokens.json: the prototype's sidebar and library page layout */
        static const int SidebarL = 260, SidebarM = 232, SidebarS = 72, SidebarXS = 64;
        static const int SidebarPadY = 14, SidebarPadX = 10, SidebarItemGap = 4, SidebarOpenBelow = 10;
        static const int SidebarIcon = 20, SidebarItemPadX = 12, SidebarIconGap = 12;
        static const int SidebarOpenIconGap = 8, SidebarOpenRadius = 7;
        static const int PagePadTop = 28, PagePadX = 32;
        static const int IconButton = 36, IconGlyph = 22;
        static const int PlayDeck = 44, PlayNowBar = 36, PausedBadge = 76;
        static const int NowPlayingBarH = 76;
        static const int SeekTrack = 4, SeekTrackHover = 6, SeekKnob = 14, SeekKnobHover = 16;
        static const int VolumeW = 140, VolumeMaxPercent = 125;
        static const int PlaylistRowH = 40, PlaylistL = 360, PlaylistM = 320, PlaylistS = 300;
        static const int ButtonH = 34, InputH = 34, UrlInputH = 38, SelectH = 32;
        static const int ToggleW = 40, ToggleH = 20, ChipH = 28;
        static const int ScrollBarW = 12; /* not in tokens.json: Windows 11 thin scroll bar */
        static const int FocusRing = 2, Hairline = 1; /* line widths, not scaled */
        static const int RadiusXs = 4, RadiusSm = 5, RadiusMd = 6, RadiusLg = 8, RadiusXl = 10, RadiusTile = 12;
    }

    /* ---- Breakpoints by window width ---- */
    namespace Breakpoint
    {
        /* XSMinH is 480, not tokens.json's 600, for quarter snaps; the
         * window also stays under half the work area high (main_interface) */
        static const int L = 1600, M = 1200, S = 960, XSMinW = 800, XSMinH = 480;
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
        static const int SlideOffset = 28, RiseOffset = 8, DropOffset = 6, ToastOffset = 12;
    }

    /* ---- Toast and drop target (DESIGN_SPEC 7; sizes from the prototype markup) ---- */
    namespace Overlay
    {
        static const int ToastPadX = 16, ToastPadY = 10, ToastGap = 10, ToastIcon = 18;
        static const int ToastMargin = 24;      /* above the bottom of the content area */
        static const int ToastOverVideoPercent = 20; /* of the video's height: two subtitle
                                                        lines at VLC's default size, and room */
        static const int ToastBorderAlpha = 26; /* ink 0.10 */
        static const int DropInset = 8, DropBorder = 2, DropFillAlpha = 26; /* accent 0.10 */
        static const int DropCardPadX = 36, DropCardPadY = 28, DropGap = 10, DropIcon = 40;
    }

    /* ---- Phase 3: video overlays (DESIGN_SPEC 2 and 7; sizes from the prototype markup) ---- */
    static const QRgb OnVideo        = 0xFFFFFFFF; /* glyphs drawn on the video, both themes */
    static const QRgb BufferingScrim = 0x99000000; /* 60 % over the last frame */
    namespace VideoOverlay
    {
        static const int BadgeGlyph = 44, BadgeBorderAlpha = 38; /* ink 0.15 */
        static const int PopFromPercent = 96;     /* the "pop" enter scales from 0.96 */
        static const int Ring = 48, RingWidth = 3, RingAlpha = 38; /* white 0.15 */
        static const int RingTurn = 900;          /* ms per turn (motion.spinner) */
        static const int BufferingGap = 14;
        /* not in tokens.json: a local file buffers for a few frames on every
         * start; the ring only shows when buffering lasts longer than that */
        static const int BufferingShowDelay = 400;
        static const int ImmersiveMargin = 16;    /* from the edges of the video */
        static const int ImmersivePadTop = 10, ImmersivePadX = 16, ImmersivePadBottom = 12;
        static const int ImmersiveBorderAlpha = 26; /* ink 0.10 */
        static const int ImmersiveOffset = 12;    /* the idle hide sinks it 12 px */
    }
}

#endif
