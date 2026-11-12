/*****************************************************************************
 * input_slider.hpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ludovic Fauvet <etix@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_INPUT_SLIDER_HPP_
#define VLC_QT_INPUT_SLIDER_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "styles/seekstyle.hpp"
#include "customwidgets.hpp"

#include <QSlider>
#include <QPainter>
#include <QTime>

class QMouseEvent;
class QWheelEvent;
class QHideEvent;
class QTimer;
class SeekPoints;
class QPropertyAnimation;
class QCommonStyle;
class TimeTooltip;
class QSequentialAnimationGroup;

/* Input Slider derived from QSlider */
class SeekSlider : public QSlider
{
    Q_OBJECT
    Q_PROPERTY(qreal loadingProperty READ loading WRITE setLoading)
public:
    SeekSlider( intf_thread_t *p_intf, Qt::Orientation q, QWidget *_parent = 0,
                bool _classic = false );
    virtual ~SeekSlider();
    void setChapters( SeekPoints * );

protected:
    void mouseMoveEvent( QMouseEvent *event ) Q_DECL_OVERRIDE;
    void mousePressEvent( QMouseEvent* event ) Q_DECL_OVERRIDE;
    void mouseReleaseEvent( QMouseEvent *event ) Q_DECL_OVERRIDE;
    void wheelEvent( QWheelEvent *event ) Q_DECL_OVERRIDE;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent( QEnterEvent *event ) Q_DECL_OVERRIDE;
#else
    void enterEvent( QEvent *event ) Q_DECL_OVERRIDE;
#endif
    void leaveEvent( QEvent * ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *ev) Q_DECL_OVERRIDE;

    bool eventFilter( QObject *obj, QEvent *event ) Q_DECL_OVERRIDE;

    QSize sizeHint() const Q_DECL_OVERRIDE;

    void processReleasedButton();
    qreal loading() const;
    void setLoading( qreal loading );
    int handleLength();

    float getValuePercentageFromXPos( int );
    int   getValueFromXPos( int );

private:
    intf_thread_t *p_intf;
    bool isSliding;        /* Whether we are currently sliding by user action */
    bool isJumping;              /* if we requested a jump to another chapter */
    int inputLength;                           /* InputLength that can change */
    QTimer *seekLimitTimer;
    TimeTooltip *mTimeTooltip;
    float f_buffering;
    QTime bufferingStart;
    SeekPoints* chapters;
    bool b_classic;
    bool b_seekable;

    QCommonStyle *alternativeStyle;

    /* Loading animation */
    qreal mLoading;
    QSequentialAnimationGroup *animLoading;
    QTimer *startAnimLoadingTimer;

    WheelToVLCConverter wheelEventConverter;

public slots:
    void setPosition( float, int64_t, int );
    void setSeekable( bool b ) { b_seekable = b ; }
    void updateBuffering( float );

private slots:
    void startSeekTimer();
    void updatePos();
    void inputUpdated( bool );
    void startAnimLoading();

signals:
    void sliderDragged( float );

};

/* Sound Slider inherited directly from QAbstractSlider */
class QPaintEvent;

class SoundSlider : public QAbstractSlider
{
    Q_OBJECT
public:
    SoundSlider(QWidget *_parent, float _i_step, char *psz_colors, int max );
    void setMuted( bool ); /* Set Mute status */
    /* the volume, also one past the maximum (a hotkey above a lowered
     * qt-max-volume): the fill stops at the end, the label shows it all */
    void setLevel( int );

protected:
    void sliderChange( SliderChange ) Q_DECL_OVERRIDE;
    void paintEvent( QPaintEvent *) Q_DECL_OVERRIDE;
    void wheelEvent( QWheelEvent *event ) Q_DECL_OVERRIDE;
    void mousePressEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseMoveEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseReleaseEvent( QMouseEvent * ) Q_DECL_OVERRIDE;

    void processReleasedButton();

private:
    bool isSliding; /* Whether we are currently sliding by user action */
    bool b_mouseOutside; /* Whether the mouse is outside or inside the Widget */
    int i_oldvalue; /* Store the old Value before changing */
    float f_step; /* How much do we increase each time we wheel */
    bool b_isMuted;
    int i_level; /* the label's volume: value(), or the one setLevel() got */

    QStringList colorList; /* qt-slider-colours when customised, else empty */

    WheelToVLCConverter wheelEventConverter;

    void changeValue( int x ); /* Function to modify the value from pixel x() */
    QRect trackRect() const; /* the 4 px track; the level text is on its right */
    QBrush fillBrush() const; /* from the theme or qt-slider-colours */
};

#endif
