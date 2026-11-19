/*****************************************************************************
 * thumbnailer.cpp : video frames and watch state for the library cards
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* before vlc_threads.h, whose msleep macro breaks qthread.h */
#include <QThreadPool>

#include "components/library/thumbnailer.hpp"
#include "components/library/library_widgets.hpp"   /* LibraryData::filePool */
#include "input_manager.hpp"                        /* THEMIM */
#include "recents.hpp"
#include "util/theme.hpp"

#include <vlc_configuration.h>          /* config_GetUserDir */
#include <vlc_interface.h>              /* EXTENSIONS_VIDEO */

#include <climits>                       /* INT_MAX */
#ifdef _WIN32
# include <windows.h>                    /* GetFileAttributesW */
/* Windows 10 1709 names, missing from older SDKs */
# ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#  define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
# endif
# ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#  define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
# endif
#endif

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

using namespace RedesignTokens;

/* One spelling per file, as the recents, the listings and the cards get it */
static QString keyOf( const QString &path )
{
    const QString key = QDir::cleanPath( QDir::fromNativeSeparators( path ) );
#ifdef _WIN32
    return key.toLower();
#else
    return key;
#endif
}

static const QStringList &videoFilters()
{
    static const QStringList filters = QString( EXTENSIONS_VIDEO ).split( ';' );
    return filters;
}

bool Thumbnailer::isVideo( const QString &path )
{
    return hasExtension( path, videoFilters() );
}

static QString thumbDirOf( vlc_userdir_t type )
{
    char *psz = config_GetUserDir( type );
    const QString dir = psz ? QDir::fromNativeSeparators( qfu( psz ) ) + QLatin1String( "/thumbnails" )
                            : QString();
    free( psz );
    return dir;
}

/* The frame cache; empty when there is none */
static QString cacheDirectory()
{
#ifdef _WIN32
    /* VLC's cache dir is in the roaming profile: the local one instead,
     * unless VLC runs portable (a "portable" folder next to vlc.exe, as
     * config_GetAppDir checks), where every dir of VLC is that folder */
    if( !QFileInfo( QCoreApplication::applicationDirPath() + QLatin1String( "/portable" ) ).isDir() )
    {
        const QString local = QStandardPaths::writableLocation( QStandardPaths::GenericDataLocation );
        if( !local.isEmpty() )
            return local + QLatin1String( "/vlc/thumbnails" );
    }
#endif
    return thumbDirOf( VLC_CACHE_DIR );
}

/* Trims the frame cache once per VLC run, in the first probe: the part
 * files of grabs cut short (VLC killed; no grab runs longer than
 * ThumbTimeoutMs), then the oldest frames past ThumbCacheMaxMB.
 * shortcut: oldest by when the frame was made, not last shown; keep a
 * last-use date if often shown frames get made again */
static void pruneCache( const QString &dir )
{
    const QDateTime now = QDateTime::currentDateTime();
    QFileInfoList frames;
    qint64 total = 0;
    /* oldest first */
    foreach( const QFileInfo &info, QDir( dir ).entryInfoList(
                 QStringList() << "*.png" << "*.swp", QDir::Files, QDir::Time | QDir::Reversed ) )
    {
        if( info.suffix() == QLatin1String( "swp" )
         || info.completeBaseName().endsWith( QLatin1String( ".part" ) ) )
        {
            if( info.lastModified().msecsTo( now ) > Library::ThumbTimeoutMs )
                QFile::remove( info.filePath() );
            continue;
        }
        frames << info;
        total += info.size();
    }
    const qint64 max = qint64( Library::ThumbCacheMaxMB ) << 20;
    for( int i = 0; i < frames.count() && total > max; i++ )
        if( QFile::remove( frames.at( i ).filePath() ) )
            total -= frames.at( i ).size();
#ifdef _WIN32
    /* the frames earlier builds kept in the roaming profile */
    const QString roaming = thumbDirOf( VLC_CACHE_DIR );
    if( !roaming.isEmpty() && QDir( roaming ) != QDir( dir ) )
        QDir( roaming ).removeRecursively();
#endif
}

/**********************************************************************
 * ThumbJob
 **********************************************************************/
