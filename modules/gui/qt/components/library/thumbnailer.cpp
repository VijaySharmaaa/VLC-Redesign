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
#include "components/library/library_widgets.hpp"   /* LibraryData::workerPool */
#include "recents.hpp"
#include "util/theme.hpp"

#include <vlc_configuration.h>          /* config_GetUserDir */
#include <vlc_interface.h>              /* EXTENSIONS_VIDEO */
#include <vlc_url.h>

#include <climits>                       /* INT_MAX */

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
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
    return videoFilters().contains( QLatin1String( "*." ) + QFileInfo( path ).suffix().toLower() );
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
    /* a replaced or edited file gets a new frame */
    const QByteArray id = ( keyOf( path ) + QLatin1Char( '|' ) + QString::number( info.size() )
                          + QLatin1Char( '|' )
                          + QString::number( info.lastModified().toMSecsSinceEpoch() ) ).toUtf8();
    /* shortcut: never pruned (a PNG of some 50 KB per video); remove the
     * frames of gone files if the folder grows past what users accept */
    const QString file = cacheDir + QLatin1Char( '/' )
                       + QString::fromLatin1( QCryptographicHash::hash( id, QCryptographicHash::Sha1 ).toHex() )
                       + QLatin1String( ".png" );
    emit probed( path, file, QFileInfo( file ).size() > 0 );
}

/**********************************************************************
 * Thumbnailer
 **********************************************************************/
Thumbnailer::Thumbnailer( intf_thread_t *_p_intf, QObject *parent )
    : QObject( parent ), p_intf( _p_intf ),
      /* each one decodes on all cores: half of them keeps VLC responsive */
      maxJobs( qBound( 1, QThread::idealThreadCount() / 2, int( Library::ThumbWorkersMax ) ) )
{
    /* portable mode: the dir functions already point next to vlc.exe */
    char *psz = config_GetUserDir( VLC_CACHE_DIR );
    if( psz )
        cacheDir = QDir::fromNativeSeparators( qfu( psz ) ) + QLatin1String( "/thumbnails" );
    free( psz );

    poll = new QTimer( this );
    poll->setInterval( Library::ThumbPollMs );
    connect( poll, &QTimer::timeout, this, &Thumbnailer::check );

    connect( RecentsMRL::getInstance( p_intf ), &RecentsMRL::changed,
             this, &Thumbnailer::loadRecents );
    loadRecents();
}

Thumbnailer::~Thumbnailer()
{
    queue.clear();
    foreach( const Job &j, jobs )
    {
        stopProcess( j.process );
        QFile::remove( j.tmpFile );
    }
}

QString Thumbnailer::frame( const QString &path, int64_t lengthMs )
{
    const QString key = keyOf( path );
    if( cacheDir.isEmpty() || !isVideo( path ) || failedFrames.contains( key ) )
        return QString();
    QHash<QString, QString>::const_iterator it = frames.constFind( key );
    if( it != frames.constEnd() )
        return it.value();
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
        connect( job, &ThumbJob::probed, this, &Thumbnailer::probed, Qt::QueuedConnection );
        LibraryData::workerPool()->start( job );
    }
    return QString();
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
        LibraryData::workerPool()->start( job );
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
    foreach( const Request &r, queue )
        pendingFrames.remove( r.key );
    queue.clear();
    foreach( const Job &j, jobs )
    {
        pendingFrames.remove( j.r.key );
        stopProcess( j.process );
        QFile::remove( j.tmpFile );
    }
    jobs.clear();
    poll->stop();
    /* listed again when shown: files come and go */
    dirs.clear();
    covers.clear();
    watchedCounts.clear();
}

void Thumbnailer::probed( const QString &path, const QString &file, bool cached )
{
    const QString key = keyOf( path );
    if( file.isEmpty() )
    {
        pendingFrames.remove( key );
        failedFrames.insert( key );
        return;
    }
    if( cached )
    {
        pendingFrames.remove( key );
        frames.insert( key, file );
        emit changed();
        return;
    }
    /* the middle of a film opens on a scene, its start often on black */
    const int64_t lengthMs = lengths.value( key );
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
        char *psz = vlc_uri2path( qtu( mrl ) );
        if( !psz )
            continue;
        const int64_t timeMs = recents->time( mrl ), lengthMs = recents->length( mrl );
        /* shortcut: RecentsMRL keeps no "finished" state, only no resume
         * point (played past 95 %, or under 5 %, or shorter than a minute),
         * and only its last 30 items; store a watched flag if that misleads */
        played.insert( keyOf( qfu( psz ) ), timeMs > 0 && lengthMs > timeMs
                                            ? qreal( timeMs ) / lengthMs : 1 );
        free( psz );
    }
    emit changed();
}

void Thumbnailer::startNext()
{
    if( queue.isEmpty() )
        return;
    if( QFileInfo( QCoreApplication::applicationFilePath() ).baseName()
            .compare( QLatin1String( "vlc" ), Qt::CaseInsensitive ) )
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
    const QString prefix = QFileInfo( r.file ).completeBaseName()
                         + QString( ".%1.part" ).arg( QCoreApplication::applicationPid() );
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
         /* the scene filter takes system memory pictures */
         << "--avcodec-hw=none"
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
    stopProcess( j.process );   /* also after a timeout: the file is whole only once renamed */
    if( ok && QFileInfo( j.tmpFile ).size() > 0
     && ( QFile::rename( j.tmpFile, j.r.file ) || QFileInfo( j.r.file ).size() > 0 ) )
    {
        pendingFrames.remove( j.r.key );
        frames.insert( j.r.key, j.r.file );
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
    QFile::remove( j.tmpFile );
    startNext();
}

void Thumbnailer::stopProcess( QProcess *p )
{
    p->disconnect();
    if( p->state() != QProcess::NotRunning )
    {
        p->kill();
        /* the picture file is let go of with the process */
        p->waitForFinished( Library::ThumbKillWaitMs );
    }
    p->deleteLater();
}
