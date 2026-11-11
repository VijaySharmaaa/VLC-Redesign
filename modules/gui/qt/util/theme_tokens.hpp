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
 * reaches 4.8:1 instead of 3.8:1. The glass is 0.88 opaque in both themes,
 * not 0.72 / 0.74: the design blurs what is under it, which Qt 5.6 widgets
 * cannot (P6-8).
 *
 * Per theme since Phase 6 (WCAG 1.4.3 / 1.4.11): the destructive text and the
 * outline of fields, selects and switches, which is their only boundary.
 * Outlines take ink 0.45 in light (>= 3:1) and the switch's ink 0.36 in dark.
 * Fills (progress, seek, volume, sliders, switches, checked boxes and radios)
 * and the volume boost are the design's bright accent in both themes, with
 * OnAccent on them (8:1): the owner chose it over P6-2's darker light fill
 * (accTx), so a light fill is 2.4:1 on white, below 1.4.11's 3:1 (I1-1).
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
    static const QRgb SeekKnobFill    = 0xFFFFFFFF; /* DESIGN_SPEC 3: white in both themes */

    /* ---- Status ---- */
    static const QRgb Error            = 0xFFFF5A4E;
    static const QRgb ErrorTint        = 0x24FF5A4E;
    static const QRgb WindowCloseHover = 0xFFC42B1C;
    static const QRgb WindowCloseGlyph = 0xFFFFFFFF; /* on WindowCloseHover, both themes */

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
        QRgb danger;   /* destructive menu text, also on the hovered row (>= 4.5:1) */
        QRgb line;     /* outline of fields, selects, switches and check boxes */
    };

    static const Palette Dark = {
        0xFF0A0A09, 0xFF0E0D0C, 0xFF121110, 0xFF141312, 0xFF1A1917, 0xFF23211F, 0xFF2C2A27, 0xFF35322F,
        0xFFF2EFEA, 0xFFCFCAC3, 0xFFA39E96, 0xFF8D877F, 0xFFFFA552,
        0xFFFFFFFF,
        0xE01E1D1B, /* rgba(30,29,27,0.88): the design's 0.72 is under a blur Qt 5.6 lacks */
        0xF71A1917, /* rgba(26,25,23,0.97) */
        0xFF22201E, 0xFF1C1A18,
        0xFFFF8A1F, 0xFF1C1206, 0xFFFF5A1F, 0xFFFF7468,
        0x5CFFFFFF  /* ink 0.36: 3:1 on hover2 */
    };

    static const Palette Light = {
        0xFFD6D2CC, 0xFFEBE8E4, 0xFFF6F4F1, 0xFFF1EEEA, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFE6E2DD, 0xFFDBD6D0,
        0xFF1B1916, 0xFF3D3933, 0xFF5F5A53, 0xFF746E66, 0xFF9A4800,
        0xFF000000,
        0xE0FAF9F7, /* rgba(250,249,247,0.88): the design's 0.74 is under a blur Qt 5.6 lacks */
        0xF7FFFFFF,
        0xFFE7E3DD, 0xFFDFDAD3,
        0xFFFF8A1F, 0xFF1C1206, 0xFFFF5A1F, 0xFFB02515, /* fills as dark (I1-1) */
        0x73000000  /* ink 0.45 */
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
        static const int NavTitle = 22;       /* "Preferences" over its sections */
        static const int TitleTracking = 99;  /* % letter spacing: -0.01em */
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
        /* Preferences in the sidebar (DESIGN_SPEC 8): the back row, the
         * heading and the sections, the active one's accent bar */
        static const int SidebarBackH = 32, SidebarBackIconGap = 10, SidebarBackBelow = 6;
        static const int SidebarTitlePadTop = 4, SidebarTitlePadBottom = 14, SidebarSectionGap = 2;
        static const int SidebarBarW = 3, SidebarBarInset = 9, SidebarBarRadius = 2;
        static const int PagePadTop = 28, PagePadX = 32;
        static const int IconButton = 36, IconGlyph = 22;
        static const int PlayDeck = 44, PlayNowBar = 36, PausedBadge = 76;
        static const int NowPlayingBarH = 76;
        static const int SeekTrack = 4, SeekTrackHover = 6, SeekKnob = 14, SeekKnobHover = 16;
        static const int VolumeW = 140, VolumeMaxPercent = 125;
        static const int PlaylistRowH = 40, PlaylistL = 360, PlaylistM = 320, PlaylistS = 300;
        static const int ButtonH = 34, InputH = 34, UrlInputH = 38, SelectH = 32;
        static const int ToggleW = 40, ToggleH = 20, ChipH = 28;
        static const int ToggleKnob = 12; /* the switch's knob, centred in ToggleH */
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
        static const int PlayPressScale = 92;   /* percent of its size while pressed */
        static const int ImmersiveIdle = 2500, ImmersiveFade = 300;
        /* the seek bar's loading bounce: one sweep, shown after the delay */
        static const int SeekLoading = 2000, SeekLoadingDelay = 500;
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
        static const int ImmersiveBorderAlpha = 26; /* ink 0.10 */
        static const int ImmersiveOffset = 12;    /* the idle hide sinks it 12 px */
        /* the player header over the top of the video: back, title, meta */
        static const int HeaderPadY = 16, HeaderPadX = 18, HeaderGap = 12;
        static const int HeaderBack = 34, HeaderBackGlyph = 20;
        static const int HeaderBackAlpha = 20, HeaderBackHoverAlpha = 41; /* white 0.08, 0.16 */
        static const int HeaderTitle = 15;        /* semibold; the meta is Type::Meta */
    }

    /* ---- Phase 3: playlist panel (DESIGN_SPEC 6; sizes from the prototype markup) ---- */
    namespace Playlist
    {
        static const int HeadPadTop = 16, HeadPadRight = 12, HeadPadBottom = 10, HeadPadLeft = 18;
        static const int HeadGap = 6, HeadButton = 30, HeadButtonGlyph = 20, CountGap = 2;
        static const int FilterMarginX = 12, FilterMarginBottom = 8;
        static const int ListPadX = 6, ListPadBottom = 10;
        static const int RowPadX = 10, RowGap = 10, RowNumberW = 24, RowIcon = 18, RowMetaGap = 2;
        static const int DrawerShadow = 50, DrawerShadowAlpha = 115; /* -20px 0 50px rgba(0,0,0,.45) */
    }

    /* ---- Phase 3: now playing, mini player, audio mode (DESIGN_SPEC 4, 5, 9;
     *      sizes from the prototype markup) ---- */
    namespace NowPlaying
    {
        /* now-playing bar: NowPlayingBarH tall, PlayNowBar play button */
        static const int PadX = 20, ColumnGap = 24, InfoGap = 12, TextGap = 3;
        static const int ThumbW = 64, ThumbH = 40, ThumbStripe = 12; /* stripe period */
        static const int SmallButton = 32, TransportGap = 12, RowGap = 4;
        static const int CentreMinW = 280, CentreMaxW = 560, TimeW = 40, TimeGap = 10;
        static const int ThinSeek = 3, ThinSeekHit = 12;

        /* mini player */
        static const int MiniW = 360, MiniH = 200, MiniPadX = 12, MiniPadY = 10, MiniGap = 6;
        static const int MiniMargin = 24;
        static const int MiniRestoreBgAlpha = 140; /* black 0.55 behind the restore glyph */

        /* audio mode, per breakpoint L / M / S / XS */
        static const int ArtL = 420, ArtM = 340, ArtS = 280, ArtXS = 220;
        static const int TitleL = 48, TitleM = 40, TitleS = 32, TitleXS = 28; /* Display at L */
        static const int GapL = 56, GapM = 44, GapS = 32, GapXS = 24;
        static const int PadTopL = 48, PadXL = 64, PadTopM = 36, PadXM = 44, PadXS = 24;
        static const int ArtStripe = 24, ArtRadius = 10, ColumnInnerGap = 16;
        static const int PillH = 30, PillPadLeft = 6, PillPadRight = 10;
        static const int ChipPadX = 8, ChipPadY = 3, ChipGap = 8;
        static const int HeaderTop = 46, TitleTop = 8, MetaTop = 8, ListTop = 24;
        static const int TrackNumW = 28, TrackPadX = 10, TrackGap = 12, TrackIcon = 18, TrackTitle = 14;
        /* not in tokens.json: below this many rows the spectrum makes room */
        static const int TrackMinRows = 3;

        /* spectrum strip: bars rise and fall at full height per 250 ms, linear */
        static const int SpectrumH = 84, SpectrumBars = 64, SpectrumGap = 3, SpectrumPadX = 44;
        static const int SpectrumMinBar = 3, SpectrumBarRadius = 2;
        static const int SpectrumAlphaEven = 217, SpectrumAlphaOdd = 140; /* accent 0.85 / 0.55 */
        static const int SpectrumMotion = 250, SpectrumFrame = 33;        /* ms, ~30 fps */
    }

    /* Phase 3: deck */
    /* ---- Control deck and its popovers (DESIGN_SPEC 3, 7; sizes from the prototype markup) ---- */
    namespace Deck
    {
        static const int PadX = 16, PadTop = 10, PadBottom = 12, RowGap = 6, Gap = 4;
        static const int SeekRowH = 20, SeekRowGap = 12, TimeW = 52;
        static const int SeparatorH = 20, SeparatorMargin = 6;
        static const int PlayGlyph = 28, KnobHalo = 3, ChapterGap = 2;
        static const int VolumeH = 24, VolumeTickH = 16, VolumeLabelW = 40;
        static const int SpeedPadX = 10;
        static const int DisabledOpacityPercent = 35;
        static const int TracksPopoverW = 560, SpeedPopoverW = 140, PopoverGap = 12;
        static const int PopoverPadX = 6, PopoverPadTop = 12, PopoverPadBottom = 8;
        static const int PopoverRowPadX = 10, PopoverRowPadY = 7, PopoverRowGap = 8;
        static const int PopoverCheck = 16, PopoverCheckCol = 20, PopoverMaxRows = 8, PopoverAddIcon = 18;
    }
    namespace InkAlpha
    {
        static const int Separator  = 26;  /* 0.10: deck separators */
        static const int VolumeTick = 64;  /* 0.25: the 100 % tick */
    }
    /* Phase 4: home */
    /* ---- Library home (DESIGN_SPEC 4; sizes from the prototype markup) ---- */
    static const QRgb CardBadgeBg = 0xBF000000; /* "9:37 left": black 0.75 */
    namespace Home
    {
        static const int TitleBottom = 24, SectionGap = 32;
        static const int SectionTitle = 15, CardsTitleBottom = 12, TableTitleBottom = 8;
        /* continue watching: 16:9 cards, L / M / S / XS cards per row */
        static const int ColsL = 5, ColsM = 4, ColsS = 3, ColsXS = 2;
        static const int CardGap = 16, CardStripe = 20, CardGlyph = 32;
        static const int CardTextGap = 8, CardMetaGap = 2, CardTitle = 14;
        static const int BadgeInsetX = 8, BadgeInsetY = 10, BadgePadX = 6, BadgePadY = 2;
        static const int Progress = 3;
        /* recent media: icon · title · path · duration · when */
        static const int RowH = 44, RowIcon = 20, RowPadX = 10, ColumnGap = 12, IconCol = 28;
        static const int PathColL = 300, PathColS = 200; /* hidden at XS */
        static const int DurationColL = 70, DurationColS = 60, WhenColL = 110, WhenColS = 90;
        /* missing files */
        static const int MissingOpacityPercent = 45;
        /* the empty state is the library pages' (Library::Empty*) */
    }
    namespace InkAlpha
    {
        static const int RowLine       = 10; /* 0.04: under each recent row */
        static const int ProgressTrack = 38; /* 0.15: the card's progress track */
    }

    /* ---- Phase 4: library pages Videos, Music, Network (DESIGN_SPEC 4;
     *      sizes from the prototype markup) ---- */
    namespace Library
    {
        /* page: PagePadTop / PagePadX, then the header row */
        static const int PagePadBottom = 32, HeaderGap = 16, HeaderBelow = 24;
        static const int SearchL = 360, SearchM = 300, SearchS = 220, SearchXS = 180;
        static const int SearchIcon = 18;
        /* card grids, per breakpoint L / M / S / XS */
        static const int VideoColsL = 5, VideoColsM = 4, VideoColsS = 3, VideoColsXS = 2;
        static const int AlbumColsL = 7, AlbumColsM = 5, AlbumColsS = 4, AlbumColsXS = 3;
        static const int GridGapX = 16, GridGapY = 24;
        static const int ThumbAspectW = 16, ThumbAspectH = 9;
        static const int CardRadius = 8, CardStripe = 20, CardTextGap = 8, CardMetaGap = 2;
        static const int CardTitle = 14, CardGlyph = 40;
        static const int BadgeInset = 8, BadgePadX = 6, BadgePadY = 2, BadgeRadius = 4;
        /* network tiles */
        static const int NetColsL = 3, NetColsM = 3, NetColsS = 2, NetColsXS = 2;
        static const int GroupGap = 28, GroupTitle = 15, GroupTitleBelow = 10;
        static const int TileGap = 12, TilePad = 14, TileInnerGap = 14, TileRadius = 8;
        static const int TileIcon = 40, TileGlyph = 22, TileSubGap = 3;
        /* empty state (Home too): up to EmptyMinH tall, less in a short window */
        static const int EmptyMinH = 420, EmptyPad = 32, EmptyGap = 14, EmptyCone = 56;
        static const int EmptyTitle = 20, EmptyTextW = 380, EmptyButtonH = 36, EmptyButtonGap = 8;
        static const int EmptyButtonsTop = 6, EmptyButtonIcon = 18;
        static const int EmptyBorderAlpha = 36; /* ink 0.14 */
        static const int EmptyBorderTenths = 15; /* 1.5 px dashes */
        /* not in tokens.json: the SDs have no "scan finished" event, so a
         * page that got no item for this long is empty */
        static const int SettleMs = 1500;
        /* the same for an internet discovery's list, which first downloads
         * its directory (Icecast: a few MB) */
        static const int NetSettleMs = 10000;
    }

    /* ---- Phase 5: effects sheet (DESIGN_SPEC 7; sizes from the
     *      prototype markup) ---- */
    namespace Effects
    {
        static const int SheetW = 430, SheetWXS = 340;
        static const int HeadPadTop = 14, HeadPadRight = 14, HeadPadLeft = 18;
        static const int CloseButton = 30, CloseGlyph = 18;
        static const int TabsPadX = 14, TabsPadTop = 12, BodyPad = 18, SectionGap = 18;
        static const int ChipGap = 6, ChipPadX = 10;
        static const int BandH = 170, BandTrack = 4, BandKnob = 14, BandGap = 4, BandScaleW = 28;
        static const int VideoRowGap = 20;
        static const int SyncRowPad = 14, SyncRowGap = 12, SyncRowRadius = 8;
        static const int StepperButton = 30, StepperGlyph = 18;
        static const int DisabledAlpha = 102; /* 0.4: bands with the equalizer off */
    }
    namespace InkAlpha
    {
        static const int ChipBorder = 31;    /* 0.12 */
        static const int StepperFill = 15;   /* 0.06 */
        static const int StepperHover = 31;  /* 0.12 */
    }

    /* ---- Phase 5: dialogs (DESIGN_SPEC 7; sizes from the prototype markup) ---- */
    namespace Dialogs
    {
        /* Open media: the tab tiles */
        static const int TabTileH = 58, TabTileGap = 6, TabTileGlyph = 22;
        /* Error: the round icon left of the title */
        static const int ErrorIcon = 40, ErrorGlyph = 22;
        /* not in tokens.json: the prototype's 480 px card, and room for
         * two lines of details before they scroll */
        static const int ErrorW = 480, ErrorH = 190;
        /* Go to time: the timecode field */
        static const int JumpInputH = 44, JumpFont = 20;
        /* About: the cone and the card width */
        static const int AboutLogo = 64, AboutW = 440;
    }

    /* ---- Phase 5: Preferences page (DESIGN_SPEC 8; sizes from the
     *      prototype markup) ---- */
    namespace Prefs
    {
        static const int ContentPadY = 28, ContentPadX = 32, HeaderBelow = 16;
        static const int ContentMaxW = 720;   /* the prototype's max-width */
        /* the card and its rows */
        static const int CardRadius = 8, RowMinH = 60, RowPadX = 18, RowPadY = 12;
        static const int RowGap = 20, DescGap = 3, ButtonGap = 8;
        /* selects and fields on the right of a row: the prototype's selW
         * (it narrows to 180 at XS) */
        static const int ControlMinW = 240, ControlMinWXS = 180;
        /* not in tokens.json: the Hotkeys table shows at least this many rows */
        static const int HotkeysMinRows = 10;
        /* a hotkey's keycap: the prototype's padding 4/10, radius 5, mono
         * 12, an ink 0.10 outline, 2 px at the bottom */
        static const int KeycapPadY = 4, KeycapPadX = 10, KeycapRadius = 5, KeycapBottom = 2;
    }
    namespace InkAlpha
    {
        static const int Keycap = 26;   /* 0.10 */
    }

    /* issues1 lane F */
    namespace Dialogs
    {
        /* not in tokens.json: Open media, a tile's icon above its label */
        static const int TabTileTextGap = 4;
        /* not in tokens.json: Capture device, how long the device list may
         * take before its boxes offer the default device (a stuck scan) */
        static const int DshowScanWaitMs = 5000;
    }
}

/* issues1 lane E */
namespace RedesignTokens
{
    namespace Library
    {
        /* not in tokens.json: video frames (Thumbnailer, I1-3), made one
         * at a time by a headless vlc.exe */
        static const int ThumbWidth = 480;          /* the PNG's width, px */
        static const int ThumbAtPercent = 10;       /* where the frame is taken */
        static const int ThumbUnknownAtS = 10;      /* the same, length unknown */
        static const int ThumbSpanS = 1;            /* played from there */
        static const int ThumbTimeoutMs = 15000;    /* then the process is killed */
        static const int ThumbKillWaitMs = 1000;
        static const int ThumbQueueMax = 24;        /* about the cards on screen */
    }
}

/* issues2 lane C */
namespace RedesignTokens
{
    namespace NowPlaying
    {
        /* not in tokens.json: the mini player's Restore, a round chip like the
         * player header's back (VideoOverlay::HeaderBack*), clear of the corner */
        static const int MiniRestoreChip = 32, MiniRestoreGlyph = 20, MiniRestoreEdge = 10;
    }
}

#endif
