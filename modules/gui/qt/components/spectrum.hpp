/*****************************************************************************
 * spectrum.hpp : the audio mode's spectrum strip and its audio filter
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

#ifndef VLC_QT_SPECTRUM_HPP_
#define VLC_QT_SPECTRUM_HPP_

#include "qt.hpp"
#include "util/theme_tokens.hpp"

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

/* The "qt_spectrum" audio filter (a pass-through submodule of the Qt
 * plugin, qt.cpp) measures each block in the decoder thread, stamped with
 * the block's playback date; the widget shows the frame that is due now.
 * The filter is put on the playlist's audio output only, never on the
 * playlist or the configuration, and it skips the work while no
 * SpectrumWidget is visible. */
int OpenSpectrumFilter( vlc_object_t * );

class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    /* measured per block; the bars shown each take the loudest of a run of them */
    enum { BANDS = RedesignTokens::NowPlaying::SpectrumMaxBars };
    SpectrumWidget( intf_thread_t *, QWidget *parent );
    virtual ~SpectrumWidget();
    QSize sizeHint() const Q_DECL_OVERRIDE;

    /* Adds or removes the filter on the playlist's audio output; adding is
     * a no-op once it is there, so it is safe to call per new input (a
     * new audio output starts without it). */
    static void setTap( intf_thread_t *, bool );

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;

private:
    void tick();
    void restart();
    void shiftForPause( int state );
    bool hidden() const;
    void setCounted( bool ); /* in the filter's count of visible widgets */

    intf_thread_t *p_intf;
    QTimer *timer;
    QElapsedTimer clock; /* real time between frames: timer ticks jitter */
    bool playing;
    bool counted;
    float shown[BANDS];
};

#endif
