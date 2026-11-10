/*****************************************************************************
 * theme.cpp : colours, fonts, sizes and motion of the interface
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util/theme.hpp"
#include "util/imagehelper.hpp"
#include "util/redesign_icons.hpp"

#include <cassert>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QIconEngine>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QRadioButton>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <QVariantAnimation>
#include <QWidget>

#ifdef _WIN32
# include "util/registry.hpp"
# include "util/win_theme.hpp"
# ifndef SPI_GETCLIENTAREAANIMATION
#  define SPI_GETCLIENTAREAANIMATION 0x1042   /* Vista+, missing from XP headers */
# endif
#endif

using namespace RedesignTokens;

Theme *Theme::s_instance = NULL;

namespace
{
    /* Same order as Theme::ColorRole */
    QRgb Palette::* const paletteFields[] = {
        &Palette::page, &Palette::chrome, &Palette::bg, &Palette::panel,
        &Palette::surface, &Palette::raised, &Palette::hover, &Palette::hover2,
        &Palette::tx, &Palette::tx2, &Palette::mute, &Palette::dim, &Palette::accTx,
        &Palette::ink, &Palette::glass, &Palette::fxbg, &Palette::stripe1, &Palette::stripe2,
    };
    const QRgb fixedColors[] = {
        Accent, AccentHover, OnAccent, AccentTint12, AccentTint14, AccentBorder50,
        AccentFocusRing, AccentKnobHalo, VolumeBoost, SeekKnobFill,
        Error, ErrorTint, DestructiveText, WindowCloseHover, WindowCloseGlyph,
        VideoSurface, PausedBadgeBg, SubtitleBox, OnVideo,
    };
    const int paletteCount = sizeof(paletteFields) / sizeof(paletteFields[0]);
    static_assert(paletteCount + sizeof(fixedColors) / sizeof(fixedColors[0])
                  == Theme::ColorRoleCount, "ColorRole and the colour tables differ");

    /* Same order as Theme::SizeRole */
    const int sizes[] = {
        Size::TitleBar, Size::CaptionButtonW, Size::MenuTitleH, Size::MenuItemH, Size::MenuMinW,
        Size::SidebarItemH, Size::SidebarOpenBtnH, Size::IconButton, Size::IconGlyph,
        Size::PlayDeck, Size::PlayNowBar, Size::PausedBadge, Size::NowPlayingBarH,
        Size::SeekTrack, Size::SeekTrackHover, Size::SeekKnob, Size::SeekKnobHover, Size::VolumeW,
        Size::PlaylistRowH, Size::PlaylistL, Size::PlaylistM, Size::PlaylistS,
        Size::ButtonH, Size::InputH, Size::UrlInputH, Size::SelectH,
        Size::ToggleW, Size::ToggleH, Size::ChipH,
        Size::RadiusXs, Size::RadiusSm, Size::RadiusMd, Size::RadiusLg, Size::RadiusXl, Size::RadiusTile,
    };
    static_assert(sizeof(sizes) / sizeof(sizes[0]) == Theme::SizeRoleCount,
                  "SizeRole and the size table differ");

    /* Same order as Theme::MotionRole */
    const int durations[] = {
        Motion::MenuDrop, Motion::Popover, Motion::ContextMenu, Motion::DialogBackdrop,
        Motion::Dialog, Motion::PanelSlide, Motion::ViewChange, Motion::Toast, Motion::ToastHold,
        Motion::PausedBadge, Motion::MiniPlayer, Motion::DropTarget, Motion::SeekTick,
        Motion::VolumeFill, Motion::EqBand, Motion::Toggle, Motion::PlayPress,
        Motion::ImmersiveIdle, Motion::ImmersiveFade,
        VideoOverlay::RingTurn, NowPlaying::SpectrumMotion,
    };
    static_assert(sizeof(durations) / sizeof(durations[0]) == Theme::MotionRoleCount,
                  "MotionRole and the duration table differ");

    QString firstInstalled(const char *families)
    {
        const QStringList installed = QFontDatabase().families();
        const QStringList wanted = QString::fromLatin1(families).split(',');
        foreach (const QString &f, wanted)
            if (installed.contains(f, Qt::CaseInsensitive))
                return f;
        return wanted.last();
    }

    QString css(const QColor &c)
    {
        return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green())
                                           .arg(c.blue()).arg(c.alpha());
    }

    bool systemHighContrast()
    {
#ifdef _WIN32
        HIGHCONTRASTW hc;
        ZeroMemory(&hc, sizeof(hc));
        hc.cbSize = sizeof(hc);
        return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)
            && (hc.dwFlags & HCF_HIGHCONTRASTON);
#else
        return false;
