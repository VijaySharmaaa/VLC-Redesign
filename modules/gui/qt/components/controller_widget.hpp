/*****************************************************************************
 * controller_widget.hpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
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

#ifndef VLC_QT_CONTROLLER_WIDGET_HPP_
#define VLC_QT_CONTROLLER_WIDGET_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "input_manager.hpp"
#include <vlc_vout.h>                       /* vout_thread_t for aspect ratio combobox */

#include <QWidget>
#include <QToolButton>
#include <QVariantAnimation>

class QFrame;
class QAbstractSlider;

/**
 * SPECIAL Widgets that are a bit more than just a ToolButton
 * and have an icon/behaviour that changes depending on the context:
 * - playButton
 * - A->B Button
 * - Teletext group buttons
 * - Sound Widget group
 **/
class PlayButton : public QToolButton
{
    Q_OBJECT
public:
    PlayButton();
    /* the deck's accent circle (DESIGN_SPEC 3), for a big play button */
    void setRound( bool );
public slots:
    void updateButtonIcons( bool );
protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
private:
    void pressTo( qreal );
    bool b_round, b_playing;
    QVariantAnimation press;   /* the round button's scale, 1 at rest */
};

class LoopButton : public QToolButton
{
    Q_OBJECT
public slots:
    void updateButtonIcons( int );
};

class AtoB_Button : public QToolButton
{
    Q_OBJECT
public slots:
    void updateButtonIcons( bool, bool );
};

/* The aspect ratio as a deck icon button (on = accent when not the
 * default); its menu has the vout's aspect-ratio choices, the current one
 * checked, and text() is the current value ("16:9", "Default") */
class AspectRatioBadge : public QToolButton
{
    Q_OBJECT
public:
    AspectRatioBadge( intf_thread_t * );
    virtual ~AspectRatioBadge();
    QMenu *ratios() const { return menu; }

private slots:
    void setVout();
    void updateRatios();
    void choose( QAction * );

private:
    static int AspectCB( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * );
    intf_thread_t* p_intf;
    vout_thread_t *p_vout;  /* held, with AspectCB on its aspect-ratio */
    QMenu *menu;
};

class SoundWidget : public QWidget
{
    Q_OBJECT

public:
    SoundWidget( QWidget *parent, intf_thread_t  *_p_i, bool,
                 bool b_special = false );
    void setMuted( bool );

private:
    intf_thread_t       *p_intf;
    QToolButton         *volMuteButton;
    QAbstractSlider     *volumeSlider;
    QFrame              *volumeControlWidget;
    QMenu               *volumeMenu;

    bool                b_is_muted;
    bool                b_ignore_valuechanged;

protected slots:
    void userUpdateVolume( int );
    void libUpdateVolume( float );
    void updateMuteStatus( bool );
    void refreshLabels( void );
    void muteClicked();
    void valueChangedFilter( int );

signals:
    void valueReallyChanged( int );
};

#endif
