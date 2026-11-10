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

class QApplication;
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
        Page, Chrome, Bg, Panel, Surface, Raised, Hover, Hover2,
        Text, Text2, Mute, Dim, AccentText, Ink, Glass, FxBg, Stripe1, Stripe2,
        /* same in both themes */
        Accent, AccentHover, OnAccent, AccentTint12, AccentTint14, AccentBorder50,
        AccentFocusRing, AccentKnobHalo, VolumeBoost, SeekKnobFill,
        Error, ErrorTint, DestructiveText, WindowCloseHover, WindowCloseGlyph,
        VideoSurface, PausedBadgeBg, SubtitleBox,
        ColorRoleCount
    };

    enum TypeRole { Display, PageTitle, PanelTitle, Body, Meta, Overline, Mono };

    enum SizeRole
    {
        TitleBar, CaptionButtonW, MenuTitleH, MenuItemH, MenuMinW,
        SidebarItemH, SidebarOpenBtnH, IconButton, IconGlyph,
        PlayDeck, PlayNowBar, PausedBadge, NowPlayingBarH,
        SeekTrack, SeekTrackHover, SeekKnob, SeekKnobHover, VolumeW,
        PlaylistRowH, PlaylistL, PlaylistM, PlaylistS,
        ButtonH, InputH, UrlInputH, SelectH, ToggleW, ToggleH, ChipH,
        RadiusXs, RadiusSm, RadiusMd, RadiusLg, RadiusXl, RadiusTile,
        SizeRoleCount
    };

    enum MotionRole
    {
        MenuDrop, Popover, ContextMenu, DialogBackdrop, Dialog, PanelSlide,
        ViewChange, Toast, ToastHold, PausedBadgeFade, MiniPlayer, DropTarget,
        SeekTick, VolumeFill, EqBand, Toggle, PlayPress, ImmersiveIdle, ImmersiveFade,
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
    int metric(SizeRole) const;
    int duration(MotionRole) const;  /* 0 when Windows animations are off */
    /* an empty size means the IconGlyph token */
    QIcon icon(const QString &name, ColorRole, const QSize &size = QSize());
    /* The same icon at any size, tinted with the colour the role has when
     * it is drawn: for menus and actions that outlive a theme change. */
    static QIcon themedIcon(const char *name, ColorRole);

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
    qreal scale;        /* logical dpi / 96, see px() */
    QString uiFamily, monoFamily;
    QHash<QString, QIcon> icons;
};

#endif