#endif
    }

    /* A redesign icon recoloured with tint; the cone keeps its colours. */
    QPixmap tintedIcon(const QString &name, const QColor &tint, const QSize &size)
    {
        QPixmap pixmap = ImageHelper::loadSvgToPixmap(
            QString(":/redesign/%1.svg").arg(name), size.width(), size.height());
        if (name != QLatin1String(RedesignIcons::IconCone))
        {
            QPainter painter(&pixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(QRect(QPoint(0, 0), pixmap.size()), tint);
        }
        return pixmap;
    }

    /* Stylesheet `image:` needs a file: write the tinted icon, at twice the
     * glyph size so it stays sharp at 200 %, to the temporary directory. The
     * colour is in the name, so a theme change gets a new url, not a stale
     * cached image; the version too, so another version's file is not reused. */
    QString tintedIconFile(intf_thread_t *p_intf, const char *name,
                           const QColor &tint, int rotation)
    {
        const QString path = QDir::temp().filePath(
            QString("vlc-qt-%1-%2-%3-%4.png").arg(QLatin1String(PACKAGE_VERSION))
                .arg(QLatin1String(name))
                .arg(rotation).arg(tint.rgba(), 8, 16, QLatin1Char('0')));
        if (QFile::exists(path))
            return path;
        const int px = 2 * Size::IconGlyph;
        QImage image = tintedIcon(QLatin1String(name), tint, QSize(px, px)).toImage();
        if (rotation)
            image = image.transformed(QTransform().rotate(rotation));
        QSaveFile file(path);  /* atomic: another VLC may read it meanwhile */
        if (file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit())
            return path;
        msg_Warn(p_intf, "theme: cannot write %s", qtu(path));
        return QString(":/redesign/%1.svg").arg(QLatin1String(name));
    }

    const char altSeenProperty[] = "vlcAltSeen";
    const char destructiveProperty[] = "vlcDestructive"; /* Theme::setDestructive */
    /* the knob's place while it slides (0 off, 1 on); unset at rest */
    const char toggleKnobProperty[] = "vlcToggleKnob";
    const char toggleHookedProperty[] = "vlcToggleHooked";

    /* DESIGN_SPEC 3: a QCheckBox with role="toggle" is drawn as a switch;
     * it stays a check box for the keyboard and accessibility */
    bool isToggle(const QWidget *w)
    {
        return qobject_cast<const QCheckBox *>(w)
            && w->property("role").toString() == QLatin1String("toggle");
    }

    /* What the stylesheet cannot do (DESIGN_SPEC 7, 11): menu items whose
     * check is the accent `check` icon and whose shortcut column is dimmed,
     * and check boxes and radio buttons with an outline that stays visible
     * in dark mode. The stylesheet hands these to its base style, this one,
     * because it styles neither QMenu::item nor ::indicator. */
    class ThemeStyle : public QProxyStyle
    {
    public:
        explicit ThemeStyle(QStyle *base) : QProxyStyle(base) {}

        QSize sizeFromContents(ContentsType type, const QStyleOption *opt,
                               const QSize &size, const QWidget *w) const Q_DECL_OVERRIDE
        {
            const QStyleOptionMenuItem *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
            /* Combo box pop-ups ask for CT_MenuItem too, but pass the whole
             * view's size, not the label's: leave those to the base style. */
            if (type != CT_MenuItem || !mi || mi->menuItemType == QStyleOptionMenuItem::Separator
             || !qobject_cast<const QMenu *>(w) || !Theme::instance())
                return QProxyStyle::sizeFromContents(type, opt, size, w);
            const Theme *t = Theme::instance();
            /* QMenu adds the shortcut column width (tabWidth) itself */
            int width = labelX(t) + size.width() + t->px(Size::MenuTitlePadX);
            if (mi->text.contains(QLatin1Char('\t')))
                width += 2 * t->px(Size::MenuTitlePadX);
            if (mi->menuItemType == QStyleOptionMenuItem::SubMenu)
                width += t->px(Size::IconGlyph);
            return QSize(width, qMax(size.height(), t->metric(Theme::MenuItemH)));
        }

        void drawControl(ControlElement element, const QStyleOption *opt,
                         QPainter *p, const QWidget *w) const Q_DECL_OVERRIDE
        {
            const QStyleOptionMenuItem *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(opt);
            Theme *t = Theme::instance();
            if (element != CE_MenuItem || !mi || !t
             || mi->menuItemType == QStyleOptionMenuItem::Separator)
            {
                QProxyStyle::drawControl(element, opt, p, w);
                return;
            }
            const bool enabled = mi->state & State_Enabled;
            const QRect r = mi->rect;
            const Qt::LayoutDirection dir = mi->direction;
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if ((mi->state & State_Selected) && enabled)
            {
                p->setPen(Qt::NoPen);
                p->setBrush(t->ink(InkAlpha::HoverFill));
                p->drawRoundedRect(r, t->metric(Theme::RadiusSm), t->metric(Theme::RadiusSm));
            }

            /* icon column: checked items show the accent check instead */
            const int padX = t->px(Size::MenuTitlePadX);
            const int iconPx = proxy()->pixelMetric(PM_SmallIconSize, opt, w);
            const QSize iconSize(iconPx, iconPx);
            QPixmap pix;
            if (mi->checkType != QStyleOptionMenuItem::NotCheckable && mi->checked)
                pix = t->icon(RedesignIcons::IconCheck, enabled ? strongAccent(t) : Theme::Dim,
                              iconSize).pixmap(iconSize);
            else if (!mi->icon.isNull())
                pix = mi->icon.pixmap(iconSize, !enabled ? QIcon::Disabled
                                      : (mi->state & State_Selected) ? QIcon::Active : QIcon::Normal);
            if (!pix.isNull())
                drawItemPixmap(p, visualRect(dir, r, QRect(r.x() + padX, r.y(),
                                                           t->px(Size::MenuIconCol), r.height())),
                               Qt::AlignCenter, pix);

            const int arrowW = mi->menuItemType == QStyleOptionMenuItem::SubMenu ? t->px(Size::IconGlyph) : 0;
            const QRect textRect = visualRect(dir, r, QRect(r.x() + labelX(t), r.y(),
                r.width() - labelX(t) - padX - arrowW, r.height()));
            int flags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
            if (!proxy()->styleHint(SH_UnderlineShortcut, mi, w))
                flags |= Qt::TextHideMnemonic;
            QString label = mi->text;
            const int tab = label.indexOf(QLatin1Char('\t'));
            if (tab >= 0)
            {
                p->setPen(t->color(Theme::Mute));
                p->setFont(t->font(Theme::Meta));
                p->drawText(textRect, flags | visualAlignment(dir, Qt::AlignRight), label.mid(tab + 1));
                label.truncate(tab);
            }
            const QMenu *menu = qobject_cast<const QMenu *>(w);
            const QAction *action = menu ? menu->actionAt(r.center()) : NULL;
            p->setPen(t->color(!enabled ? Theme::Dim
                               : action && action->property(destructiveProperty).toBool()
                               ? Theme::DestructiveText : Theme::Text));
            p->setFont(mi->font);
            p->drawText(textRect, flags | visualAlignment(dir, Qt::AlignLeft), label);

            if (arrowW)
            {
                QPixmap arrow = t->icon(RedesignIcons::IconSubmenu, enabled ? Theme::Text2 : Theme::Dim,
                                        iconSize).pixmap(iconSize);
                if (dir == Qt::RightToLeft)
                    arrow = QPixmap::fromImage(arrow.toImage().mirrored(true, false));
                drawItemPixmap(p, visualRect(dir, r, QRect(r.right() + 1 - padX - arrowW,
                                                           r.y(), arrowW, r.height())),
                               Qt::AlignCenter, arrow);
            }
            p->restore();
        }

        void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt, QPainter *p,
                           const QWidget *w) const Q_DECL_OVERRIDE
        {
            Theme *t = Theme::instance();
            if (t && isToggle(w) && (pe == PE_FrameFocusRect || pe == PE_IndicatorCheckBox))
            {
                if (pe == PE_IndicatorCheckBox)
                    drawToggle(t, opt, p, w);
                return;  /* the switch draws its own focus ring */
            }
            if (pe == PE_FrameFocusRect && t
             && (qobject_cast<const QCheckBox *>(w) || qobject_cast<const QRadioButton *>(w)))
            {
                /* a 2 px accent ring like the other controls, not Fusion's faint box */
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                const qreal inset = Size::FocusRing / 2.;
                p->setPen(QPen(t->color(strongAccent(t)), Size::FocusRing));
                p->setBrush(Qt::NoBrush);
                p->drawRoundedRect(QRectF(opt->rect).adjusted(inset, inset, -inset, -inset),
                                   t->metric(Theme::RadiusXs), t->metric(Theme::RadiusXs));
                p->restore();
                return;
            }
            if ((pe != PE_IndicatorCheckBox && pe != PE_IndicatorRadioButton) || !t)
            {
                QProxyStyle::drawPrimitive(pe, opt, p, w);
                return;
            }
            const bool on = opt->state & (State_On | State_NoChange);
            const QRectF r = QRectF(opt->rect).adjusted(0.5, 0.5, -0.5, -0.5);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if (!(opt->state & State_Enabled))
                p->setOpacity(0.35);  /* DESIGN_SPEC 3: disabled controls */
            /* unchecked: ToggleOff outline, at least 3:1 against the window */
            p->setPen(on ? t->color(Theme::Accent) : t->ink(InkAlpha::ToggleOff));
            p->setBrush(on ? t->color(Theme::Accent) : t->color(Theme::Surface));
            if (pe == PE_IndicatorRadioButton)
            {
                p->drawEllipse(r);
                if (on)
                {
                    p->setPen(Qt::NoPen);
                    p->setBrush(t->color(Theme::OnAccent));
                    p->drawEllipse(r.center(), r.width() / 6, r.height() / 6);
                }
            }
            else
            {
                p->drawRoundedRect(r, t->metric(Theme::RadiusXs), t->metric(Theme::RadiusXs));
                if (opt->state & State_NoChange)
                {
                    p->setPen(QPen(t->color(Theme::OnAccent), 2));
                    p->drawLine(QPointF(r.left() + r.width() / 4, r.center().y()),
                                QPointF(r.right() - r.width() / 4, r.center().y()));
                }
                else if (on)
                    p->drawPixmap(opt->rect, t->icon(RedesignIcons::IconCheck, Theme::OnAccent,
                                                     opt->rect.size()).pixmap(opt->rect.size()));
            }
            p->restore();
        }

        /* Windows shows access-key underlines only once Alt was pressed in
         * the window (or always, with the keyboard cues setting); Fusion
         * always does. Theme::eventFilter records the Alt press. */
        int styleHint(StyleHint hint, const QStyleOption *opt, const QWidget *w,
                      QStyleHintReturn *ret) const Q_DECL_OVERRIDE
        {
            if (hint != SH_UnderlineShortcut)
                return QProxyStyle::styleHint(hint, opt, w, ret);
#ifdef _WIN32
            BOOL cues = FALSE;
            if (SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &cues, 0) && cues)
                return 1;
            /* a pop-up menu belongs to the window that opened it */
            const QWidget *window = qobject_cast<const QMenu *>(w)
                                  ? QApplication::activeWindow() : w ? w->window() : NULL;
            return window && window->property(altSeenProperty).toBool();
#else
            return QProxyStyle::styleHint(hint, opt, w, ret);
#endif
        }

        /* the switch, with room around it for its focus ring */
        int pixelMetric(PixelMetric m, const QStyleOption *opt, const QWidget *w) const Q_DECL_OVERRIDE
        {
            const Theme *t = Theme::instance();
            if (t && (m == PM_IndicatorWidth || m == PM_IndicatorHeight) && isToggle(w))
                return t->metric(m == PM_IndicatorWidth ? Theme::ToggleW : Theme::ToggleH)
                     + 4 * Size::FocusRing;
            return QProxyStyle::pixelMetric(m, opt, w);
        }

        using QProxyStyle::polish;
        /* Motion::Toggle: the knob slides; at once when animations are off */
        void polish(QWidget *w) Q_DECL_OVERRIDE
        {
            QProxyStyle::polish(w);
            if (!isToggle(w) || w->property(toggleHookedProperty).toBool())
                return;
            w->setProperty(toggleHookedProperty, true);
            QCheckBox *box = static_cast<QCheckBox *>(w);
            QObject::connect(box, &QAbstractButton::toggled, box, [box](bool on) {
                const Theme *t = Theme::instance();
                const int ms = t ? t->duration(Theme::Toggle) : 0;
                const QVariant from = box->property(toggleKnobProperty);
                foreach (QVariantAnimation *old, box->findChildren<QVariantAnimation *>(
                             QString(), Qt::FindDirectChildrenOnly))
                    delete old;
                if (ms <= 0)
                {
                    box->setProperty(toggleKnobProperty, QVariant());
                    return;
                }
                QVariantAnimation *a = new QVariantAnimation(box);
                a->setStartValue(from.isValid() ? from.toReal() : on ? 0. : 1.);
                a->setEndValue(on ? 1. : 0.);
                a->setDuration(ms);
                a->setEasingCurve(QEasingCurve::OutCubic);
                QObject::connect(a, &QVariantAnimation::valueChanged, box, [box](const QVariant &v) {
                    box->setProperty(toggleKnobProperty, v);
                    box->update();
                });
                QObject::connect(a, &QAbstractAnimation::finished, box, [box]() {
                    box->setProperty(toggleKnobProperty, QVariant());
                    box->update();
                });
                a->start(QAbstractAnimation::DeleteWhenStopped);
            });
        }

    private:
        /* The prototype's switch: off, an outlined track and a Text2 knob;
         * on, an accent track and an OnAccent knob */
        static void drawToggle(Theme *t, const QStyleOption *opt, QPainter *p, const QWidget *w)
        {
            const bool on = opt->state & (State_On | State_NoChange);
            const QVariant knob = w->property(toggleKnobProperty);
            const qreal at = knob.isValid() ? knob.toReal() : on ? 1. : 0.;
            const QRectF ring = QRectF(opt->rect).adjusted(Size::FocusRing / 2., Size::FocusRing / 2.,
                                                           -Size::FocusRing / 2., -Size::FocusRing / 2.);
            const QRectF track = QRectF(opt->rect).adjusted(2 * Size::FocusRing + .5, 2 * Size::FocusRing + .5,
                                                            -2 * Size::FocusRing - .5, -2 * Size::FocusRing - .5);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if (opt->state & State_HasFocus)
            {
                p->setPen(QPen(t->color(strongAccent(t)), Size::FocusRing));
                p->setBrush(Qt::NoBrush);
                p->drawRoundedRect(ring, ring.height() / 2, ring.height() / 2);
            }
            if (!(opt->state & State_Enabled))
                p->setOpacity(0.35);  /* DESIGN_SPEC 3: disabled controls */
            p->setPen(on ? t->color(Theme::Accent) : t->ink(InkAlpha::ToggleOff));
            p->setBrush(on ? t->color(Theme::Accent) : QColor(Qt::transparent));
            p->drawRoundedRect(track, track.height() / 2, track.height() / 2);
            const qreal d = t->px(Size::ToggleKnob);
            const qreal inset = (track.height() - d) / 2;
            const qreal x = track.left() + inset + at * (track.width() - d - 2 * inset);
            p->setPen(Qt::NoPen);
            p->setBrush(t->color(on ? Theme::OnAccent : Theme::Text2));
            p->drawEllipse(QRectF(x, track.top() + inset, d, d));
            p->restore();
        }

        /* The accent is 2.4:1 on the light theme's white: marks and focus
         * rings take the darker accent text colour there (>= 3:1). */
        static Theme::ColorRole strongAccent(const Theme *t)
        {
            return t->isDark() ? Theme::Accent : Theme::AccentText;
        }
        /* the label starts after the 22 px icon column */
        static int labelX(const Theme *t)
        {
            return 2 * t->px(Size::MenuTitlePadX) + t->px(Size::MenuIconCol);
        }
    };
}

