/*****************************************************************************
 * spectrum.cpp : the audio mode's spectrum strip and its audio filter
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

#include "components/spectrum.hpp"
#include "input_manager.hpp"
#include "util/theme.hpp"
#include "util/spectrum_analysis.hpp"

#include <vlc_aout.h>
#include <vlc_filter.h>

#ifdef _WIN32
# include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QMutex>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QStringList>
#include <QTimer>

using namespace RedesignTokens;

namespace {
const char tapName[] = "qt_spectrum";

struct SpectrumFrame
{
    mtime_t date;
    float bands[SpectrumWidget::BANDS];
};
/* The decoder filters ~1.2 s ahead of playback: hold >= 2.5 s of 10 ms blocks */
enum { RING = 256 };
QMutex ringLock;
SpectrumFrame ring[RING];
unsigned ringHead;
/* Visible widgets; the filter passes blocks through untouched at 0 */
std::atomic<int> viewers( 0 );
/* A pause holds back the blocks measured before it by its length: when it
 * began (0: not paused) and the ring's head then (under ringLock) */
mtime_t pausedAt;
unsigned pausedHead;

void clearRing()
{
    QMutexLocker locker( &ringLock );
    memset( ring, 0, sizeof( ring ) );
}
}

static block_t *SpectrumFilter( filter_t *p_filter, block_t *p_block )
{
    const unsigned channels = p_filter->fmt_in.audio.i_channels;
    const unsigned n = SpectrumAnalysis::windowFor( p_block->i_nb_samples );
    if( viewers.load() == 0 || n == 0 || channels == 0 )
        return p_block;

    /* Mono downmix of the block's first n frames */
    const float *in = (const float *)p_block->p_buffer;
    float mono[SpectrumAnalysis::MaxWindow], scratch[SpectrumAnalysis::MaxWindow];
    for( unsigned i = 0; i < n; i++ )
    {
        float s = 0.f;
        for( unsigned c = 0; c < channels; c++ )
            s += in[i * channels + c];
        mono[i] = s / channels;
    }

    SpectrumFrame f;
    f.date = p_block->i_pts;
    SpectrumAnalysis::bands( mono, scratch, n, p_filter->fmt_in.audio.i_rate,
                             f.bands, SpectrumWidget::BANDS );

    QMutexLocker locker( &ringLock );
    ring[ringHead++ % RING] = f;
    return p_block;
}

/* a seek or a stop (aout_FiltersFlush): the blocks measured ahead will
 * not play */
static void FlushSpectrum( filter_t * )
{
    clearRing();
}

int OpenSpectrumFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Pass-through: ask for float and give the same format back */
    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare( &p_filter->fmt_in.audio );
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = SpectrumFilter;
    p_filter->pf_flush = FlushSpectrum;

    clearRing(); /* drop the previous track */
    return VLC_SUCCESS;
}

void SpectrumWidget::setTap( intf_thread_t *p_intf, bool add )
{
    audio_output_t *aout = THEMIM->getAout();
    if( aout == NULL )
        return;
    char *psz = var_GetString( aout, "audio-filter" );
    QStringList list = qfu( psz ).split( ':', QString::SkipEmptyParts );
    free( psz );
    if( list.contains( tapName ) != add )
    {
        if( add )
            list << tapName;
        else
            list.removeAll( tapName );
        /* the audio output rebuilds its filters (FilterCallback) */
        var_SetString( aout, "audio-filter", qtu( list.join( ":" ) ) );
    }
    vlc_object_release( aout );
}

