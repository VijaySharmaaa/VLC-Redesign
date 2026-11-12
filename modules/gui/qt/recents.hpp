/*****************************************************************************
 * recents.hpp : Recents MRL (menu)
 *****************************************************************************
 * Copyright © 2008-2014 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *          Jean-baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef QVLC_RECENTS_H_
#define QVLC_RECENTS_H_

#include "qt.hpp"
#include "util/singleton.hpp"

#include <QObject>
#include <QStringList>
class QDateTime;
class QRegularExpression;
class QSignalMapper;

#define RECENTS_LIST_SIZE 30

class Open
{
public:
    static int openMRL( intf_thread_t*,
                        const QString &,
                        bool b_start = true,
                        bool b_playlist = true);

    static int openMRLwithOptions( intf_thread_t*,
                                   const QString &,
                                   QStringList *options,
                                   bool b_start = true,
                                   bool b_playlist = true,
                                   const char* title = NULL);
};

class RecentsMRL : public QObject, public Singleton<RecentsMRL>
{
    Q_OBJECT
    friend class Singleton<RecentsMRL>;

public:
    void addRecent( const QString & );
    QStringList recentList();
    QSignalMapper *signalMapper;

    /* time() of an item played to its end; stock VLC reads it as no
     * resume point, as -1 (never closed) and 0 (stopped too soon) */
    enum { Ended = -2 };
    /* resume position in ms, -1 or 0 when there is none, or Ended */
    int time( const QString &mrl );
    /* when the input closes: position (-1 for none, or Ended) and length
     * in microseconds */
    void setTime( const QString &mrl, const int64_t time, const int64_t length );
    /* length in ms, 0 when unknown (entries from older configurations) */
    int64_t length( const QString &mrl );
    /* while mrl plays: its length in ms as the input knows it so far;
     * true when it changed. No changed(): it moves while VBR plays, so the
     * caller updates what shows it in place */
    bool setLength( const QString &mrl, int64_t length );
    /* last played, invalid when unknown */
    QDateTime date( const QString &mrl );
    void removeRecent( const QString &mrl );
    /* P4-4: the next input of mrl seeks to its saved time without asking
     * (qt-continue); true once, for that input (InputManager::setInput) */
    void resumeOnce( const QString &mrl ) { resumeMrl = mrl; }
    bool takeResume( const QString &mrl )
    {
        const bool b = !resumeMrl.isEmpty() && resumeMrl == mrl;
        resumeMrl.clear();
        return b;
    }

signals:
    /* the list, an order, a time or a date changed (not setLength) */
    void changed();

private:
    RecentsMRL( intf_thread_t* _p_intf );
    virtual ~RecentsMRL();

    intf_thread_t *p_intf;

    /* parallel lists, saved under RecentsMRL/: list, times (ms), lengths
     * (ms) and dates (ISO 8601, UTC); stock VLC ignores the last two, and
     * listForExtras, the list they belong to */
    QStringList   recents;
    QStringList   times;
    QStringList   lengths;
    QStringList   dates;
    QString       resumeMrl;
    QRegularExpression *filter;
    bool          isActive;

    void load();
    void save();

public slots:
    void clear();
    void playMRL( const QString & );
};

#endif
