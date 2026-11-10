/*****************************************************************************
 * sidebar.hpp : navigation sidebar of the main interface
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

#ifndef VLC_QT_SIDEBAR_HPP_
#define VLC_QT_SIDEBAR_HPP_

#include "qt.hpp"
#include "util/breakpoints.hpp"

#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class SidebarButton;

/* DESIGN_SPEC 2: "Open media", the library pages and Preferences. It owns
 * one page widget per nav item, Preferences included; the main interface
 * puts them in its stack and shows the one pageRequested() names. Below
 * SidebarRailBelow it is an icon rail whose tooltips carry the labels. */
class Sidebar : public QWidget
{
    Q_OBJECT
public:
    enum Page { Home, Videos, Music, Network, Preferences, PageCount };

    explicit Sidebar( intf_thread_t *, QWidget *parent = NULL );
    /* Parentless until added to a stack, which then owns it */
    QWidget *page( Page p ) const { return pages[p]; }
    /* Its width at a breakpoint, device-independent px */
    static int widthAt( BreakpointWatcher::Size );

public slots:
    /* As if the user picked it, e.g. for "go to library"; Preferences
     * without the qt-advanced-pref check of its item */
    void showPage( int );
    /* Keep the active item in step with the stack's current widget:
     * none is active when it is not one of the pages */
    void setCurrentWidget( QWidget * );
    void setBreakpoint( BreakpointWatcher::Size );

signals:
    void pageRequested( QWidget * );

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    QButtonGroup *group;
    QList<SidebarButton *> buttons;
    QWidget *pages[PageCount];
    QWidget *currentPage;   /* the stack's, from setCurrentWidget() */
};

#endif