void ThumbJob::run()
{
    if( listing )
    {
        QDir dir( path );
        dir.setNameFilters( videoFilters() );
        QStringList videos;
        foreach( const QFileInfo &info, dir.entryInfoList( QDir::Files | QDir::Readable,
                                                           QDir::Name | QDir::IgnoreCase ) )
            videos << info.filePath();
        /* a folder of folders (a series by season): the first video down
         * its subfolders, breadth first, in a bounded number of them (links
         * can loop) */
        QString cover = videos.isEmpty() ? QString() : videos.first();
        QStringList todo( path );
        for( int visited = 0; cover.isEmpty() && !todo.isEmpty()
                              && visited < Library::ThumbCoverDirs; visited++ )
        {
            QDir sub( todo.takeFirst() );
            if( visited > 0 )
            {
                sub.setNameFilters( videoFilters() );
                const QFileInfoList files = sub.entryInfoList( QDir::Files | QDir::Readable,
                                                               QDir::Name | QDir::IgnoreCase );
                if( !files.isEmpty() )
                    cover = files.first().filePath();
                sub.setNameFilters( QStringList() );
            }
            foreach( const QFileInfo &info, sub.entryInfoList( QDir::Dirs | QDir::NoDotAndDotDot
                                                               | QDir::Readable,
                                                               QDir::Name | QDir::IgnoreCase ) )
                todo << info.filePath();
        }
        emit listed( path, videos, cover );
        return;
    }
    const QFileInfo info( path );
    if( !info.isFile() || !QDir().mkpath( cacheDir ) )
    {
        emit probed( path, QString(), false );
        return;
    }
    /* before any probe hands out a frame, so none is one it deletes */
    {
        static QMutex pruneLock;
        static bool pruned = false;
        QMutexLocker locker( &pruneLock );
        if( !pruned )
        {
            pruneCache( cacheDir );
            pruned = true;
        }
    }
    /* a replaced or edited file gets a new frame */
    const QByteArray id = ( keyOf( path ) + QLatin1Char( '|' ) + QString::number( info.size() )
                          + QLatin1Char( '|' )
                          + QString::number( info.lastModified().toMSecsSinceEpoch() ) ).toUtf8();
    /* a PNG of some 50 KB per video, trimmed by ThumbPrune */
    const QString file = cacheDir + QLatin1Char( '/' )
                       + QString::fromLatin1( QCryptographicHash::hash( id, QCryptographicHash::Sha1 ).toHex() )
                       + QLatin1String( ".png" );
    const bool cached = QFileInfo( file ).size() > 0;
#ifdef _WIN32
    /* an online-only file (OneDrive, other cloud folders) would be
     * downloaded by the grab: its frame only when it is on the disk */
    const DWORD attrs = GetFileAttributesW(
        reinterpret_cast<const wchar_t *>( QDir::toNativeSeparators( path ).utf16() ) );
    if( !cached && attrs != INVALID_FILE_ATTRIBUTES
     && ( attrs & ( FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_OPEN
                  | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS ) ) )
    {
        emit probed( path, QString(), false );
        return;
    }
#endif
    emit probed( path, file, cached );
}

/**********************************************************************
 * Thumbnailer
 **********************************************************************/
/* kept by the input manager, which outlives the pages */
static QPointer<Thumbnailer> shared;

Thumbnailer *Thumbnailer::instance( intf_thread_t *p_intf )
{
    if( !shared )
        shared = new Thumbnailer( p_intf, THEMIM );
    return shared;
}

Thumbnailer::Thumbnailer( intf_thread_t *_p_intf, QObject *parent )
    : QObject( parent ), p_intf( _p_intf ), epoch( 0 ), serial( 0 ),
      /* each one decodes on one thread (--avcodec-threads=1): on half the
       * cores, the player and the GUI keep the rest */
      maxJobs( qBound( 1, QThread::idealThreadCount() / 2, int( Library::ThumbWorkersMax ) ) )
{
    cacheDir = cacheDirectory();

    poll = new QTimer( this );
    poll->setInterval( Library::ThumbPollMs );
    connect( poll, &QTimer::timeout, this, &Thumbnailer::check );

    connect( RecentsMRL::getInstance( p_intf ), &RecentsMRL::changed,
             this, &Thumbnailer::loadRecents );
    loadRecents();
}

