/*****************************************************************************
 * deck_popover.hpp : the control deck's tracks and speed popovers
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

#ifndef VLC_QT_DECK_POPOVER_HPP_
#define VLC_QT_DECK_POPOVER_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

class QAbstractButton;
class QWidget;

/* DESIGN_SPEC 7: pop-up windows built afresh on every opening, rising above
 * their button, which shows as open ([open="true"]) meanwhile. Esc, a click
 * outside or a choice that ends the popover closes and deletes it. Return
 * the popover, NULL when there is nothing to show. */
QWidget *showTracksPopover( intf_thread_t *, QAbstractButton *anchor );
QWidget *showSpeedPopover( intf_thread_t *, QAbstractButton *anchor );

#endif