Theme::Theme(intf_thread_t *_p_intf)
    : p_intf(_p_intf), dark(false), highContrast(false), scale(1.)
{
    assert(s_instance == NULL);
    s_instance = this;
}

Theme::~Theme()
{
    s_instance = NULL;
}

bool Theme::readDark() const
{
    switch (var_InheritInteger(p_intf, "qt-redesign-theme"))
    {
    case Dark:
        return true;
    case Light:
        return false;
    default:
        /* configurations from before the redesign keep their dark palette */
        if (var_InheritBool(p_intf, "qt-dark-palette"))
            return true;
#ifdef _WIN32
        /* AppsUseLightTheme == 0 means dark; a missing value (before 1809) means light */
        return QVLCRegistry(HKEY_CURRENT_USER).ReadRegistry(
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   "AppsUseLightTheme", 1) == 0;
#else
        return false;
#endif
    }
}

void Theme::apply(QApplication &app)
{
    highContrast = systemHighContrast();
    dark = readDark();
    /* Qt 5.6 scales by whole device pixel ratios only (1 at 125 %) while
     * fonts in points follow the logical dpi: the metrics take its factor */
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (screen)
        scale = screen->logicalDotsPerInchY() / 96.;
    msg_Dbg(p_intf, "theme: logical dpi %.0f, device pixel ratio %.2f, scale %.3f",
            screen ? screen->logicalDotsPerInchY() : 0., app.devicePixelRatio(), scale);
    uiFamily = firstInstalled(Type::UiFamilies);
    monoFamily = firstInstalled(Type::MonoFamilies);

    /* The user's style choice from the preferences wins over Fusion. */
    const QString style = getSettings()->value("MainWindow/QtStyle", "").toString();

    setBaseStyle(style);
    if (highContrast)
    {
        /* Keep the system style and colours; only report light or dark. */
        dark = app.palette().color(QPalette::Window).lightness() < 128;
        msg_Dbg(p_intf, "theme: high contrast, keeping the system colours");
    }
    else
    {
        app.setFont(font(Body));
        msg_Dbg(p_intf, "theme: %s, font %s", dark ? "dark" : "light", qtu(uiFamily));
    }
#ifdef _WIN32
    app.installEventFilter(this);
#endif
}

