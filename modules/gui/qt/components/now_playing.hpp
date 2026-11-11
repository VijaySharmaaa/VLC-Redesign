/*****************************************************************************
 * now_playing.hpp : now-playing bar, audio mode and mini player
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

#ifndef VLC_QT_NOW_PLAYING_HPP_
#define VLC_QT_NOW_PLAYING_HPP_

#include "qt.hpp"
#include "util/breakpoints.hpp"
#include "util/theme.hpp"

#include <QSlider>
#include <QWidget>
#include <QPixmap>

class QLabel;
class QPainter;
class QRectF;
class QListView;
class QToolButton;
class QBoxLayout;
class QGridLayout;
class QElidingLabel;
class QPropertyAnimation;

/* Shared with the library pages (components/library); timeString() is in
 * util/customwidgets.hpp */
/* The striped placeholder at 135 degrees in the theme's two tints */
void paintStripes( QPainter &, const QRectF &, qreal period, bool onVideo = false );
/* A theme font at a token px size */
QFont fontAt( Theme::TypeRole, int tokenPx, bool semibold );

/* "FLAC", "H.264": short codec names, or the fourcc */
QString codecName( const QString &fourcc );
/* The back arrow, pointing right in a right-to-left UI */
QIcon backIcon( Theme::ColorRole );

/* Cover art, cropped to fill, or the striped placeholder (DESIGN_SPEC 4) */
class ArtView : public QWidget
{
    Q_OBJECT
public:
    /* stripe: the placeholder's stripe period, radius: corners (token px),
     * onVideo: stands for the video, so its placeholder stays dark */
    ArtView( int stripe, int radius, QWidget *parent, bool onVideo = false );
    void setArt( const QString &url );  /* empty: placeholder */
protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
private:
    QPixmap art;
    int stripe, radius;
    bool onVideo;
};

/* The design's 3 px seek line: click or drag to seek, arrows step. Follows
 * the current input; disabled when it cannot seek. */
class ThinSeekBar : public QSlider
{
    Q_OBJECT
public:
    ThinSeekBar( intf_thread_t *, QWidget *parent );
    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void mousePressEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseMoveEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseReleaseEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
private:
    int valueAt( int x ) const;
    void seek();
    intf_thread_t *p_intf;
    QString timeText;   /* "0:10 of 1:00": the accessible value */
    friend class ThinSeekAccessible;
};

/* What the now-playing views share: title, meta line, format and art of
 * the current input, kept after it stops so the views keep showing the
 * last item. */
class NowPlayingInfo : public QObject
{
    Q_OBJECT
public:
    NowPlayingInfo( intf_thread_t *, QObject *parent );
    QString title, artist, album, year, format, art;
    /* "artist · album", or the format when there is no tag */
    QString metaLine() const;
signals:
    void changed();
private:
    void readItem( input_item_t * );
    intf_thread_t *p_intf;
};

/* Transport buttons shared by the bar and the mini player */
QToolButton *makeIconButton( intf_thread_t *, const char *icon, const QString &name,
                             const char *hotkey, int size, QWidget *parent );
QToolButton *makePlayButton( intf_thread_t *, int size, QWidget *parent );

/* DESIGN_SPEC 4: the 76 px bar under the library pages once something
 * played. The main interface decides when it shows. */
class NowPlayingBar : public QWidget
{
    Q_OBJECT
public:
    NowPlayingBar( intf_thread_t *, NowPlayingInfo *, QWidget *parent );
    QWidget *videoHost() const { return thumb; }   /* VideoWidget::setHost */
signals:
    void expandRequested();
protected:
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
private:
    void refresh();
    intf_thread_t *p_intf;
    NowPlayingInfo *info;
    QWidget *left;
    ArtView *thumb;
    QElidingLabel *title, *meta;
    QLabel *elapsed, *length;
};

/* DESIGN_SPEC 5: art, track header, the playlist as a track list, and the
 * spectrum strip. */
class AudioModePage : public QWidget
{
    Q_OBJECT
public:
    AudioModePage( intf_thread_t *, NowPlayingInfo *, QWidget *parent );
public slots:
    void setBreakpoint( BreakpointWatcher::Size );
signals:
    void libraryRequested();
protected:
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;
private:
    void refresh();
    void fitArt();
    void readInset();
    intf_thread_t *p_intf;
    NowPlayingInfo *info;
    QBoxLayout *columns;
    QWidget *leftColumn;
    ArtView *art;
    QLabel *formatChip, *yearChip, *title;
    QElidingLabel *meta;
    QListView *tracks;
    QWidget *spectrum;
    BreakpointWatcher::Size breakpoint;
    int artMax;
};

/* DESIGN_SPEC 9: a small always-on-top window with the transport, the
 * video playing in its art (the main window is hidden meanwhile). */
class MiniPlayer : public QWidget
{
    Q_OBJECT
public:
    MiniPlayer( intf_thread_t *, NowPlayingInfo *, QWidget *parent );
    void popIn();
    QWidget *videoHost() const { return art; }   /* VideoWidget::setHost */
signals:
    void restoreRequested();
    void keyPressed( QKeyEvent * );
protected:
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;
    void mousePressEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseMoveEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;
    void closeEvent( QCloseEvent * ) Q_DECL_OVERRIDE;
private:
    void refresh();
    intf_thread_t *p_intf;
    NowPlayingInfo *info;
    ArtView *art;
    QElidingLabel *title;
    QLabel *time;
    QPoint dragOffset;
    QPropertyAnimation *fade;
};

#endif
