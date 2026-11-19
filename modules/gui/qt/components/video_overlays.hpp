/*****************************************************************************
 * video_overlays.hpp : paused badge, buffering ring, immersive bar, header
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
#include <QIcon>
#include <QPointer>

class QAbstractButton;
class QVariantAnimation;
class QPropertyAnimation;
class QParallelAnimationGroup;
class QVBoxLayout;
class InputControlsWidget;
class ControlsWidget;
class InputManager;
class NowPlayingInfo;
class PlayerHeader;

/* Where the vout draws the current video inside area (global px): the
 * track's size and aspect, or the user's aspect ratio, fitted and centred.
 * area itself when no video size is known. */
QRect videoPictureRect( intf_thread_t *, const QRect &area );

/* The playing video track's format as the demuxer reported it: the
 * selected track ("video-es"), else the first; false without one.
 * p_palette is not copied. */
bool currentVideoFormat( intf_thread_t *, video_format_t * );

/* The floating decks' glass (the immersive bar, the fullscreen controller):
 * fill and border over the whole widget, rounded by radius */
void paintDeckGlass( QWidget *, int radius );

/* Centre of the video while it is paused; a click resumes. */
class PausedBadge : public OverlayWindow
{
public:
    PausedBadge( intf_thread_t *, QWidget *owner, QWidget *video );

private:
    void refresh();
    void place();

    intf_thread_t *p_intf;
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
    float level() const;
    bool buffering() const;
    void refresh();
    void place();

    intf_thread_t *p_intf;
    OverlayHost *host;
    QVariantAnimation *spin;
    QTimer delay;
    QString url;
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
    void setHeader( PlayerHeader * );   /* sleeps and wakes with the bar */

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void customEvent( QEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    static int MouseMovedCB( vlc_object_t *, const char *,
                             vlc_value_t, vlc_value_t, void * );
    void setVoutList( vout_thread_t **, int );
    bool takeFocus();
    void wake();
    void sleepIfIdle();
    void setAsleep( bool );
    void animate( bool in );
    void place();
    QRect restingRect() const;
    void liftSubtitles();

    intf_thread_t *p_intf;
    QWidget *area;
    OverlayHost *host;
    QVBoxLayout *layout;
    InputControlsWidget *inputC;
    ControlsWidget *controls;
    QList<vout_thread_t *> vouts;
    int lift;                   /* added to each vout's "sub-margin", in its units */
    QAtomicInt wakePosted;      /* one wake event in flight at a time */
    QTimer idle;
    QParallelAnimationGroup *anim;
    QPropertyAnimation *fade, *slide;
    QPointer<PlayerHeader> header;
    bool active, asleep;
};

/* The player's top row over the video (renders 1.2.x): back to the library,
 * the title and the meta line on a scrim that fades into the picture. The
 * strip lets the mouse through to the video; the back button takes it in a
 * small window of its own, reached with Shift+Tab from the player. In the
 * immersive layout it sleeps and wakes with the deck, in the normal one with
 * the mouse cursor while a video plays. */
class PlayerHeader : public OverlayWindow
{
public:
    PlayerHeader( intf_thread_t *, QWidget *owner, QWidget *video, NowPlayingInfo * );
    virtual ~PlayerHeader();

    QAbstractButton *backButton() const { return back; }
    void setActive( bool );    /* the video page, neither full screen nor minimal */
    void setAsleep( bool );    /* the immersive deck's idle hide */
    void setCursorHidden( bool );  /* the normal layout's: VLC hid or showed the cursor */
    void forgetCursor();       /* the immersive layout takes over */
    bool hovered() const;      /* the mouse on its back button */

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    void refresh();
    void place();
    void followCursor();
    void setOpacity( qreal );
    QWidget *deckStop( bool first ) const;
    QFont titleFont() const;
    int rowHeight() const;
    QRect backRect( int width ) const;  /* in the strip, mirrored right to left */

    intf_thread_t *p_intf;
    NowPlayingInfo *info;
    QWidget *video;
    OverlayHost *host;
    OverlayWindow *backWindow;
    QAbstractButton *back;
    QVariantAnimation *fade;
    QString meta;
    bool active, asleep, cursorHidden;
    QPoint hiddenAt;      /* the cursor when VLC hid it */
    QTimer moveCheck;     /* after a show that was not the user's */
};

#endif