void Theme::setBaseStyle(const QString &name)
{
    if (highContrast)
    {
        if (!name.isEmpty())
            QApplication::setStyle(name);
        return;
    }
    /* Empty, or a name the factory does not know (the translated "System's
     * default" older versions stored), means Fusion: with NULL the proxy
     * would take the platform style, which ignores the token palette. */
    QStyle *base = name.isEmpty() ? NULL : QStyleFactory::create(name);
    if (!base)
        base = QStyleFactory::create(QStringLiteral("Fusion"));
    QApplication::setStyle(new ThemeStyle(base));
    applyPalette();
    qApp->setStyleSheet(styleSheet());
}

void Theme::reload()
{
    if (highContrast)
        return;  /* shortcut: high contrast is read once at start, restart to leave or enter it */
    const bool wasDark = dark;
    dark = readDark();
    msg_Dbg(p_intf, "theme: reload, %s -> %s", wasDark ? "dark" : "light",
            dark ? "dark" : "light");
    if (dark == wasDark)
        return;
    icons.clear();
    applyPalette();
    qApp->setStyleSheet(styleSheet());
#ifdef _WIN32
    foreach (QWidget *w, QApplication::topLevelWidgets())
        if (w->testAttribute(Qt::WA_WState_Created))
            applyWindowFrame(w);
#endif
    emit changed();
}

const Palette &Theme::palette() const
{
    return dark ? RedesignTokens::Dark : RedesignTokens::Light;
}

QColor Theme::color(ColorRole role) const
{
    QColor c;
    if (role < paletteCount)
        c = QColor::fromRgba(palette().*paletteFields[role]);
    else if (role < ColorRoleCount)
        c = QColor::fromRgba(fixedColors[role - paletteCount]);
    if (!highContrast || !c.isValid())
        return c;

    /* high contrast: painted widgets take the system colours, at the token's alpha */
    QPalette::ColorGroup group = QPalette::Active;
    QPalette::ColorRole sys;
    switch (role)
    {
    case Text: case Text2: case Ink:
        sys = QPalette::WindowText; break;
    case Mute: case Dim:
        group = QPalette::Disabled; sys = QPalette::Text; break;
    case Surface:
        sys = QPalette::Base; break;
    case AccentText: case Accent: case AccentHover: case AccentTint12: case AccentTint14:
    case AccentBorder50: case AccentFocusRing: case AccentKnobHalo:
    case VolumeBoost:
        sys = QPalette::Highlight; break;
    case OnAccent: case SeekKnobFill:  /* both sit on the accent */
        sys = QPalette::HighlightedText; break;
    case Error: case ErrorTint: case DestructiveText: case WindowCloseHover: case WindowCloseGlyph:
    case VideoSurface: case PausedBadgeBg: case SubtitleBox: case OnVideo:
        return c;  /* status and video colours keep their meaning */
    default:       /* the backgrounds: Page .. Hover2, Glass, FxBg, stripes */
        sys = QPalette::Window; break;
    }
    QColor s = QApplication::palette().color(group, sys);
    s.setAlpha(c.alpha());
    return s;
}

QColor Theme::ink(int alpha) const
{
    QColor c = highContrast ? QApplication::palette().color(QPalette::WindowText)
                            : QColor::fromRgb(palette().ink);
    c.setAlpha(alpha);
    return c;
}

QFont Theme::font(TypeRole role) const
{
    static const int px[] = { Type::Display, Type::PageTitle, Type::PanelTitle,
                              Type::Body, Type::Meta, Type::Overline, Type::Mono,
                              Type::NavTitle };
    QFont f(role == Mono ? monoFamily : uiFamily);
    /* points follow the Windows text scale; tokens are px at 96 dpi */
    f.setPointSizeF(px[role] * 72.0 / 96.0);
    if (role == Display || role == PageTitle || role == PanelTitle || role == Overline
        || role == NavTitle)
        f.setWeight(QFont::DemiBold);
    return f;
}

int Theme::metric(SizeRole role) const
{
    return role >= 0 && role < SizeRoleCount ? px(sizes[role]) : 0;
}

int Theme::duration(MotionRole role) const
{
#ifdef _WIN32
    BOOL animate = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0) && !animate)
        return 0;
#endif
    return role >= 0 && role < MotionRoleCount ? durations[role] : 0;
}

QIcon Theme::icon(const QString &name, ColorRole role, const QSize &requested)
{
    const QSize size = requested.isEmpty()
                     ? QSize(px(Size::IconGlyph), px(Size::IconGlyph)) : requested;
    const QColor tint = color(role);
    const QString key = QString("%1|%2|%3x%4").arg(name).arg(tint.rgba())
                                              .arg(size.width()).arg(size.height());
    QHash<QString, QIcon>::const_iterator it = icons.constFind(key);
    if (it != icons.constEnd())
        return it.value();

    const QString path = QString(":/redesign/%1.svg").arg(name);
    QIcon result;
    if (!QFile::exists(path))
        msg_Warn(p_intf, "theme: missing icon %s", qtu(path));
    else
        result = QIcon(tintedIcon(name, tint, size));
    icons.insert(key, result);  /* also caches a miss, so it warns once */
    return result;
}

namespace
{
    class ThemedIconEngine : public QIconEngine
    {
    public:
        ThemedIconEngine(const char *_name, Theme::ColorRole _role)
            : name(QLatin1String(_name)), role(_role) {}

        QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE
        {
            Theme *t = Theme::instance();
            return t ? t->icon(name, role, size).pixmap(size, mode, state) : QPixmap();
        }
        void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE
        {
            const qreal dpr = p->device()->devicePixelRatioF();
            QPixmap pix = pixmap(rect.size() * dpr, mode, state);
            pix.setDevicePixelRatio(dpr);
            p->drawPixmap(rect, pix);
        }
        QIconEngine *clone() const Q_DECL_OVERRIDE
        {
            return new ThemedIconEngine(*this);
        }

    private:
        QString name;
        Theme::ColorRole role;
    };
}

QIcon Theme::themedIcon(const char *name, ColorRole role)
{
    return QIcon(new ThemedIconEngine(name, role));
}

void Theme::setDestructive(QAction *action)
{
    action->setProperty(destructiveProperty, true);
}

