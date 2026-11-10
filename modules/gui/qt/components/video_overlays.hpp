/*****************************************************************************
 * video_overlays.hpp : paused badge, buffering ring and immersive bar
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

#ifndef QVLC_VIDEO_OVERLAYS_H_
#define QVLC_VIDEO_OVERLAYS_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/overlay_host.hpp"

#include <QAtomicInt>

class QAbstractButton;
class QVariantAnimation;
class QPropertyAnimation;
class QParallelAnimationGroup;
class QVBoxLayout;
class InputControlsWidget;
class ControlsWidget;
class InputManager;

/* Centre of the video while it is paused; a click resumes. */
class PausedBadge : public OverlayWindow
{
public:
    PausedBadge( intf_thread_t *, QWidget *owner, QWidget *video );

private:
    void refresh();
    void place();

    InputManager *im;
    OverlayHost *host;
    QAbstractButton *button;
    QVariantAnimation *pop;
    bool wanted;
};

/* Ring, "Buffering... N%" and the media's URL over the player view, while
 * the input fills its cache. Stays with the last % if it never completes. */
class BufferingOverlay : public OverlayWindow
{
public:
    BufferingOverlay( intf_thread_t *, QWidget *owner, QWidget *area );
    /* the player view (video or cover art) is the current page */
    void setOnPlayer( bool );

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    bool buffering() const;
    void refresh();
    void place();

    intf_thread_t *p_intf;
    OverlayHost *host;
    QVariantAnimation *spin;
    QTimer delay;
    QString url;
    float level;
    bool onPlayer;
};

/* The immersive layout's deck: the standard seek and button bars floating
 * over the bottom of the player view. Hides after an idle time while a
 * video plays; mouse moves over the video (the vout's "mouse-moved") and
 * keys in the main window bring it back. */
class ImmersiveBar : public OverlayWindow
{
public:
    ImmersiveBar( intf_thread_t *, QWidget *owner, QWidget *area, bool advanced );
    virtual ~ImmersiveBar();

    void setActive( bool );
    void rebuild( bool advanced );   /* after a toolbar or advanced change */

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void customEvent( QEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    static int MouseMovedCB( vlc_object_t *, const char *,
                             vlc_value_t, vlc_value_t, void * );
    void setVoutList( vout_thread_t **, int );
    void wake();
    void sleepIfIdle();
    void animate( bool in );
    void place();
    QRect restingRect() const;

    intf_thread_t *p_intf;
    OverlayHost *host;
    QVBoxLayout *layout;
    InputControlsWidget *inputC;
    ControlsWidget *controls;
    QList<vout_thread_t *> vouts;
    QAtomicInt wakePosted;      /* one wake event in flight at a time */
    QTimer idle;
    QParallelAnimationGroup *anim;
    QPropertyAnimation *fade, *slide;
    bool active, asleep;
};

#endif
