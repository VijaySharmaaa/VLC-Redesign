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
#include "util/theme_tokens.hpp"

#include <cassert>

#include <QEvent>
#include <QStyle>
#include <QVariant>

static const char * const names[] = { "XS", "S", "M", "L" };

BreakpointWatcher::BreakpointWatcher( QWidget *_window )
    : QObject( _window ), window( _window ), size( fromWidth( _window->width() ) )
{
    assert( fromWidth( RedesignTokens::Breakpoint::M ) == M
         && fromWidth( RedesignTokens::Breakpoint::M - 1 ) == S
         && fromWidth( RedesignTokens::Breakpoint::S - 1 ) == XS
         && fromWidth( RedesignTokens::Breakpoint::L ) == L );
    apply( window );
    window->installEventFilter( this );
}

BreakpointWatcher::Size BreakpointWatcher::fromWidth( int width )
{
    if( width >= RedesignTokens::Breakpoint::L ) return L;
    if( width >= RedesignTokens::Breakpoint::M ) return M;
    if( width >= RedesignTokens::Breakpoint::S ) return S;
    return XS;
}

void BreakpointWatcher::addTarget( QWidget *w )
{
    targets.append( w );
    apply( w );
}

/* Dynamic-property selectors are not re-evaluated on their own */
void BreakpointWatcher::apply( QWidget *w ) const
{
    w->setProperty( "breakpoint", QLatin1String( names[size] ) );
    w->style()->unpolish( w );
    w->style()->polish( w );
}

bool BreakpointWatcher::eventFilter( QObject *obj, QEvent *event )
{
    if( obj == window && event->type() == QEvent::Resize )
    {
        const Size now = fromWidth( window->width() );
        if( now != size )
        {
            size = now;
            apply( window );
            for( const QPointer<QWidget> &w : targets )
                if( w )
                    apply( w );
            emit changed( size );
        }
    }
    return QObject::eventFilter( obj, event );
}
