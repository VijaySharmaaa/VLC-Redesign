/*****************************************************************************
 * drop_overlay.hpp : "Drop to play" frame shown while files are dragged in
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

#ifndef QVLC_DROP_OVERLAY_H_
#define QVLC_DROP_OVERLAY_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/overlay_host.hpp"

class QPropertyAnimation;

/* Covers the whole window. Input-transparent, so the drop itself still
 * lands on MainInterface::dropEvent. */
class DropOverlay : public OverlayWindow
{
public:
    explicit DropOverlay( QWidget *window );
    void setActive( bool );

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    void place();

    OverlayHost *host;
    QPropertyAnimation *fade;
    bool active;
};

#endif