Thumbnailer::~Thumbnailer()
{
    /* VLC quits: every probe, also those let go (stopProcess) whose
     * deleteLater no longer runs, killed first, then waited for together,
     * ThumbKillWaitMs in all; one still alive then (an antivirus can hold
     * a dying process) is left behind, as ~QProcess would wait 30 s */
    const QList<QProcess *> all = findChildren<QProcess *>();
    foreach( QProcess *p, all )
    {
        p->disconnect();
        p->kill();
    }
    QElapsedTimer clock;
    clock.start();
    foreach( QProcess *p, all )
    {
        const int left = qMax( 0, int( Library::ThumbKillWaitMs - clock.elapsed() ) );
        if( !p->waitForFinished( left ) && p->state() != QProcess::NotRunning )
            p->setParent( NULL );
    }
    /* this run's part files; one still held goes in the next run's prune */
    if( cacheDir.isEmpty() )
        return;
    QDir dir( cacheDir );
    foreach( const QString &name, dir.entryList( QStringList(
                 QString( "*.%1.*.part.*" ).arg( QCoreApplication::applicationPid() ) ), QDir::Files ) )
        dir.remove( name );
}

void Thumbnailer::removeCache()
{
    if( shared )
    {
        shared->cancel();
        shared->frames.clear();
    }
    /* shortcut: removed on the GUI thread (at most ThumbCacheMaxMB of
     * frames, once per clear); move it to filePool if it ever stalls */
    const QString dir = cacheDirectory();
    if( !dir.isEmpty() )
        QDir( dir ).removeRecursively();
}

QString Thumbnailer::frame( const QString &path, int64_t lengthMs )
{
    const QString key = keyOf( path );
    if( cacheDir.isEmpty() || !isVideo( path ) || failedFrames.contains( key ) )
        return QString();
    /* a frame known before the last cancel() is shown while the file is
     * looked at again (a replaced file gets a new frame) */
    QHash<QString, Frame>::const_iterator it = frames.constFind( key );
    if( it != frames.constEnd() && it.value().epoch == epoch )
        return it.value().file;
    const QString shown = it != frames.constEnd() ? it.value().file : QString();
    if( pendingFrames.contains( key ) )
    {
        /* painted again, so on screen: ahead of the cards scrolled away */
        for( int i = 1; i < queue.count(); i++ )
            if( queue.at( i ).key == key )
            {
                queue.move( i, 0 );
                break;
            }
    }
    else
    {
        pendingFrames.insert( key );
        lengths.insert( key, lengthMs );
        ThumbJob *job = new ThumbJob( path, cacheDir, false );
        /* queued: a result that comes after this is gone is dropped */
        const int asked = epoch;
        connect( job, &ThumbJob::probed, this,
                 [this, asked]( const QString &p, const QString &file, bool cached ) {
                     probed( p, file, cached, asked );
                 }, Qt::QueuedConnection );
        LibraryData::filePool()->start( job );
    }
    return shown;
}

const QStringList *Thumbnailer::videos( const QString &dir )
{
    const QString key = keyOf( dir );
    QHash<QString, QStringList>::const_iterator it = dirs.constFind( key );
    if( it != dirs.constEnd() )
        return &it.value();
    if( !pendingDirs.contains( key ) )
    {
        pendingDirs.insert( key );
        ThumbJob *job = new ThumbJob( dir, cacheDir, true );
        connect( job, &ThumbJob::listed, this, &Thumbnailer::listed, Qt::QueuedConnection );
        LibraryData::filePool()->start( job );
    }
    return NULL;
}

QString Thumbnailer::folderFrame( const QString &dir )
{
    if( !videos( dir ) )
        return QString();
    const QString cover = covers.value( keyOf( dir ) );
    return cover.isEmpty() ? QString() : frame( cover, 0 );
}

qreal Thumbnailer::resume( const QString &path ) const
{
    const qreal at = played.value( keyOf( path ), -1 );
    return at < 1 ? at : -1;
}

int Thumbnailer::watchedIn( const QString &dir )
{
    /* counted once, not at each paint of a folder of thousands */
    const QString key = keyOf( dir );
    QHash<QString, int>::const_iterator it = watchedCounts.constFind( key );
    if( it != watchedCounts.constEnd() )
        return it.value();
    int n = 0;
    foreach( const QString &video, dirs.value( key ) )
        n += played.value( keyOf( video ), -1 ) >= 1;
    watchedCounts.insert( key, n );
    return n;
}

