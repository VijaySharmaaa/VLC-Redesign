/*****************************************************************************
 * theme.hpp : colours, fonts, sizes and motion of the interface
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

#ifndef VLC_QT_THEME_HPP_
#define VLC_QT_THEME_HPP_

#include "qt.hpp"
#include "util/theme_tokens.hpp"

#include <QObject>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QIcon>

class QAction;
class QApplication;
class QPainter;
class QRectF;
class QScreen;
class QWidget;

/* The single source of the interface's look. One instance lives in the Qt
 * thread (ThreadPlatform) for the whole life of the QApplication; widgets
 * read it through Theme::instance(). Qt thread only. */
class Theme : public QObject
{
    Q_OBJECT
public:
    /* qt-redesign-theme values */
    enum Mode { FollowSystem = 0, Dark = 1, Light = 2 };

    enum ColorRole
    {
        /* per theme (RedesignTokens::Palette) */
        Chrome, Bg, Panel, Surface, Raised, Hover, Hover2,
        Text, Text2, Mute, Dim, AccentText, Glass, FxBg, Stripe1, Stripe2,
        /* AccentFill: progress and "on" fills, the accent in both themes
         * (I1-1); ControlLine: a field's only boundary */
        AccentFill, OnAccentFill, VolumeBoost, DestructiveText, ControlLine,
        /* same in both themes, except AccentFocusRing: focus rings and
         * accent marks, Accent in dark and AccentText in light (the accent
         * is 2.4:1 on light surfaces, AccentText >= 3:1) */
        Accent, AccentHover, OnAccent, AccentTint12, AccentTint14, AccentBorder50,
        AccentFocusRing, AccentKnobHalo, SeekKnobFill,
        Error, ErrorTint, WindowCloseHover, WindowCloseGlyph,
        VideoSurface, PausedBadgeBg, OnVideo,
        ColorRoleCount
    };

    enum TypeRole { Display, PageTitle, PanelTitle, Body, Meta, Overline, Mono, NavTitle };

    enum SizeRole
    {
        TitleBar, CaptionButtonW, MenuTitleH, MenuItemH, MenuMinW,
        SidebarItemH, SidebarOpenBtnH, IconButton, IconGlyph,
        PlayDeck, PausedBadge, NowPlayingBarH,
        SeekTrack, SeekTrackHover, SeekKnob, SeekKnobHover, VolumeW,
        PlaylistRowH, PlaylistL, PlaylistM, PlaylistS,
        InputH, UrlInputH, SelectH, ToggleW, ToggleH, ChipH,
        RadiusXs, RadiusSm, RadiusMd, RadiusLg, RadiusXl, RadiusTile,
        SizeRoleCount
    };

    enum MotionRole
    {
        Popover, PanelSlide, ViewChange, Toast, PausedBadgeFade, MiniPlayer, DropTarget,
        Toggle, PlayPress, ImmersiveFade,
        Spinner,    /* one turn of the buffering ring */
        Spectrum,
        SeekLoading,
        MotionRoleCount
    };

    explicit Theme(intf_thread_t *);
    ~Theme();
    static Theme *instance() { return s_instance; }

    /* Style, palette, font and stylesheet of the whole application. Also
     * hooks the title bar colours of every top-level window. Call once,
     * before the first window exists. */
    void apply(QApplication &);
    /* Re-reads the mode (options and Windows) and re-applies when it
     * changed; emits changed(). Call after WM_SETTINGCHANGE or a setting. */
    void reload();
    /* The only way to change the application style: wraps the named
     * QStyleFactory style (empty = Fusion) so the token painting stays. */
    void setBaseStyle(const QString &);
    /* The metrics follow the logical dpi of the screen the main window is
     * on: when it differs, re-applies the stylesheet and emits changed().
     * Returns false when the scale stays the same. */
    bool setScreen(const QScreen *);

    bool isDark() const { return dark; }
    bool isHighContrast() const { return highContrast; }
    const RedesignTokens::Palette &palette() const;
    QColor color(ColorRole) const;
    QColor ink(int alpha) const;     /* alpha from RedesignTokens::InkAlpha */
    QFont font(TypeRole) const;
    /* A token's px (RedesignTokens::Size, Overlay, Breakpoint) in
     * device-independent px: tokens are px at 96 dpi and fonts are in
     * points, so metrics follow the logical dpi as the text does. */
    int px(int tokenPx) const { return qRound(tokenPx * scale); }
    /* A stock point size or step in px: a font in pixels follows the
     * screen's scale (setScreen), one in points keeps the dpi Qt 5.6
     * resolved it at */
    int pt(qreal points) const { return qRound(points * 4 / 3 * scale); }
    int metric(SizeRole) const;
    int duration(MotionRole) const;  /* 0 when Windows animations are off */
    /* How far a vertically centred line of this font must move up so its
     * capitals, not its line box, sit on the centre: Segoe UI's capitals
     * are about 0.064 em below the line box's centre. */
    int textNudge(const QFont &) const;
    /* an empty size means the IconGlyph token */
    QIcon icon(const QString &name, ColorRole, const QSize &size = QSize());
    /* The same icon at any size, tinted with the colour the role has when
     * it is drawn: for menus and actions that outlive a theme change.
     * glyphPx > 0: the glyph keeps that token size, centred in a bigger
     * box (a line edit's action icons are 32 px in a tall field). */
    static QIcon themedIcon(const char *name, ColorRole, int glyphPx = 0);
    /* A menu entry that removes or clears: drawn in DestructiveText */
    static void setDestructive(QAction *);
    /* Whether the widget, which has focus, shows its focus ring: only for
     * focus from the keyboard (Tab, an access key, arrow keys), and always
     * in a text field, as CSS :focus-visible. In place of hasFocus() where
     * a widget or delegate paints its own ring. */
    static bool showsFocus(const QWidget *);
    /* The keyboard focus ring: an AccentFocusRing line FocusRing px wide,
     * inside r, its centre line rounded by radius. Leaves that pen and no
     * brush set. */
    static void paintFocusRing(QPainter *, const QRectF &r, qreal radius);
    /* Text on a coloured fill (the accent): grayscale antialiasing, as
     * ClearType's red and green fringes show on anything but grey. Same
     * arguments as QPainter::drawText, in the painter's current font. */
    static void drawGrayText(QPainter *, const QRect &r, int flags, const QString &text, const QColor &);

signals:
    void changed();

protected:
    bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;

private:
    bool readDark() const;
    void applyPalette();
    QString styleSheet() const;
    void applyWindowFrame(QWidget *) const;

    static Theme *s_instance;
    intf_thread_t *p_intf;
    bool dark;
    bool highContrast;
    bool keyboardInput;  /* the last key or mouse press was a key (showsFocus) */
    qreal scale;        /* logical dpi / 96, see px() */
    QString uiFamily, monoFamily;
    QHash<QString, QIcon> icons;
    mutable QHash<QString, int> nudges;  /* textNudge(), by font and pixel size */
};

#endif
