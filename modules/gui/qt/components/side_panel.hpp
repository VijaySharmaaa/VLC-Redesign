/*****************************************************************************
 * side_panel.hpp : a panel docked at the right of the content, a drawer at XS
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

#ifndef VLC_QT_SIDE_PANEL_HPP_
#define VLC_QT_SIDE_PANEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QWidget>

class QBoxLayout;
class QPainter;
class QVariantAnimation;
class OverlayHost;

/* The playlist and the Effects sheet (DESIGN_SPEC 6, 7): the last widget of
 * the dock layout, the video shrinking beside it; at XS a drawer, a tool
 * window over the right of `over` (which may be the native video) with a
 * shadow on its left. The subclass builds its widgets in `content`, which
 * keeps the full width and is revealed as the panel slides in.
 * Without a dock (NULL) it is a plain widget the subclass makes a window. */
class SidePanel : public QWidget
{
public:
    SidePanel( QBoxLayout *dock, QWidget *over );

protected:
    /* the width and the mode for a breakpoint; true when the mode changed */
    bool setDocked( bool drawer, int width );
    /* enter slides (unless motion is off); exit is instant, a hide() */
    void slideIn();
    int shadowWidth() const;
    /* the panel's own fill, right of the shadow */
    virtual void paintSheet( QPainter &, const QRect & );

    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;

    QBoxLayout *dock;       /* NULL without the main window */
    QWidget *dockParent;
    QWidget *content;       /* full width, revealed as the panel slides */
    QVariantAnimation *slide;
    bool drawer;

private:
    void setShownWidth( int );
    void place();

    OverlayHost *host;
    int fullWidth;
    int shownWidth;
};

#endif
