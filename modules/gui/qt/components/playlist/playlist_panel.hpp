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

#include <QWidget>
#include <QTimer>

class QBoxLayout;
class QLabel;
class QModelIndex;
class QVariantAnimation;
class OverlayHost;
class PLModel;
class PlListView;

/* DESIGN_SPEC 6: the playlist docked at the right of the content area, on
 * its own PLModel of the main playlist; its filter and the separate
 * playlist window's search show the same text. From S up it is the last widget of the dock layout; at XS it
 * becomes a drawer: a tool window over the right of `over`, which may be
 * the native video, with a shadow. Exit is instant, enter slides. */
class PlaylistPanel : public QWidget
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
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    void popup( const QPoint & );
    void updateCount();
    void setShownWidth( int );
    void place();
    int shadowWidth() const;

    intf_thread_t *p_intf;
    QBoxLayout *dock;
    QWidget *dockParent;
    OverlayHost *host;
    PLModel *model;
    QWidget *content;       /* full width, revealed as the panel slides */
    PlListView *view;
    QLabel *count;
    QVariantAnimation *slide;
    QTimer countTimer;
    int fullWidth;
    int shownWidth;
    bool drawer;
};

#endif