void Theme::applyPalette()
{
    const Palette &p = palette();
    QPalette pal(QColor::fromRgb(p.raised), QColor::fromRgb(p.bg));
    pal.setColor(QPalette::WindowText, QColor::fromRgb(p.tx));
    pal.setColor(QPalette::Base, QColor::fromRgb(p.surface));
    pal.setColor(QPalette::AlternateBase, QColor::fromRgb(p.panel));
    pal.setColor(QPalette::ToolTipBase, QColor::fromRgb(p.raised));
    pal.setColor(QPalette::ToolTipText, QColor::fromRgb(p.tx));
    pal.setColor(QPalette::Text, QColor::fromRgb(p.tx));
    pal.setColor(QPalette::ButtonText, QColor::fromRgb(p.tx));
    pal.setColor(QPalette::BrightText, QColor::fromRgb(p.tx));
    pal.setColor(QPalette::Highlight, color(Accent));
    pal.setColor(QPalette::HighlightedText, color(OnAccent));
    pal.setColor(QPalette::Link, QColor::fromRgb(p.accTx));
    pal.setColor(QPalette::LinkVisited, QColor::fromRgb(p.accTx));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor::fromRgb(p.dim));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor::fromRgb(p.dim));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor::fromRgb(p.dim));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor::fromRgb(p.hover2));
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor::fromRgb(p.dim));
    QApplication::setPalette(pal);
}

/* The application stylesheet, generated from tokens only. Sizes stay those
 * of the existing layouts (no fixed heights) except menus; the redesigned
 * widgets of later phases add [role="..."] rules. SeekSlider and SoundSlider
 * paint themselves: `.QSlider` matches the exact class, not subclasses. */
QString Theme::styleSheet() const
{
    static const char qss[] = R"QSS(
QMenuBar { background: @chrome@; color: @tx2@; }
QMenuBar::item { background: transparent; padding: @menuTitlePadY@px @menuTitlePadX@px; border-radius: @radiusSm@px; }
QMenuBar::item:selected { background: @hoverFill@; color: @tx@; }
QMenuBar::item:pressed { background: @openFill@; color: @tx@; }

QMenu { background: @raised@; color: @tx@; border: @popupBorder@; padding: @menuPad@px; min-width: @menuMinW@px;
    -qt-style-features: background-color; }
QMenu::separator { height: 1px; background: @border@; margin: @menuPad@px @menuSepX@px; }

QToolTip { background: @raised@; color: @tx@; border: @popupBorder@; padding: @menuPad@px @menuTitlePadX@px; }

QPushButton { background: @raised@; color: @tx@; border: 1px solid @outline@; border-radius: @radiusMd@px; padding: @menuPad@px @buttonPadX@px; }
QPushButton:hover { background: @hover@; }
QPushButton:pressed, QPushButton:checked { background: @accentTint@; border-color: @accentBorder@; }
QPushButton:default { background: @accent@; color: @onAccent@; border-color: @accent@; }
QPushButton:default:hover { background: @accentHover@; }
QPushButton:disabled { background: @panel@; color: @dim@; border-color: @divider@; }

QToolButton { background: transparent; border: 2px solid transparent; border-radius: @radiusMd@px; }
QToolButton:hover { background: @hoverFill@; }
QToolButton:pressed { background: @openFill@; }
QToolButton:checked { background: @accentTint@; }
QToolButton[popupMode="1"] { padding-right: @iconGlyph@px; }
QToolButton::menu-button { background: transparent; border: none; width: @iconGlyph@px; }
QToolButton::menu-arrow { image: url("@arrowDown@"); width: @iconGlyph@px; height: @iconGlyph@px; }
QToolButton::menu-arrow:disabled { image: url("@arrowDownDim@"); }

QLineEdit, QAbstractSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
    background: @surface@; color: @tx@; border: 1px solid @outline@; border-radius: @radiusMd@px;
    selection-background-color: @accent@; selection-color: @onAccent@; }
QLineEdit, QAbstractSpinBox { padding: 1px @menuPad@px; }
/* a combo's vertical padding becomes its pop-up's PM_MenuVMargin, which Qt 5.6
   leaves out of the pop-up height (one row short, with a scroller): use margin */
QComboBox { margin: 1px 0; padding: 0 @menuPad@px; }
QLineEdit:hover, QAbstractSpinBox:hover, QComboBox:hover { border-color: @toggleOff@; }
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled { background: @panel@; color: @dim@; border-color: @divider@; }
QComboBox::drop-down, QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: none; background: transparent; width: @iconGlyph@px; }
QComboBox::down-arrow { width: @iconGlyph@px; height: @iconGlyph@px; }
QAbstractSpinBox::up-arrow, QAbstractSpinBox::down-arrow { width: @iconGlyph@px; height: @halfGlyph@px; }
QComboBox::down-arrow, QAbstractSpinBox::down-arrow { image: url("@arrowDown@"); }
QAbstractSpinBox::up-arrow { image: url("@arrowUp@"); }
QComboBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off { image: url("@arrowDownDim@"); }
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off { image: url("@arrowUpDim@"); }
QComboBox QAbstractItemView { background: @raised@; border: @popupBorder@; selection-background-color: @hoverFill@; selection-color: @tx@; outline: 0; }

QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus { border: 2px solid @focusRing@; padding: 0px @menuPadFocus@px; }
QComboBox:focus { margin: 0; }
QTextEdit:focus, QPlainTextEdit:focus, QPushButton:focus { border: 2px solid @focusRing@; }
QToolButton:focus { border-color: @focusRing@; }
QPushButton:focus { padding: @menuPadFocus@px @buttonPadXFocus@px; }

QTabWidget::pane { background: @panel@; border: 1px solid @divider@; border-radius: @radiusLg@px; top: -1px; }
QTabBar::tab { background: transparent; color: @mute@; border: none; border-bottom: 2px solid transparent; padding: @menuItemPadY@px @buttonPadX@px; }
QTabBar::tab:hover { color: @tx@; }
QTabBar::tab:selected { color: @tx@; border-bottom-color: @accent@; }
QTabBar::tab:focus { color: @tx@; background: @hoverFill@; }

QGroupBox { border: 1px solid @divider@; border-radius: @radiusLg@px; margin-top: @groupTitleH@px; padding-top: @menuPad@px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: @menuTitlePadX@px; padding: 0 @menuPad@px; color: @mute@; }

QAbstractItemView { background: @surface@; alternate-background-color: @panel@; border: 1px solid @divider@;
    selection-background-color: @accentTint@; selection-color: @tx@; }
QAbstractItemView::item:hover { background: @hoverFill@; }
QAbstractItemView::item:selected { background: @accentTint@; color: @tx@; }
QHeaderView::section { background: @panel@; color: @mute@; border: none; border-bottom: 1px solid @divider@; border-right: 1px solid @divider@; padding: @menuPad@px @menuTitlePadX@px; }

