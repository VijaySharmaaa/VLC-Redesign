/*****************************************************************************
 * spectrum_analysis_test.cpp : self-check of spectrum_analysis.hpp
 *****************************************************************************
 * Not part of the plugin: the header needs neither Qt nor VLC. "make check"
 * builds and runs it (modules/gui/qt/Makefile.am); on its own:
 *   g++ -std=gnu++11 -o /tmp/spectrum_test modules/gui/qt/util/spectrum_analysis_test.cpp && /tmp/spectrum_test
 *
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

#undef NDEBUG
#include "spectrum_analysis.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace SpectrumAnalysis;

/* the levels of a full-scale sine of hz in count bars, from frames samples */
static std::vector<float> levels( float hz, unsigned count, float rate = 48000.f,
                                  unsigned frames = 4096 )
{
    const unsigned n = windowFor( frames );
    std::vector<float> mono( n ), im( n ), out( count );
    for( unsigned i = 0; i < n; i++ )
        mono[i] = sinf( 2.f * float( M_PI ) * hz * i / rate );
    bands( mono.data(), im.data(), n, rate, out.data(), count );
    return out;
}

/* every level is a number in 0..1 */
static void assertRange( const std::vector<float> &v )
{
    for( size_t b = 0; b < v.size(); b++ )
        assert( v[b] >= 0.f && v[b] <= 1.f );
}

/* the bar that reads loudest, and that its centre is near hz */
static void assertPeakNear( const std::vector<float> &v, float hz, float minLevel )
{
    unsigned loud = 0;
    for( unsigned b = 1; b < v.size(); b++ )
        if( v[b] > v[loud] )
            loud = b;
    const float centre = LowHz * powf( HighHz / LowHz, loud / float( v.size() - 1 ) );
    assert( centre > hz * 0.8f && centre < hz * 1.25f );
    assert( v[loud] > minLevel );
}

int main()
{
    assert( windowFor( 63 ) == 0 );
    assert( windowFor( 100 ) == 64 );
    assert( windowFor( 100000 ) == MaxWindow );
    assert( windowFor( 64 ) == 64 && windowFor( 127 ) == 64 && windowFor( 128 ) == 128 );

    /* a full-scale 1 kHz sine reads about 1 in its bar, far less far away */
    const std::vector<float> v = levels( 1000.f, 48 );
    unsigned loud = 0;
    for( unsigned b = 1; b < v.size(); b++ )
        if( v[b] > v[loud] )
            loud = b;
    const float centre = LowHz * powf( HighHz / LowHz, loud / 47.f );
    assert( centre > 800.f && centre < 1250.f );
    assert( v[loud] > 0.95f && v[loud] <= 1.f );
    assert( v[0] < 0.5f && v[47] < 0.5f );

    /* one bar does not divide by zero; no bar writes nothing */
    const std::vector<float> one = levels( 50.f, 1 );
    assert( one[0] >= 0.f && one[0] <= 1.f );
    levels( 1000.f, 0 );

    /* 22.05 and 8 kHz: the bars above Nyquist stay flat, the rest still read */
    std::vector<float> low = levels( 1000.f, 48, 22050.f );
    assertRange( low );
    assertPeakNear( low, 1000.f, 0.9f );
    low = levels( 1000.f, 48, 8000.f );
    assertRange( low );
    assertPeakNear( low, 1000.f, 0.9f );
    assert( low[47] == 0.f );  /* 16 kHz, above 4 kHz */

    /* 192 kHz: a 10 kHz tone in a 2048 window */
    const std::vector<float> high = levels( 10000.f, 48, 192000.f );
    assertRange( high );
    assertPeakNear( high, 10000.f, 0.9f );

    /* the shortest window: 64 samples at 48 kHz, 750 Hz bins; low bars
     * fall back to a bin, a 6 kHz tone still lands near its bar */
    const std::vector<float> tiny = levels( 6000.f, 48, 48000.f, 64 );
    assertRange( tiny );
    assertPeakNear( tiny, 6000.f, 0.5f );

    /* broken input (a NaN or an infinity in the block) never gives a
     * level outside 0..1, which the widget would draw as is */
    const unsigned n = windowFor( 1024 );
    const float bad[] = { NAN, INFINITY, -INFINITY };
    for( unsigned k = 0; k < sizeof( bad ) / sizeof( bad[0] ); k++ )
    {
        std::vector<float> mono( n, 0.25f ), im( n ), out( 48 );
        mono[n / 2] = bad[k];
        bands( mono.data(), im.data(), n, 48000.f, out.data(), 48 );
        assertRange( out );
    }

    puts( "spectrum_analysis: ok" );
    return 0;
}