void Thumbnailer::cancel()
{
    /* probes and listings not started yet go (the pool runs only ours), so
     * a page shown again does not queue behind them; those still running
     * are dropped as they come (probed); a page that asks the same frame
     * or folder again starts its own */
    LibraryData::filePool()->clear();
    pendingDirs.clear();
    epoch++;
    pendingFrames.clear();
    lengths.clear();
    queue.clear();
    foreach( const Job &j, jobs )
        stopProcess( j.process, j.tmpFile );
    jobs.clear();
    poll->stop();
    /* listed again when shown: files come and go */
    dirs.clear();
    covers.clear();
    watchedCounts.clear();
    /* tried again when shown: a share may be back, a grab may have timed
     * out while a film played */
    failedFrames.clear();
    if( frames.count() > Library::ThumbFramesMax )
        frames.clear();
}

void Thumbnailer::probed( const QString &path, const QString &file, bool cached, int asked )
{
    if( asked != epoch )
        return;
    const QString key = keyOf( path );
    const int64_t lengthMs = lengths.take( key );
    if( file.isEmpty() )
    {
        pendingFrames.remove( key );
        failedFrames.insert( key );
        frames.remove( key );
        return;
    }
    if( cached )
    {
        pendingFrames.remove( key );
        const bool fresh = frames.value( key ).file != file;
        const Frame f = { file, epoch };
        frames.insert( key, f );
        if( fresh )
            emit changed();
        return;
    }
    /* the middle of a film opens on a scene, its start often on black */
    const Request r = { key, path, file,
                        lengthMs > 0 ? int( lengthMs / 1000 * Library::ThumbAtPercent / 100 )
                                     : Library::ThumbUnknownAtS };
    queue.prepend( r );
    /* scrolled past: dropped, asked again when painted again */
    while( queue.count() > Library::ThumbQueueMax )
        pendingFrames.remove( queue.takeLast().key );
    startNext();
}

void Thumbnailer::listed( const QString &dir, const QStringList &videos, const QString &cover )
{
    pendingDirs.remove( keyOf( dir ) );
    dirs.insert( keyOf( dir ), videos );
    covers.insert( keyOf( dir ), cover );
    watchedCounts.remove( keyOf( dir ) );
    emit changed();
}

void Thumbnailer::loadRecents()
{
    RecentsMRL *recents = RecentsMRL::getInstance( p_intf );
    played.clear();
    watchedCounts.clear();
    foreach( const QString &mrl, recents->recentList() )
    {
        const QString path = localPath( mrl );
        if( path.isEmpty() )
            continue;
        const int64_t timeMs = recents->time( mrl ), lengthMs = recents->length( mrl );
        /* shortcut: RecentsMRL keeps only its last 30 items; keep a watched
         * flag per file if older ones showing unwatched misleads */
        if( timeMs == RecentsMRL::Ended )
            played.insert( keyOf( path ), 1 );
        else if( timeMs > 0 && lengthMs > timeMs )
            played.insert( keyOf( path ), qreal( timeMs ) / lengthMs );
    }
    emit changed();
}

void Thumbnailer::startNext()
{
    if( queue.isEmpty() )
        return;
    /* shortcut: VLC's own program by the build's name; a fork that renames
     * it must rename PACKAGE too (configure's AC_INIT) */
    if( QFileInfo( QCoreApplication::applicationFilePath() ).baseName()
            .compare( QLatin1String( PACKAGE ), Qt::CaseInsensitive ) )
    {
        /* hosted by another program: no vlc.exe to ask */
        foreach( const Request &r, queue )
        {
            pendingFrames.remove( r.key );
            failedFrames.insert( r.key );
        }
        queue.clear();
        return;
    }
    while( jobs.count() < maxJobs && !queue.isEmpty() )
        start( queue.takeFirst() );
}