QScrollBar:vertical { background: transparent; width: @scrollW@px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: @scrollW@px; margin: 0; }
QScrollBar::handle { background: @toggleOff@; border-radius: @radiusXs@px; margin: 2px; }
QScrollBar::handle:hover { background: @mute@; }
QScrollBar::handle:vertical { min-height: @iconGlyph@px; }
QScrollBar::handle:horizontal { min-width: @iconGlyph@px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QSplitter::handle { background: @divider@; }
QStatusBar { background: @chrome@; color: @mute@; border-top: 1px solid @divider@; }
QStatusBar::item { border: none; }

QLabel[role="hint"] { color: @mute@; }

.QSlider::groove:horizontal { height: @track@px; background: @trackInk@; border-radius: @trackRadius@px; }
.QSlider::groove:vertical { width: @track@px; background: @trackInk@; border-radius: @trackRadius@px; }
.QSlider::sub-page:horizontal, .QSlider::add-page:vertical { background: @accent@; border-radius: @trackRadius@px; }
.QSlider::handle:horizontal { width: @knob@px; margin: @knobMargin@px 0; border-radius: @knobRadius@px; background: @accent@; }
.QSlider::handle:vertical { height: @knob@px; margin: 0 @knobMargin@px; border-radius: @knobRadius@px; background: @accent@; }
.QSlider::handle:hover { background: @accentHover@; }
.QSlider::sub-page:horizontal:disabled, .QSlider::add-page:vertical:disabled, .QSlider::handle:disabled { background: @toggleOff@; }
.QSlider { border: 2px solid transparent; border-radius: @radiusSm@px; }
.QSlider:focus { border-color: @focusRing@; }

/* ---- Title bar (components/title_bar.cpp) ---- */
QToolButton[role="caption"] { border-radius: 0; }
QToolButton[role="caption"][nchover="true"] { background: @hoverFill@; }
QToolButton#captionClose:hover, QToolButton#captionClose:pressed { background: @closeHover@; }

/* Phase 3: playlist panel (components/playlist/playlist_panel.cpp) */
#playlistPanel QListView { background: transparent; border: none; outline: 0; }
/* Phase 3: now playing, mini player, audio mode (components/now_playing.cpp) */
QLabel[np="mute"] { color: @mute@; }
QLabel[np="accent"] { color: @accTx@; }  /* the accent is 2.4:1 on light surfaces */
QLabel[np="chip"] { background: @accentTint@; color: @accTx@; border-radius: @radiusXs@px; padding: @chipPadY@px @chipPadX@px; }
QLabel[np="chip2"] { background: @divider@; color: @tx2@; border-radius: @radiusXs@px; padding: @chipPadY@px @chipPadX@px; }
QToolButton[np="play"] { background: @accent@; border-radius: @playNowR@px; }
QToolButton[np="play"]:hover { background: @accentHover@; }
QToolButton[np="play"]:focus { border-color: @onAccent@; }
QToolButton[np="pill"] { color: @tx2@; padding: 0 @pillPadR@px 0 @pillPadL@px; }
QToolButton[np="overlay"] { background: @scrim@; }
QListView[np="tracks"] { background: transparent; border: none; }
/* Phase 3: deck (components/controller.cpp, deck_popover.cpp). Roles:
   deck = icon button (on = checked, open = [open="true"]), play = accent circle,
   speed = the speed label, deckSeparator, deckTime, popover, popoverDivider, popoverLink. */
QToolButton[role="deck"], QToolButton[role="speed"] { background: transparent; border: 2px solid transparent; border-radius: @radiusMd@px; padding: 0; }
QToolButton[role="deck"]:hover, QToolButton[role="speed"]:hover { background: @hoverFill@; }
QToolButton[role="deck"]:pressed, QToolButton[role="speed"]:pressed { background: @openFill@; }
QToolButton[role="deck"] { color: @tx@; font-weight: 600; }
QToolButton[role="deck"]:disabled { color: @dim@; }
QToolButton[role="deck"]:checked { background: transparent; color: @accTx@; }
QToolButton[role="deck"][record="true"]:checked { background: @errorTint@; }
QToolButton[role="deck"]:checked:hover { background: @hoverFill@; }
QToolButton[role="deck"][open="true"], QToolButton[role="speed"][open="true"] { background: @openFill@; }
QToolButton[role="deck"]:focus, QToolButton[role="speed"]:focus { border-color: @focusRing@; }
QToolButton[role="speed"] { color: @tx2@; font-weight: 600; padding: 0 @speedPadX@px; }
QToolButton[role="speed"][on="true"], QToolButton[role="speed"][open="true"] { color: @accTx@; }
QToolButton[role="speed"]:disabled { color: @dim@; }
QToolButton[role="speed"]::menu-indicator { image: none; width: 0; }
QToolButton[role="play"] { background: @accent@; border: 2px solid transparent; border-radius: @playRadius@px; padding: 0; }
QToolButton[role="play"]:hover { background: @accentHover@; }
QToolButton[role="play"]:pressed { background: @accent@; }
QToolButton[role="play"]:focus { border-color: @tx@; }
QFrame[role="deckSeparator"] { background: @separatorInk@; border: none; margin: 0 @deckSepMargin@px; }
QLabel[role="deckTime"] { color: @tx2@; }
QFrame[role="popover"] { background: @raised@; border: @popupBorder@; }
QFrame[role="popover"] QListView { background: transparent; border: none; }
QFrame[role="popover"] QLabel { color: @dim@; }
QFrame[role="popoverDivider"] { background: @divider@; border: none; }
QPushButton[role="popoverLink"] { background: transparent; color: @accTx@; border: 2px solid transparent; border-top: 1px solid @divider@;
    border-radius: 0; text-align: left; padding: @menuPad@px; }
QPushButton[role="popoverLink"]:hover { background: @hoverFill@; }
QPushButton[role="popoverLink"]:focus { border: 2px solid @focusRing@; border-radius: @radiusMd@px; }
/* Phase 4: home (components/library/home_page.cpp); the delegate draws the row focus ring */
QTreeView[role="homeRecent"] { background: transparent; border: none; outline: 0; }
QTreeView[role="homeRecent"]::item { padding: 0 @homeCellPad@px; border-bottom: 1px solid @homeRowLine@; }
QTreeView[role="homeRecent"]::item:first { padding-left: @homeRowPad@px; }
QTreeView[role="homeRecent"]::item:last { padding-right: @homeRowPad@px; }
/* not :default, which also names the button "... Enter" to screen readers */
QPushButton[role="primary"] { background: @accent@; color: @onAccent@; border-color: @accent@; font-weight: 600; }
QPushButton[role="primary"]:hover { background: @accentHover@; }
/* like role="play": @focusRing@ is the accent fill itself in the dark theme */
QPushButton[role="primary"]:focus { border-color: @tx@; }
/* Phase 4: library (components/library/) */
QListView[role="library"], QScrollArea[role="library"] { background: transparent; border: none; outline: 0; }
QScrollArea[role="library"] > QWidget > QWidget { background: transparent; }
QLineEdit[role="librarySearch"] { border: 1px solid @border@; }
QLineEdit[role="librarySearch"]:hover { border-color: @outline@; }
QLineEdit[role="librarySearch"]:focus { border: 2px solid @focusRing@; }
QPushButton[role="outline"] { background: transparent; }
QPushButton[role="outline"]:hover { background: @hoverFill@; }
QLabel[role="libraryDim"] { color: @dim@; }
/* Phase 5: effects (dialogs/extended.cpp, components/extended_panels.cpp) */
#effectsSheet QTabWidget::pane { background: transparent; border: none; }
#effectsSheet QTabWidget#effectsTabs::pane { border-top: 1px solid @divider@; }
#effectsSheet QTabWidget#effectsTabs::tab-bar { left: @fxTabsPadX@px; }
QToolButton#effectsMore { padding-right: @iconGlyph@px; }
QToolButton#effectsMore::menu-indicator { image: url("@arrowDown@"); subcontrol-origin: padding; subcontrol-position: center right;
    width: @iconGlyph@px; height: @iconGlyph@px; }
