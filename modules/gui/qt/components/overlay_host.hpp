/*****************************************************************************
 * overlay_host.hpp : top-level windows drawn above the main window
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

#ifndef QVLC_OVERLAY_HOST_H_
#define QVLC_OVERLAY_HOST_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QWidget>
#include <QTimer>
#include <QList>
#include <QPointer>
#include <QEasingCurve>

/* A frameless, per-pixel translucent tool window owned by the main window.
 * The video is a native child HWND, so only a separate top-level can paint
 * over it. It never takes focus and, being owned rather than topmost, stays
 * above its owner only. By default it takes no input either (drags and drops
 * fall through to the window below); TakeInput gets clicks and hands drags
 * and drops to the owner, so a drop on it plays as a drop on the window.
 * One that accepts focus (Tab moves the keys into it) still never takes
 * them on a click, and hands the keys its buttons leave to the owner. */
class OverlayWindow : public QWidget
{
public:
    enum InputMode { PassInput, TakeInput };
    explicit OverlayWindow( QWidget *owner, InputMode = PassInput );
    static QEasingCurve enterCurve();   /* the tokens' enter easing */
    void giveFocusBack();   /* the keys back to the owner, if it has them */
protected:
    bool event( QEvent * ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;
    bool nativeEvent( const QByteArray &, void *, long * ) Q_DECL_OVERRIDE;
};

/* Follows a widget of the main window: emits changed() once per burst of
 * moves, resizes, shows, hides and window state changes of the widget or
 * any of its ancestors. Overlays place themselves on targetRect(). */
class OverlayHost : public QObject
{
    Q_OBJECT
public:
    OverlayHost( QWidget *target, QObject *parent );
    QRect targetRect() const;   /* global coordinates */
    bool targetShown() const;   /* visible and its window not minimised */

signals:
    void changed();

protected:
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    void watchAncestors();

    QPointer<QWidget> target;
    QList<QPointer<QWidget> > watched;
    QTimer pending;
};

#endif
