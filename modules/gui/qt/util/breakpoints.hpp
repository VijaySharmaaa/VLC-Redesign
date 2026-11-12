/*****************************************************************************
 * breakpoints.hpp : window-width breakpoints of the main interface
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

#ifndef VLC_QT_BREAKPOINTS_HPP_
#define VLC_QT_BREAKPOINTS_HPP_

#include <QObject>
#include <QWidget>

/* Watches the width of a window and sorts it into the design's breakpoints
 * (RedesignTokens::Breakpoint). Emits changed() when the breakpoint changes,
 * and after every theme change (another scale moves the breakpoints and the
 * sizes the listeners take with Theme::px). Widgets connect to changed() and
 * read current() once. */
class BreakpointWatcher : public QObject
{
    Q_OBJECT
public:
    enum Size { XS, S, M, L };

    explicit BreakpointWatcher( QWidget *window );
    Size current() const { return size; }
    /* width in device-independent px; the breakpoints scale like Theme::px */
    static Size fromWidth( int );

signals:
    void changed( BreakpointWatcher::Size );

protected:
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    void update( bool always );

    QWidget *window;
    Size size;
};

#endif