void Thumbnailer::start( const Request &r )
{
    Job j;
    j.r = r;
    /* one name per grab: a killed one's file goes only once it is let go */
    const QString prefix = QFileInfo( r.file ).completeBaseName()
                         + QString( ".%1.%2.part" ).arg( QCoreApplication::applicationPid() )
                                                   .arg( ++serial );
    j.tmpFile = cacheDir + QLatin1Char( '/' ) + prefix + QLatin1String( ".png" );

    QStringList args;
    /* no interface, window, sound, subtitles or text over the picture; not
     * the user's configuration (one instance, extra interfaces, the media
     * library) and not the crash report question; local files only; no
     * Lua scripts (playlist probes) and no preparsing of the item */
    args << "--intf=dummy" << "--dummy-quiet" << "--ignore-config" << "--no-one-instance"
         << "--no-media-library" << "--no-crashdump" << "--quiet" << "--no-stats"
         << "--no-metadata-network-access" << "--no-lua" << "--no-auto-preparse"
         << "--vout=dummy" << "--no-audio"
         << "--no-spu" << "--no-osd" << "--no-video-title-show" << "--no-sub-autodetect-file"
         /* the scene filter takes system memory pictures; one decoding
          * thread, as several probes run at once */
         << "--avcodec-hw=none" << "--avcodec-threads=1"
         /* the first picture at once: no file cache (1 s) before it shows,
          * a seek to the key frame before the time instead of decoding up
          * to the time, and nothing dropped for being late */
         << "--file-caching=0" << "--input-fast-seek"
         << "--no-drop-late-frames" << "--no-skip-frames"
         << QString( "--start-time=%1" ).arg( r.atSec )
         << QString( "--stop-time=%1" ).arg( r.atSec + Library::ThumbSpanS )
         << "--video-filter=scene" << "--scene-format=png" << "--scene-replace"
         /* the first picture only */
         << QString( "--scene-ratio=%1" ).arg( INT_MAX )
         << QString( "--scene-width=%1" ).arg( Library::ThumbWidth )
         << "--scene-path=" + QDir::toNativeSeparators( cacheDir )
         << "--scene-prefix=" + prefix
         << "--play-and-exit"
         << QDir::toNativeSeparators( QFileInfo( r.path ).absoluteFilePath() )
         << "vlc://quit";

    QProcess *process = new QProcess( this );
    j.process = process;
    process->setProcessChannelMode( QProcess::ForwardedChannels );
    connect( process, static_cast<void (QProcess::*)( int, QProcess::ExitStatus )>( &QProcess::finished ),
             this, [this, process]( int, QProcess::ExitStatus status ) {
        finish( process, status == QProcess::NormalExit );
    } );
    connect( process, &QProcess::errorOccurred, this, [this, process]( QProcess::ProcessError e ) {
        if( e == QProcess::FailedToStart )
            finish( process, false );
    } );
    j.clock.start();
    jobs.append( j );
    process->start( QCoreApplication::applicationFilePath(), args );
    /* not when it failed to start: finish() ran inside start() already */
    if( !jobs.isEmpty() )
        poll->start();
}

/* The scene filter writes a .swp file and renames it: once the picture is
 * there it is whole, and the rest of the playback is not waited for */
void Thumbnailer::check()
{
    QList<QProcess *> done, late;
    foreach( const Job &j, jobs )
        if( QFileInfo( j.tmpFile ).size() > 0 )
            done << j.process;
        else if( j.clock.elapsed() > Library::ThumbTimeoutMs )
            late << j.process;
    foreach( QProcess *p, done )
        finish( p, true );
    foreach( QProcess *p, late )
        finish( p, false );
}

void Thumbnailer::finish( QProcess *process, bool ok )
{
    int i = 0;
    while( i < jobs.count() && jobs.at( i ).process != process )
        i++;
    if( i == jobs.count() )
        return;   /* already finished */
    Job j = jobs.takeAt( i );
    if( jobs.isEmpty() )
        poll->stop();
    /* the picture is whole once renamed by the scene filter, even while
     * the process still runs */
    if( ok && QFileInfo( j.tmpFile ).size() > 0
     && ( QFile::rename( j.tmpFile, j.r.file ) || QFileInfo( j.r.file ).size() > 0 ) )
    {
        pendingFrames.remove( j.r.key );
        const Frame f = { j.r.file, epoch };
        frames.insert( j.r.key, f );
        emit changed();
    }
    else if( ok && j.r.atSec > 0 )
    {
        /* past the end of a short video: its first picture then */
        j.r.atSec = 0;
        queue.prepend( j.r );
    }
    else
    {
        pendingFrames.remove( j.r.key );
        failedFrames.insert( j.r.key );
    }
    stopProcess( j.process, j.tmpFile );   /* also after a timeout */
    startNext();
}

/* Killed without waiting on the GUI thread (an antivirus can hold a dying
 * process): its part file goes once the process has let go of it */
void Thumbnailer::stopProcess( QProcess *p, const QString &tmpFile )
{
    p->disconnect();
    if( p->state() == QProcess::NotRunning )
    {
        QFile::remove( tmpFile );
        p->deleteLater();
        return;
    }
    connect( p, static_cast<void (QProcess::*)( int, QProcess::ExitStatus )>( &QProcess::finished ),
             p, [p, tmpFile]() {
        QFile::remove( tmpFile );
        p->deleteLater();
    } );
    p->kill();
}
