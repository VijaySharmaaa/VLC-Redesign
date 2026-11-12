/*****************************************************************************
 * breakpoints.cpp : window-width breakpoints of the main interface
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

#include "util/breakpoints.hpp"
#include "util/theme.hpp"
#include "util/customwidgets.hpp"   /* setStyleProperty */

#include <QEvent>
#include <QStyle>
#include <QVariant>

static const char * const names[] = { "XS", "S", "M", "L" };

BreakpointWatcher::BreakpointWatcher( QWidget *_window )
    : QObject( _window ), window( _window ), size( fromWidth( _window->width() ) )
{
    apply( window );
    window->installEventFilter( this );
    /* another screen's scale moves the breakpoints */
    connect( Theme::instance(), &Theme::changed, this, [this]() { update( true ); } );
}

BreakpointWatcher::Size BreakpointWatcher::fromWidth( int width )
{
    const Theme *t = Theme::instance();
    if( width >= t->px( RedesignTokens::Breakpoint::L ) ) return L;
    if( width >= t->px( RedesignTokens::Breakpoint::M ) ) return M;
    if( width >= t->px( RedesignTokens::Breakpoint::S ) ) return S;
    return XS;
}

void BreakpointWatcher::addTarget( QWidget *w )
{
    targets.append( w );
    apply( w );
}

void BreakpointWatcher::apply( QWidget *w ) const
{
    setStyleProperty( w, "breakpoint", QString::fromLatin1( names[size] ) );
}

bool BreakpointWatcher::eventFilter( QObject *obj, QEvent *event )
{
    if( obj == window && event->type() == QEvent::Resize )
        update( false );
    return QObject::eventFilter( obj, event );
}

void BreakpointWatcher::update( bool always )
{
    const Size now = fromWidth( window->width() );
    if( now == size && !always )
        return;
    size = now;
    apply( window );
    for( const QPointer<QWidget> &w : targets )
        if( w )
            apply( w );
    emit changed( size );
}
