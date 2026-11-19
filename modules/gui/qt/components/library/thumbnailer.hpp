/*****************************************************************************
 * thumbnailer.hpp : video frames and watch state for the library cards
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

#ifndef VLC_QT_THUMBNAILER_HPP_
#define VLC_QT_THUMBNAILER_HPP_

#include "qt.hpp"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QRunnable>
#include <QSet>
#include <QStringList>

class QProcess;
class QTimer;

/* Frames of local videos for the library cards (issue I1-3). VLC 3 has no
 * thumbnailer API: the package's own vlc.exe grabs one frame, headless,
 * with the scene filter, into the cache directory, keyed by path, size and
 * modification time. A few processes at a time (ThumbWorkersMax, half the
 * cores), each killed as soon as its picture is written, after a timeout,
 * when the page is left (cancel) and when VLC quits; the latest asked frame
 * (a card on screen) goes first. The user's media are only read. Also lists
 * local folders (the videos in them, and a video further down for a folder
 * without its own) and tells, from the recents (RecentsMRL), where a file
 * resumes and whether it was watched. One for Home and Videos (instance):
 * one process budget, one cache. */
class Thumbnailer : public QObject
{
    Q_OBJECT
public:
    static Thumbnailer *instance( intf_thread_t * );
    ~Thumbnailer();

    /* The frame (a PNG file) of a local video, empty while it is made or
     * when there is none: changed() follows a new one. lengthMs <= 0 when
     * unknown. Paths are native or Qt paths. */
    QString frame( const QString &path, int64_t lengthMs );
    /* The videos of a local folder, by name; NULL while it is listed */
    const QStringList *videos( const QString &dir );
    /* The frame of a folder's first video, or of the first one in its
     * subfolders when it has none of its own; empty as frame() */
    QString folderFrame( const QString &dir );
    /* Where a file resumes, 0..1, or -1 when it does not */
    qreal resume( const QString &path ) const;
    /* How many videos of a listed folder were played to their end */
    int watchedIn( const QString &dir );
    /* Drops the queue, the listings and the failures: the page was left.
     * Nothing starts again until a frame or a listing is asked for. */
    void cancel();
    /* Removes every cached frame (the recents were cleared or are off):
     * they tell what was in the library */
    static void removeCache();

    static bool isVideo( const QString &path );

signals:
    void changed();

private slots:
    void listed( const QString &dir, const QStringList &videos, const QString &cover );
    void loadRecents();

private:
    struct Request
    {
        QString key, path, file;
        int atSec;
    };
    struct Frame
    {
        QString file;                        /* a PNG */
        int epoch;                           /* when the file was last looked at */
    };
    struct Job
    {
        Request r;
        QString tmpFile;
        QProcess *process;
        QElapsedTimer clock;
        unsigned sarNum, sarDen;             /* den 0 until its log tells */
    };
    Thumbnailer( intf_thread_t *, QObject *parent );
    /* asked: the epoch of the frame() that started the probe */
    void probed( const QString &path, const QString &file, bool cached, int asked );
    void startNext();
    void start( const Request & );
    void check();
    void finish( QProcess *, bool ok );
    static void stopProcess( QProcess *, const QString &tmpFile );
    void readLog( QProcess * );
    static bool saveSquare( const QString &from, const QString &to, unsigned num, unsigned den );

    intf_thread_t *p_intf;
    int epoch;                               /* cancel()s so far */
    int serial;                              /* of the part files */
    QString cacheDir;
    QHash<QString, Frame> frames;            /* key: its frame */
    QSet<QString> pendingFrames, failedFrames, pendingDirs;
    QHash<QString, QStringList> dirs;
    QHash<QString, QString> covers;          /* key: of dirs, a video under it */
    QHash<QString, int> watchedCounts;       /* key: of dirs, as painted */
    QHash<QString, int64_t> lengths;         /* key: length asked with, while probed */
    QHash<QString, qreal> played;            /* key: resume point, 1 when watched */
    QList<Request> queue;                    /* the latest asked first */
    QList<Job> jobs;                         /* running */
    int maxJobs;
    QTimer *poll;                            /* while jobs run */
};

/* The probe (size and date, the cached frame) of a file, or the listing of
 * a folder, on LibraryData::filePool(): an offline share can block */
class ThumbJob : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ThumbJob( const QString &_path, const QString &_cacheDir, bool _listing )
        : path( _path ), cacheDir( _cacheDir ), listing( _listing ) {}
    void run() Q_DECL_OVERRIDE;

signals:
    /* file empty: no such file */
    void probed( const QString &path, const QString &file, bool cached );
    void listed( const QString &dir, const QStringList &videos, const QString &cover );

private:
    QString path, cacheDir;
    bool listing;
};

#endif