SpectrumWidget::SpectrumWidget( intf_thread_t *_p_i, QWidget *parent )
    : QWidget( parent ), p_intf( _p_i ), playing( false ), counted( false ),
      lastFrame( 0 )
{
    memset( shown, 0, sizeof( shown ) );
    setAttribute( Qt::WA_TransparentForMouseEvents );
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    setFixedHeight( Theme::instance()->px( NowPlaying::SpectrumH ) );
    /* decoration: the track's own controls carry its state */
    setAccessibleName( qtr( "Spectrum" ) );

    timer = new QTimer( this );
    timer->setInterval( NowPlaying::SpectrumFrame );
    timer->setTimerType( Qt::PreciseTimer ); /* an even cadence: coarse timers wander 5 % */
    connect( timer, &QTimer::timeout, this, &SpectrumWidget::tick );
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged, this,
             [this]( int state ) {
                 playing = ( state == PLAYING_S );
                 shiftForPause( state );
                 restart();
             } );
    playing = THEMIM->getIM()->playingStatus() == PLAYING_S;
}

SpectrumWidget::~SpectrumWidget()
{
    setCounted( false );
}

void SpectrumWidget::setCounted( bool b )
{
    if( b != counted )
        viewers += b ? 1 : -1;
    counted = b;
}

QSize SpectrumWidget::sizeHint() const
{
    return QSize( 0, height() );
}

/* The blocks measured before a pause play after it, later by its length:
 * so are their frames. Frames measured since the resume are left alone. */
void SpectrumWidget::shiftForPause( int state )
{
    const mtime_t now = mdate();
    QMutexLocker locker( &ringLock );
    if( state == PAUSE_S )
    {
        if( pausedAt == 0 )
        {
            pausedAt = now;
            pausedHead = ringHead;
        }
        return;
    }
    if( state == PLAYING_S && pausedAt != 0 )
    {
        const unsigned since = ringHead - pausedHead;
        for( unsigned k = 1; since < RING && k <= RING - since; k++ )
        {
            SpectrumFrame &f = ring[( pausedHead - k ) % RING];
            if( f.date != 0 )
                f.date += now - pausedAt;
        }
    }
    pausedAt = 0;
}

void SpectrumWidget::restart()
{
    lastFrame = mdate(); /* give the first frames time to come */
    if( isVisible() )
        timer->start( NowPlaying::SpectrumFrame );
    else
        timer->stop();
}

/* Nothing of it can be seen: minimised, or an inactive window with other
 * windows over the bars (Windows tells no one about occlusion: ask) */
bool SpectrumWidget::hidden() const
{
    const QWidget *w = window();
    if( w->isMinimized() )
        return true;
    if( w->isActiveWindow() )
        return false;
#ifdef _WIN32
    const HWND top = reinterpret_cast<HWND>( w->winId() );
    const int y = height() / 2;
    const int xs[] = { width() / 4, width() / 2, width() * 3 / 4 };
    for( int x : xs )
    {
        /* Qt scales from each screen's origin, which stays in Windows' px */
        const QPoint g = mapToGlobal( QPoint( x, y ) );
        const QPoint o = w->windowHandle()->screen()->geometry().topLeft();
        const QPoint n = o + ( g - o ) * devicePixelRatioF();
        const POINT pt = { n.x(), n.y() };
        /* its own overlays (the immersive bar) are owned by the window */
        if( GetAncestor( WindowFromPoint( pt ), GA_ROOTOWNER ) == top )
            return false;
    }
    return true;
#else
    return false;
#endif
}

void SpectrumWidget::showEvent( QShowEvent * )
{
    setCounted( true );
    restart();
}

void SpectrumWidget::hideEvent( QHideEvent * )
{
    setCounted( false );
    timer->stop();
    clock.invalidate();
    memset( shown, 0, sizeof( shown ) ); /* rise from flat when shown again */
}