#effectsSheet QScrollArea, #effectsSheet QScrollArea > QWidget > QWidget { background: transparent; }
QPushButton[role="chip"] { background: transparent; color: @tx2@; border: 1px solid @chipBorder@; border-radius: @chipRadius@px; padding: 0 @fxChipPadX@px; }
QPushButton[role="chip"]:hover { background: @hoverFill@; }
QPushButton[role="chip"]:checked { background: @accentTint@; color: @accTx@; border-color: @accentBorder@; }
QPushButton[role="chip"]:focus { border: 2px solid @focusRing@; padding: 0 @fxChipPadXFocus@px; }
QPushButton[role="chip"]:disabled { background: transparent; color: @dim@; border-color: @divider@; }
QFrame[role="fxRow"] { background: @raised@; border-radius: @fxRowRadius@px; }
QToolButton[role="stepper"] { background: @stepperFill@; }
QToolButton[role="stepper"]:hover { background: @stepperHover@; }
/* Phase 5: dialogs (dialogs/open.cpp, errors.cpp; DESIGN_SPEC 7).
   openTile = an Open media tab tile, split = the accent Play split button,
   errorIcon = the round error icon, link = a text button in the accent text colour. */
QToolButton[role="openTile"] { background: @raised@; color: @tx2@; border: 1px solid transparent; border-radius: @radiusLg@px; }
QToolButton[role="openTile"]:hover { background: @hover@; color: @tx@; }
QToolButton[role="openTile"]:checked { background: @accentTint@; color: @accTx@; border-color: @accentBorder@; }
QToolButton[role="openTile"]:focus { border-color: @focusRing@; }
#OpenDisk QRadioButton { background: @raised@; border: 1px solid transparent; border-radius: @radiusMd@px; padding: @menuPad@px @buttonPadX@px; spacing: 0; }
#OpenDisk QRadioButton::indicator { image: none; width: 0; height: 0; border: none; }
#OpenDisk QRadioButton:hover { background: @hover@; }
#OpenDisk QRadioButton:checked { background: @accentTint@; color: @accTx@; border-color: @accentBorder@; }
#OpenDisk QRadioButton:focus { border-color: @focusRing@; }
QToolButton[role="split"] { background: @accent@; color: @onAccent@; font-weight: 600; padding: @menuPad@px @buttonPadX@px; }
QToolButton[role="split"][popupMode="1"] { padding-right: @splitPadR@px; }
QToolButton[role="split"]:hover { background: @accentHover@; }
QToolButton[role="split"]:focus { border-color: @onAccent@; }
QToolButton[role="split"]::menu-arrow { image: url("@arrowDownOnAccent@"); }
QLabel[role="errorIcon"] { background: @errorTint@; border-radius: @errorIconR@px; }
QPushButton[role="link"] { background: transparent; color: @accTx@; border: 2px solid transparent; }
QPushButton[role="link"]:hover { background: @hoverFill@; }
QPushButton[role="link"]:focus { border-color: @focusRing@; }
/* netUrl = the network URL field: no arrow (netRecent lists its items), the accent ring as in 1.6.2 */
QComboBox[role="netUrl"]::drop-down { width: 0; border: none; }
QComboBox[role="netUrl"]::down-arrow { image: none; width: 0; }
QComboBox[role="netUrl"]:focus { border-color: @focusRing@; }
QListWidget[role="netRecent"] { background: transparent; border: none; outline: 0; }
QListWidget[role="netRecent"]::item { padding: @menuPad@px 0; border-radius: @radiusSm@px; }
QListWidget[role="netRecent"]::item:selected { background: @hoverFill@; color: @tx@; }
QListWidget[role="netRecent"]:focus { border: 2px solid @focusRing@; border-radius: @radiusMd@px; }
/* Phase 5: preferences (components/prefs_page.cpp): the Simple Preferences
   group boxes as cards of rows */
PrefsPage QScrollArea, PrefsPage QScrollArea > QWidget > QWidget { background: transparent; }
PrefsPage QGroupBox, QFrame[role="prefsCard"] { background: @surface@; border: 1px solid @divider@; border-radius: @prefsCardR@px; }
QFrame[role="prefsRow"], QFrame[role="prefsRowNext"] { background: transparent; border: none; }
QFrame[role="prefsRowNext"] { border-top: 1px solid @prefsRowLine@; }
/* the select height less the combo's 1 px border and 1 px margin, each side */
PrefsPage QComboBox { min-height: @prefsSelectInner@px; }
)QSS";

    const Palette &p = palette();
#ifdef _WIN32
    /* DWM rounds and outlines pop-ups from Windows 11 (applyWindowFrame):
     * a square border would be clipped at the corners there */
    const bool dwmRounds = WinTheme::build() >= 22000;
#else
    const bool dwmRounds = false;
