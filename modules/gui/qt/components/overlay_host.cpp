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

void OverlayWindow::setVisible( bool visible )
{
    const bool wasVisible = isVisible();
    QWidget::setVisible( visible );
    if( visible && !wasVisible )
        raiseDrawers();
}

/* The XS drawers (playlist, Effects sheet) are tool windows of the same
 * owner over the video: an overlay shown after them would cover them and
 * take their clicks */
void OverlayWindow::raiseDrawers()
{
    if( !parentWidget() )
        return;
#ifdef _WIN32
    /* the overlay goes under the lowest drawer below it: only VLC's own
     * windows change places, never above another application's (HWND_TOP
     * would, with VLC in the background), and nothing is activated */
    QList<HWND> drawers;
#endif
    foreach( QWidget *w, parentWidget()->findChildren<QWidget *>( QString(),
                                                                 Qt::FindDirectChildrenOnly ) )
    {
        if( !w->isWindow() || !w->isVisible()
         || !( w->inherits( "PlaylistPanel" ) || w->inherits( "ExtendedDialog" ) ) )
            continue;
#ifdef _WIN32
        drawers << reinterpret_cast<HWND>( w->winId() );
#else
        w->raise();
#endif
    }
#ifdef _WIN32
    if( drawers.isEmpty() )
        return;
    const HWND self = reinterpret_cast<HWND>( winId() );
    HWND lowest = NULL;
    for( HWND h = GetWindow( self, GW_HWNDNEXT ); h != NULL; h = GetWindow( h, GW_HWNDNEXT ) )
        if( drawers.contains( h ) )
            lowest = h;
    if( lowest )
        SetWindowPos( self, lowest, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER );
#endif
}

QEasingCurve OverlayWindow::enterCurve()
{
    QEasingCurve c( QEasingCurve::BezierSpline );
    using namespace RedesignTokens::Motion;
    c.addCubicBezierSegment( QPointF( EnterX1, EnterY1 ) / 100., QPointF( EnterX2, EnterY2 ) / 100.,
                             QPointF( 1., 1. ) );
    return c;
}

QWidget *firstTabStop( QWidget *root )
{
    for( QWidget *w = root->nextInFocusChain(); w != root; w = w->nextInFocusChain() )
        if( root->isAncestorOf( w ) && w->isEnabled() && w->isVisibleTo( root )
         && ( w->focusPolicy() & Qt::TabFocus ) )
            return w;
    return NULL;
}

OverlayHost::OverlayHost( QWidget *_target, QObject *parent )
    : QObject( parent ), target( _target )
{
    /* coalesce the burst of move/resize events a drag or a layout change sends */
    pending.setSingleShot( true );
    pending.setInterval( 0 );
    connect( &pending, &QTimer::timeout, this, &OverlayHost::changed );
    /* another screen's scale: the overlays size and place themselves again */
    connect( Theme::instance(), &Theme::changed, &pending,
             static_cast<void (QTimer::*)()>( &QTimer::start ) );
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