void SpectrumWidget::tick()
{
    /* reduced motion (read live): static flat bars. Covered: no work. Both
     * check back at the slow rate, for the setting or the window to return */
    const bool still = Theme::instance()->duration( Theme::Spectrum ) == 0;
    const bool idle = still || hidden();
    /* and the decoder thread skips the FFT: the ring refills about a
     * second after the bars return */
    setCounted( !idle );
    if( idle )
    {
        clock.invalidate();
        timer->setInterval( NowPlaying::SpectrumMotion );
        /* stopping while covered: flat, or the bars stay up when it returns */
        if( ( still || !playing ) && std::any_of( shown, shown + BANDS, []( float h ) { return h != 0.f; } ) )
        {
            memset( shown, 0, sizeof( shown ) );
            update();
        }
        if( !playing )
            timer->stop();
        return;
    }
    float target[BANDS] = { 0.f };
    const mtime_t now = mdate();
    if( playing )
    {
        /* Newest frame that is due, ignoring stale ones (seek, pause, gaps) */
        QMutexLocker locker( &ringLock );
        const SpectrumFrame *best = NULL;
        for( unsigned i = 0; i < RING; i++ )
        {
            const SpectrumFrame &f = ring[i];
            if( f.date != 0 && f.date <= now && f.date > now - CLOCK_FREQ / 4
             && ( best == NULL || f.date > best->date ) )
                best = &f;
        }
        if( best != NULL )
        {
            memcpy( target, best->bands, sizeof( target ) );
            lastFrame = now;
        }
    }

    /* Eased approach over the real time since the last frame (capped, so a
     * stall does not jump): quick to rise, slower to fall */
    const float dt = clock.isValid() ? float( std::min<qint64>( clock.restart(), NowPlaying::SpectrumMotion ) )
                                     : float( timer->interval() );
    if( !clock.isValid() )
        clock.start();
    const float rise = 1.f - std::exp( -dt / NowPlaying::SpectrumAttack );
    const float fall = 1.f - std::exp( -dt / NowPlaying::SpectrumRelease );
    bool settled = true, moved = false;
    for( unsigned b = 0; b < BANDS; b++ )
    {
        const float d = target[b] - shown[b];
        const float was = shown[b];
        shown[b] = std::fabs( d ) < 1e-3f ? target[b] : shown[b] + d * ( d > 0.f ? rise : fall );
        settled &= shown[b] == target[b];
        moved |= shown[b] != was;
    }
    if( moved )
        update();
    if( !playing && settled )
        timer->stop(); /* paused or stopped: settled flat */
    /* playing with no frame for a second (passthrough, a filter-less
     * output, a stall): flat, so check back at the slow rate */
    const bool starved = settled && now - lastFrame > CLOCK_FREQ;
    const int interval = starved ? NowPlaying::SpectrumMotion : NowPlaying::SpectrumFrame;
    if( timer->interval() != interval )
        timer->setInterval( interval ); /* restarts the timer: only on a change */
}

void SpectrumWidget::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    const qreal padX = t->px( NowPlaying::SpectrumPadX );
    const qreal gap = t->px( NowPlaying::SpectrumGap );
    const qreal minH = t->px( NowPlaying::SpectrumMinBar );
    const qreal radius = t->px( NowPlaying::SpectrumBarRadius );
    /* as many bars as fit at about SpectrumBarW, within the token range */
    const int bars = qBound<int>( NowPlaying::SpectrumMinBars,
                                  int( ( width() - 2 * padX + gap ) / ( t->px( NowPlaying::SpectrumBarW ) + gap ) ),
                                  BANDS );
    const qreal w = ( width() - 2 * padX - gap * ( bars - 1 ) ) / bars;
    if( w <= 0 )
        return;
    QColor even = t->color( Theme::Accent ), odd = even;
    even.setAlpha( NowPlaying::SpectrumAlphaEven );
    odd.setAlpha( NowPlaying::SpectrumAlphaOdd );
    for( int b = 0; b < bars; b++ )
    {
        /* bands [b*BANDS/bars, (b+1)*BANDS/bars): never empty, all covered */
        const float level = *std::max_element( shown + b * BANDS / bars,
                                               shown + ( b + 1 ) * BANDS / bars );
        const qreal h = minH + level * ( height() - minH );
        /* rounded top only: the radius below is cut off by the widget */
        p.setBrush( b % 2 ? odd : even );
        p.drawRoundedRect( QRectF( padX + b * ( w + gap ), height() - h, w, h + radius ),
                           radius, radius );
    }
}
