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

#include <atomic>

#include <QMutex>
#include <QPainter>
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

int OpenSpectrumFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Pass-through: ask for float and give the same format back */
    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare( &p_filter->fmt_in.audio );
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = SpectrumFilter;

    QMutexLocker locker( &ringLock );
    memset( ring, 0, sizeof( ring ) ); /* drop the previous track */
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
    : QWidget( parent ), p_intf( _p_i ), playing( false ), counted( false )
{
    memset( shown, 0, sizeof( shown ) );
    setAttribute( Qt::WA_TransparentForMouseEvents );
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    setFixedHeight( Theme::instance()->px( NowPlaying::SpectrumH ) );
    /* decoration: the track's own controls carry its state */
    setAccessibleName( qtr( "Spectrum" ) );

    timer = new QTimer( this );
    timer->setInterval( NowPlaying::SpectrumFrame );
    connect( timer, &QTimer::timeout, this, &SpectrumWidget::tick );
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged, this,
             [this]( int state ) {
                 playing = ( state == PLAYING_S );
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

void SpectrumWidget::restart()
{
    /* reduced motion: static flat bars, nothing to run */
    if( isVisible() && Theme::instance()->duration( Theme::Spectrum ) > 0 )
        timer->start();
    else
        timer->stop();
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
    memset( shown, 0, sizeof( shown ) ); /* rise from flat when shown again */
}

void SpectrumWidget::tick()
{
    float target[BANDS] = { 0.f };
    if( playing )
    {
        /* Newest frame that is due, ignoring stale ones (seek, pause, gaps) */
        const mtime_t now = mdate();
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
            memcpy( target, best->bands, sizeof( target ) );
    }

    /* Linear approach: full height in SpectrumMotion */
    const float step = float( timer->interval() ) / NowPlaying::SpectrumMotion;
    bool settled = true;
    for( unsigned b = 0; b < BANDS; b++ )
    {
        const float d = target[b] - shown[b];
        shown[b] += std::max( -step, std::min( step, d ) );
        settled &= shown[b] == target[b];
    }
    update();
    if( !playing && settled )
        timer->stop(); /* paused or stopped: settled flat */
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
    const qreal w = ( width() - 2 * padX - gap * ( BANDS - 1 ) ) / BANDS;
    if( w <= 0 )
        return;
    QColor even = t->color( Theme::Accent ), odd = even;
    even.setAlpha( NowPlaying::SpectrumAlphaEven );
    odd.setAlpha( NowPlaying::SpectrumAlphaOdd );
    for( unsigned b = 0; b < BANDS; b++ )
    {
        const qreal h = minH + shown[b] * ( height() - minH );
        /* rounded top only: the radius below is cut off by the widget */
        p.setBrush( b % 2 ? odd : even );
        p.drawRoundedRect( QRectF( padX + b * ( w + gap ), height() - h, w, h + radius ),
                           radius, radius );
    }
}
