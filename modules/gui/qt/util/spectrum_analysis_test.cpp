/*****************************************************************************
 * spectrum_analysis_test.cpp : self-check of spectrum_analysis.hpp
 *****************************************************************************
 * Not part of the plugin: the header needs neither Qt nor VLC. Run it with
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

/* the levels of a full-scale sine of hz, rate 48 kHz, in count bars */
static std::vector<float> levels( float hz, unsigned count )
{
    const float rate = 48000.f;
    const unsigned n = windowFor( 4096 );
    std::vector<float> mono( n ), im( n ), out( count );
    for( unsigned i = 0; i < n; i++ )
        mono[i] = sinf( 2.f * float( M_PI ) * hz * i / rate );
    bands( mono.data(), im.data(), n, rate, out.data(), count );
    return out;
}

int main()
{
    assert( windowFor( 63 ) == 0 );
    assert( windowFor( 100 ) == 64 );
    assert( windowFor( 100000 ) == MaxWindow );

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

    puts( "spectrum_analysis: ok" );
    return 0;
}
