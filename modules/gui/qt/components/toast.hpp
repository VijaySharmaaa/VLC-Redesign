/*****************************************************************************
 * toast.hpp : short notice at the bottom of the main window
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

#ifndef QVLC_TOAST_H_
#define QVLC_TOAST_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/overlay_host.hpp"

#include <QAtomicInt>
#include <QPointer>

class QPropertyAnimation;
class QParallelAnimationGroup;

/* Bottom-centre of the content area; rises in, holds, disappears. A new
 * message replaces the one shown and restarts the hold. Also announces
 * every snapshot VLC saves ("snapshot-file"). */
class Toast : public OverlayWindow
{
    Q_OBJECT
public:
    Toast( intf_thread_t *, QWidget *owner, QWidget *content );
    virtual ~Toast();

    void showMessage( const QString & );
    /* Over a video the toast rises above the subtitles' lines */
    void setOverVideo( bool );

signals:
    void snapshotSaved( const QString & );  /* emitted from the vout thread */

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;

private:
    static int SnapshotCB( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * );
    void place();
    void fitText();
    QPoint restingPos() const;
    void onSnapshot( const QString & );

    intf_thread_t *p_intf;
    QPointer<QWidget> content;
    OverlayHost *host;
    QString text;
    QParallelAnimationGroup *enter;
    QPropertyAnimation *fade, *rise;
    QTimer hold;
    bool overVideo;
    QAtomicInt canShow;     /* host->targetShown(), for SnapshotCB's thread */
};

#endif
