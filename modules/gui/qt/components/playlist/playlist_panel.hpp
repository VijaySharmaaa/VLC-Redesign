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
class QModelIndex;
class PLModel;
class PlListView;

/* DESIGN_SPEC 6: the playlist docked at the right of the content area, on
 * the shared PLModel (so a filter here also filters the separate playlist
 * window); a drawer at XS (SidePanel). Exit is instant, enter slides. */
class PlaylistPanel : public SidePanel
{
    Q_OBJECT
public:
    PlaylistPanel( intf_thread_t *, QBoxLayout *dock, QWidget *over );

    void setOpen( bool );

public slots:
    void setBreakpoint( BreakpointWatcher::Size );

signals:
    void closeRequested();

protected:
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    void popup( const QPoint & );

    intf_thread_t *p_intf;
    PLModel *model;
    PlListView *view;
    QLabel *count;
    QTimer countTimer;
};

#endif
