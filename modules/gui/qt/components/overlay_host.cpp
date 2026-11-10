/*****************************************************************************
 * overlay_host.cpp : top-level windows drawn above the main window
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

#include "components/overlay_host.hpp"
#include "util/theme.hpp"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>

#ifdef _WIN32
# include <windows.h>
#endif

OverlayWindow::OverlayWindow( QWidget *owner, InputMode input )
    : QWidget( owner, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
                      | Qt::WindowDoesNotAcceptFocus
                      | ( input == PassInput ? Qt::WindowTransparentForInput
                                             : Qt::WindowFlags() ) )
{
    setAcceptDrops( input == TakeInput );
    setAttribute( Qt::WA_TranslucentBackground );
    setAttribute( Qt::WA_ShowWithoutActivating );
    setFocusPolicy( Qt::NoFocus );
    connect( Theme::instance(), &Theme::changed,
             this, static_cast<void (QWidget::*)()>( &QWidget::update ) );
}

bool OverlayWindow::event( QEvent *e )
{
    switch( e->type() )
    {
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
    case QEvent::Drop:
        /* only an input-taking overlay accepts drops. Straight to the owner's
         * handlers (they do not read the position): sendEvent() would route
         * the drag back to its current target, this window, forever. */
        return parentWidget() && static_cast<QObject *>( parentWidget() )->event( e );
    default:
        return QWidget::event( e );
    }
}

/* The keys its buttons leave (Space, the hotkeys) are the owner's */
void OverlayWindow::keyPressEvent( QKeyEvent *e )
{
    if( parentWidget() )
        QApplication::sendEvent( parentWidget(), e );
}

bool OverlayWindow::nativeEvent( const QByteArray &type, void *message, long *result )
{
#ifdef _WIN32
    /* a click is not a request for the keys: the owner keeps them */
    MSG *msg = static_cast<MSG *>( message );
    if( msg->message == WM_MOUSEACTIVATE )
    {
        *result = MA_NOACTIVATE;
        return true;
    }
#endif
    return QWidget::nativeEvent( type, message, result );
}

void OverlayWindow::giveFocusBack()
{
    /* not isActiveWindow(): a tool window can share its owner's */
    if( QApplication::activeWindow() != this || !parentWidget() )
        return;
    parentWidget()->activateWindow();
    parentWidget()->setFocus( Qt::OtherFocusReason );
}

QEasingCurve OverlayWindow::enterCurve()
{
    QEasingCurve c( QEasingCurve::BezierSpline );
    c.addCubicBezierSegment( QPointF( .2, .8 ), QPointF( .2, 1. ), QPointF( 1., 1. ) );
    return c;
}

OverlayHost::OverlayHost( QWidget *_target, QObject *parent )
    : QObject( parent ), target( _target )
{
    /* coalesce the burst of move/resize events a drag or a layout change sends */
    pending.setSingleShot( true );
    pending.setInterval( 0 );
    connect( &pending, &QTimer::timeout, this, &OverlayHost::changed );
    watchAncestors();
}

QRect OverlayHost::targetRect() const
{
    return target ? QRect( target->mapToGlobal( QPoint( 0, 0 ) ), target->size() ) : QRect();
}

bool OverlayHost::targetShown() const
{
    return target && target->isVisible() && !target->window()->isMinimized();
}

void OverlayHost::watchAncestors()
{
    foreach( const QPointer<QWidget> &w, watched )
        if( w )
            w->removeEventFilter( this );
    watched.clear();
    for( QWidget *w = target; w; w = w->parentWidget() )
    {
        w->installEventFilter( this );
        watched << w;
    }
}

bool OverlayHost::eventFilter( QObject *o, QEvent *e )
{
    switch( e->type() )
    {
    case QEvent::ParentChange:      /* e.g. Ctrl+L moves the video into the playlist */
        if( o == target )
            watchAncestors();
        /* fall through */
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::WindowStateChange:
        pending.start();
        break;
    default:
        break;
    }
    return false;
}