#endif
    const int lineH = QFontMetrics(font(Body)).height();
    const int metaH = QFontMetrics(font(Meta)).height();
    const struct { const char *name; QString value; } vars[] = {
        { "chrome", css(QColor::fromRgb(p.chrome)) },
        { "panel", css(QColor::fromRgb(p.panel)) },
        { "surface", css(QColor::fromRgb(p.surface)) },
        { "raised", css(QColor::fromRgb(p.raised)) },
        { "hover", css(QColor::fromRgb(p.hover)) },
        { "tx", css(QColor::fromRgb(p.tx)) },
        { "tx2", css(QColor::fromRgb(p.tx2)) },
        { "mute", css(QColor::fromRgb(p.mute)) },
        { "dim", css(QColor::fromRgb(p.dim)) },
        { "accent", css(color(Accent)) },
        { "accentHover", css(color(AccentHover)) },
        /* the accent is 2.4:1 on light surfaces: focus rings need >= 3:1 */
        { "focusRing", css(color(dark ? Accent : AccentText)) },
        { "onAccent", css(color(OnAccent)) },
        { "closeHover", css(color(WindowCloseHover)) },
        { "accentTint", css(color(AccentTint14)) },
        { "accentBorder", css(color(AccentBorder50)) },
        { "popupBorder", dwmRounds ? QString("none")
                                   : QString("1px solid %1").arg(css(ink(InkAlpha::Border))) },
        { "hoverFill", css(ink(InkAlpha::HoverFill)) },
        { "openFill", css(ink(InkAlpha::OpenFill)) },
        { "divider", css(ink(InkAlpha::Divider)) },
        { "border", css(ink(InkAlpha::Border)) },
        { "outline", css(ink(InkAlpha::Outline)) },
        { "trackInk", css(ink(InkAlpha::Track)) },
        { "toggleOff", css(ink(InkAlpha::ToggleOff)) },
        { "radiusXs", QString::number(px(Size::RadiusXs)) },
        { "radiusSm", QString::number(px(Size::RadiusSm)) },
        { "radiusMd", QString::number(px(Size::RadiusMd)) },
        { "radiusLg", QString::number(px(Size::RadiusLg)) },
        { "menuPad", QString::number(px(Size::MenuPanelPadding)) },
        { "menuPadFocus", QString::number(px(Size::MenuPanelPadding) - 1) },
        { "menuSepX", QString::number(2 * px(Size::MenuPanelPadding)) },
        { "menuMinW", QString::number(px(Size::MenuMinW)) },
        { "menuTitlePadX", QString::number(px(Size::MenuTitlePadX)) },
        { "menuTitlePadY", QString::number(qMax(0, (px(Size::MenuTitleH) - lineH) / 2)) },
        { "menuItemPadY", QString::number(qMax(0, (px(Size::MenuItemH) - lineH) / 2)) },
        { "buttonPadX", QString::number(px(Size::ButtonPadX)) },
        { "buttonPadXFocus", QString::number(px(Size::ButtonPadX) - 1) },
        { "groupTitleH", QString::number(metaH) },
        { "scrollW", QString::number(px(Size::ScrollBarW)) },
        { "iconGlyph", QString::number(px(Size::IconGlyph)) },
        { "halfGlyph", QString::number(px(Size::IconGlyph) / 2) },
        { "arrowDown", tintedIconFile(p_intf, RedesignIcons::IconExpandMore, QColor::fromRgb(p.tx2), 0) },
        { "arrowUp", tintedIconFile(p_intf, RedesignIcons::IconExpandMore, QColor::fromRgb(p.tx2), 180) },
        { "arrowDownDim", tintedIconFile(p_intf, RedesignIcons::IconExpandMore, QColor::fromRgb(p.dim), 0) },
        { "arrowUpDim", tintedIconFile(p_intf, RedesignIcons::IconExpandMore, QColor::fromRgb(p.dim), 180) },
        { "track", QString::number(px(Size::SeekTrack)) },
        { "trackRadius", QString::number(px(Size::SeekTrack) / 2) },
        { "knob", QString::number(px(Size::SeekKnob)) },
        { "knobRadius", QString::number(px(Size::SeekKnob) / 2) },
        { "knobMargin", QString::number(-(px(Size::SeekKnob) - px(Size::SeekTrack)) / 2) },
        /* Phase 3: now playing, mini player, audio mode */
        { "accTx", css(color(AccentText)) },
        { "chipPadX", QString::number(px(NowPlaying::ChipPadX)) },
        { "chipPadY", QString::number(px(NowPlaying::ChipPadY)) },
        { "playNowR", QString::number(px(Size::PlayNowBar) / 2) },
        { "pillPadL", QString::number(px(NowPlaying::PillPadLeft)) },
        { "pillPadR", QString::number(px(NowPlaying::PillPadRight)) },
        { "scrim", css(QColor::fromRgba(qRgba(qRed(VideoSurface), qGreen(VideoSurface), qBlue(VideoSurface),
                                              NowPlaying::MiniRestoreBgAlpha))) },
        /* Phase 3: deck */
        { "separatorInk", css(ink(InkAlpha::Separator)) },
        { "deckSepMargin", QString::number(px(Deck::SeparatorMargin)) },
        { "playRadius", QString::number(px(Size::PlayDeck) / 2) },
        { "speedPadX", QString::number(px(Deck::SpeedPadX)) },
        { "errorTint", css(color(ErrorTint)) },
        /* Phase 4: home */
        { "homeCellPad", QString::number(px(Home::ColumnGap) / 2) },
        { "homeRowPad", QString::number(px(Home::RowPadX)) },
        { "homeRowLine", css(ink(InkAlpha::RowLine)) },
        /* Phase 5: effects */
        { "fxTabsPadX", QString::number(px(Effects::TabsPadX)) },
        { "chipBorder", css(ink(InkAlpha::ChipBorder)) },
        { "chipRadius", QString::number(px(Size::ChipH) / 2) },
        { "fxChipPadX", QString::number(px(Effects::ChipPadX)) },
        { "fxChipPadXFocus", QString::number(px(Effects::ChipPadX) - 1) },
        { "fxRowRadius", QString::number(px(Effects::SyncRowRadius)) },
        { "stepperFill", css(ink(InkAlpha::StepperFill)) },
        { "stepperHover", css(ink(InkAlpha::StepperHover)) },
        /* Phase 5: dialogs */
        { "splitPadR", QString::number(px(Size::IconGlyph) + px(Size::MenuPanelPadding)) },
        { "arrowDownOnAccent", tintedIconFile(p_intf, RedesignIcons::IconExpandMore, color(OnAccent), 0) },
        { "errorIconR", QString::number(px(Dialogs::ErrorIcon) / 2) },
        /* Phase 5: preferences */
        { "prefsCardR", QString::number(px(Prefs::CardRadius)) },
        { "prefsRowLine", css(ink(InkAlpha::Hairline)) },
        { "prefsSelectInner", QString::number(px(Size::SelectH) - 4) },
    };

    QString out = QString::fromLatin1(qss);
    for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++)
        out.replace(QLatin1Char('@') + QLatin1String(vars[i].name) + QLatin1Char('@'),
                    vars[i].value);
    if (out.contains(QLatin1Char('@')))
        msg_Err(p_intf, "theme: unresolved stylesheet token in %s",
                qtu(out.mid(out.indexOf(QLatin1Char('@')), 32)));
    return out;
}

#ifdef _WIN32
/* Title bar colours for windows, rounded corners for menus and tooltips.
 * Applied when a window is shown: its native window exists then but is not
 * on screen yet, and windows whose flags changed get it again. */
void Theme::applyWindowFrame(QWidget *w) const
{
    /* Frameless windows (fullscreen controller, time tooltip) draw their
     * own shape: no caption to colour, no corners to round. */
    if (w->windowFlags() & Qt::FramelessWindowHint)
        return;
    const Qt::WindowType type = w->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip)
    {
        WinTheme::setRoundCorners(reinterpret_cast<HWND>(w->winId()));
        return;
    }
    if (type != Qt::Window && type != Qt::Dialog && type != Qt::Tool)
        return;
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const HRESULT hrDark = WinTheme::setDarkTitleBar(hwnd, dark);
    const HRESULT hrCaption = WinTheme::setCaptionColors(hwnd, palette().chrome, palette().tx);
    WinTheme::refreshFrame(hwnd);
    msg_Dbg(p_intf, "theme: title bar of %s dark=%d hr=0x%08lx caption hr=0x%08lx",
            w->metaObject()->className(), dark,
            (unsigned long)hrDark, (unsigned long)hrCaption);
}
#endif

bool Theme::eventFilter(QObject *o, QEvent *e)
{
#ifdef _WIN32
    if (e->type() == QEvent::Show && o->isWidgetType() && !highContrast)
    {
        QWidget *w = static_cast<QWidget *>(o);
        if (w->isWindow())
            applyWindowFrame(w);
    }
    else if (e->type() == QEvent::KeyPress && o->isWidgetType()
          && static_cast<QKeyEvent *>(e)->key() == Qt::Key_Alt)
    {
        /* from now on this window shows its access keys (ThemeStyle) */
        QWidget *window = static_cast<QWidget *>(o)->window();
        if (!window->property(altSeenProperty).toBool())
        {
            window->setProperty(altSeenProperty, true);
            foreach (QWidget *child, window->findChildren<QWidget *>())
                child->update();
        }
    }
#endif
    return QObject::eventFilter(o, e);
}
