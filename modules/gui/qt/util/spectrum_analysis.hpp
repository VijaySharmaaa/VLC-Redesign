/*****************************************************************************
 * spectrum_analysis.hpp : band levels of a block of audio (no Qt, no VLC)
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

#ifndef VLC_QT_SPECTRUM_ANALYSIS_HPP_
#define VLC_QT_SPECTRUM_ANALYSIS_HPP_

#include <algorithm>
#include <cmath>

namespace SpectrumAnalysis
{
    /* Bars span 50 Hz .. 16 kHz on a log scale and -60 .. 0 dBFS */
    static const float LowHz = 50.f, HighHz = 16000.f, FloorDb = -60.f;
    enum { MinWindow = 64, MaxWindow = 2048 };

    /* Largest power of two <= frames, within [MinWindow, MaxWindow]; 0 if too short */
    inline unsigned windowFor( unsigned frames )
    {
        if( frames < MinWindow )
            return 0;
        unsigned n = MinWindow;
        while( n * 2 <= frames && n * 2 <= MaxWindow )
            n *= 2;
        return n;
    }

    /* In-place radix-2 complex FFT, n a power of two */
    inline void fft( float *re, float *im, unsigned n )
    {
        for( unsigned i = 1, j = 0; i < n; i++ )
        {
            unsigned bit = n >> 1;
            for( ; j & bit; bit >>= 1 )
                j ^= bit;
            j ^= bit;
            if( i < j )
            {
                std::swap( re[i], re[j] );
                std::swap( im[i], im[j] );
            }
        }
        for( unsigned len = 2; len <= n; len <<= 1 )
        {
            const float angle = -2.f * float( M_PI ) / len;
            const float wr = cosf( angle ), wi = sinf( angle );
            for( unsigned i = 0; i < n; i += len )
            {
                float cr = 1.f, ci = 0.f;
                for( unsigned k = 0; k < len / 2; k++ )
                {
                    const unsigned a = i + k, b = a + len / 2;
                    const float tr = re[b] * cr - im[b] * ci;
                    const float ti = re[b] * ci + im[b] * cr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                    const float nr = cr * wr - ci * wi;
                    ci = cr * wi + ci * wr;
                    cr = nr;
                }
            }
        }
    }

    /* mono: n samples (n from windowFor), overwritten. im: n floats of
     * scratch. bands: count levels in 0..1. Each bar takes the loudest bin
     * between its neighbours' log midpoints, or the bin under its centre
     * when the window is too short to resolve it. */
    inline void bands( float *mono, float *im, unsigned n, float rate,
                       float *out, unsigned count )
    {
        for( unsigned i = 0; i < n; i++ )
        {
            mono[i] *= 0.5f - 0.5f * cosf( 2.f * float( M_PI ) * i / ( n - 1 ) ); /* Hann */
            im[i] = 0.f;
        }
        fft( mono, im, n );

        const float ratio = HighHz / LowHz, binHz = rate / n;
        const float step = count > 1 ? 1.f / ( count - 1 ) : 0.f; /* one bar: LowHz */
        for( unsigned b = 0; b < count; b++ )
        {
            const float centre = LowHz * powf( ratio, b * step );
            out[b] = 0.f;
            if( centre >= rate / 2.f )
                continue;
            int lo = (int)ceilf( LowHz * powf( ratio, ( b - 0.5f ) * step ) / binHz );
            int hi = (int)floorf( LowHz * powf( ratio, ( b + 0.5f ) * step ) / binHz );
            hi = std::min( hi, (int)n / 2 - 1 );
            if( lo > hi )
                lo = hi = std::min( (int)lroundf( centre / binHz ), (int)n / 2 - 1 );
            float peak = 0.f;
            for( int k = std::max( lo, 1 ); k <= hi; k++ )
                peak = std::max( peak, mono[k] * mono[k] + im[k] * im[k] );
            /* one-sided spectrum x2, Hann coherent gain 0.5: a full-scale sine reads 1 */
            const float amp = 4.f * sqrtf( peak ) / n;
            const float db = 20.f * log10f( amp + 1e-9f );
            out[b] = std::min( 1.f, std::max( 0.f, ( db - FloorDb ) / -FloorDb ) );
        }
    }
}

#endif
