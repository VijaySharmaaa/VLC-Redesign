/*****************************************************************************
 * playlist_panel.hpp : the docked playlist beside the video
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

#ifndef VLC_QT_PLAYLIST_PANEL_HPP_
#define VLC_QT_PLAYLIST_PANEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "util/breakpoints.hpp"
#include "components/side_panel.hpp"

#include <QTimer>

class QLabel;
class QLineEdit;
class QModelIndex;
class PLModel;
class PlListView;

/* DESIGN_SPEC 6: the playlist docked at the right of the content area, on
 * its own PLModel of the main playlist; its filter and the separate
 * playlist window's search show the same text. A drawer at XS (SidePanel).
 * Exit is instant, enter slides. */
class PlaylistPanel : public SidePanel
{
    Q_OBJECT
public:
    PlaylistPanel( intf_thread_t *, QBoxLayout *dock, QWidget *over );

    /* closing clears the filter: its marks would make the player skip the
     * hidden items with nothing on screen */
    void setOpen( bool );
    /* Ctrl+K, as the playlist window's search */
    void focusFilter();

public slots:
    void setBreakpoint( BreakpointWatcher::Size );

signals:
    void closeRequested();

protected:
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;
    /* files dropped here are queued, as on the playlist window */
    void dragEnterEvent( QDragEnterEvent * ) Q_DECL_OVERRIDE;
    void dropEvent( QDropEvent * ) Q_DECL_OVERRIDE;

private:
    void popup( const QPoint & );
    void updateCount();

    intf_thread_t *p_intf;
    PLModel *model;
    PlListView *view;
    QLabel *count;
    QLineEdit *filter;
    QTimer countTimer;
};

#endif
