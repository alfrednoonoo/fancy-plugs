#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#if ! (JUCE_MAC || JUCE_IOS) && (defined (_M_X64) || defined (__SSE2__))
 #define FANCY_COMPLEX_FFT_SSE 1
 #include <xmmintrin.h>
#else
 #define FANCY_COMPLEX_FFT_SSE 0
#endif

namespace fancy::dsp
{

// A complex FFT, as juce::dsp::FFT::perform(), for Fancy EQ's spectral bands and Verb's Freeze
// (docs/eq.md and docs/verb.md, "Ratios to Comp"; the user chose to optimise on 26 November 2026).
//
// On the Mac, JUCE's engine is vDSP, which is fast: this is juce::dsp::FFT, so the output is what it
// was to the bit. Elsewhere JUCE falls back to its own FFT, which cost Windows most of those
// features' CPU, so on x64 this is a Stockham radix-4 FFT (a radix-2 stage last for odd orders) in
// SSE, real and imaginary parts held apart, vectorised over the butterflies: no bit reversal.
// The inverse is the forward transform with real and imaginary parts swapped on the way in and out.
//
// Scaling: the forward transform is unscaled; the inverse is whatever the engine gives (JUCE's
// scales by 1 / size, this one doesn't), so callers measure it in prepare(), as StftFrames and
// SpectralFreeze do. The own engine also works in place. prepare() allocates; perform() doesn't.
class ComplexFft
{
public:
    static constexpr bool ownEngine = FANCY_COMPLEX_FFT_SSE != 0;
    static constexpr int minOwnOrder = 6; // the first stage takes four butterflies at a time

    void prepare (int newOrder)
    {
        order = newOrder;
        size = 1 << order;

        if (! ownEngine || order < minOwnOrder)
        {
            juceFft = std::make_unique<juce::dsp::FFT> (order);
            return;
        }

        juceFft.reset();
        stages.clear();
        twiddles.clear();

        for (auto length = size, stride = 1; length >= 2; stride *= 4, length /= 4)
        {
            if (length == 2)
            {
                stages.push_back ({ 2, stride, 0 });
                break;
            }

            const auto m = length / 4;
            stages.push_back ({ length, stride, twiddles.size() });

            // w1, w2 and w3 for each p, real parts then imaginary: exp (-2 pi i k p / length).
            for (int power = 1; power <= 3; ++power)
            {
                for (int part = 0; part < 2; ++part)
                {
                    for (int p = 0; p < m; ++p)
                    {
                        const auto angle = -2.0 * std::numbers::pi * (double) (power * p) / (double) length;
                        twiddles.push_back ((float) (part == 0 ? std::cos (angle) : std::sin (angle)));
                    }
                }
            }
        }

        bufferA.assign ((size_t) (2 * size), 0.0f);
        bufferB.assign ((size_t) (2 * size), 0.0f);
    }

    int getSize() const noexcept { return size; }

    void perform (const juce::dsp::Complex<float>* input, juce::dsp::Complex<float>* output, bool inverse) noexcept
    {
        if (juceFft != nullptr)
        {
            juceFft->perform (input, output, inverse);
            return;
        }

       #if FANCY_COMPLEX_FFT_SSE
        auto* xr = bufferA.data();
        auto* xi = bufferA.data() + size;
        auto* yr = bufferB.data();
        auto* yi = bufferB.data() + size;

        // The inverse: swap real and imaginary on the way in and out.
        deinterleave (reinterpret_cast<const float*> (input), inverse ? xi : xr, inverse ? xr : xi);

        for (const auto& stage : stages)
        {
            if (stage.length == 2)
                radix2 (xr, xi, yr, yi, stage.stride);
            else if (stage.stride == 1)
                radix4First (xr, xi, yr, yi, stage.length / 4, twiddles.data() + stage.twiddle);
            else
                radix4 (xr, xi, yr, yi, stage.length / 4, stage.stride, twiddles.data() + stage.twiddle);

            std::swap (xr, yr);
            std::swap (xi, yi);
        }

        interleave (reinterpret_cast<float*> (output), inverse ? xi : xr, inverse ? xr : xi);
       #endif
    }

private:
    struct Stage
    {
        int length, stride;
        size_t twiddle;
    };

   #if FANCY_COMPLEX_FFT_SSE
    void deinterleave (const float* in, float* re, float* im) const noexcept
    {
        for (int i = 0; i < size; i += 4)
        {
            const auto lo = _mm_loadu_ps (in + 2 * i);
            const auto hi = _mm_loadu_ps (in + 2 * i + 4);
            _mm_storeu_ps (re + i, _mm_shuffle_ps (lo, hi, _MM_SHUFFLE (2, 0, 2, 0)));
            _mm_storeu_ps (im + i, _mm_shuffle_ps (lo, hi, _MM_SHUFFLE (3, 1, 3, 1)));
        }
    }

    void interleave (float* out, const float* re, const float* im) const noexcept
    {
        for (int i = 0; i < size; i += 4)
        {
            const auto r = _mm_loadu_ps (re + i);
            const auto m = _mm_loadu_ps (im + i);
            _mm_storeu_ps (out + 2 * i, _mm_unpacklo_ps (r, m));
            _mm_storeu_ps (out + 2 * i + 4, _mm_unpackhi_ps (r, m));
        }
    }

    // One radix-4 butterfly on four lanes: a, b, c, d in; y0 to y3 out, y1 to y3 twiddled.
    struct Butterfly
    {
        __m128 y0r, y0i, y1r, y1i, y2r, y2i, y3r, y3i;
    };

    static Butterfly butterfly (__m128 ar, __m128 ai, __m128 br, __m128 bi, __m128 cr, __m128 ci, __m128 dr, __m128 di,
                                __m128 w1r, __m128 w1i, __m128 w2r, __m128 w2i, __m128 w3r, __m128 w3i) noexcept
    {
        const auto apcr = _mm_add_ps (ar, cr), apci = _mm_add_ps (ai, ci);
        const auto amcr = _mm_sub_ps (ar, cr), amci = _mm_sub_ps (ai, ci);
        const auto bpdr = _mm_add_ps (br, dr), bpdi = _mm_add_ps (bi, di);
        const auto bmdr = _mm_sub_ps (br, dr), bmdi = _mm_sub_ps (bi, di);

        // (a - c) - i (b - d), (a + c) - (b + d), (a - c) + i (b - d)
        const auto t1r = _mm_add_ps (amcr, bmdi), t1i = _mm_sub_ps (amci, bmdr);
        const auto t2r = _mm_sub_ps (apcr, bpdr), t2i = _mm_sub_ps (apci, bpdi);
        const auto t3r = _mm_sub_ps (amcr, bmdi), t3i = _mm_add_ps (amci, bmdr);

        Butterfly out;
        out.y0r = _mm_add_ps (apcr, bpdr);
        out.y0i = _mm_add_ps (apci, bpdi);
        out.y1r = _mm_sub_ps (_mm_mul_ps (w1r, t1r), _mm_mul_ps (w1i, t1i));
        out.y1i = _mm_add_ps (_mm_mul_ps (w1r, t1i), _mm_mul_ps (w1i, t1r));
        out.y2r = _mm_sub_ps (_mm_mul_ps (w2r, t2r), _mm_mul_ps (w2i, t2i));
        out.y2i = _mm_add_ps (_mm_mul_ps (w2r, t2i), _mm_mul_ps (w2i, t2r));
        out.y3r = _mm_sub_ps (_mm_mul_ps (w3r, t3r), _mm_mul_ps (w3i, t3i));
        out.y3i = _mm_add_ps (_mm_mul_ps (w3r, t3i), _mm_mul_ps (w3i, t3r));
        return out;
    }

    // Stride 1: four values of p at a time, each needing its own twiddles; the outputs for one p
    // are adjacent, so the four butterflies' outputs are transposed before they're stored.
    static void radix4First (const float* xr, const float* xi, float* yr, float* yi, int m, const float* t) noexcept
    {
        for (int p = 0; p < m; p += 4)
        {
            auto o = butterfly (_mm_loadu_ps (xr + p), _mm_loadu_ps (xi + p),
                                _mm_loadu_ps (xr + p + m), _mm_loadu_ps (xi + p + m),
                                _mm_loadu_ps (xr + p + 2 * m), _mm_loadu_ps (xi + p + 2 * m),
                                _mm_loadu_ps (xr + p + 3 * m), _mm_loadu_ps (xi + p + 3 * m),
                                _mm_loadu_ps (t + p), _mm_loadu_ps (t + m + p),
                                _mm_loadu_ps (t + 2 * m + p), _mm_loadu_ps (t + 3 * m + p),
                                _mm_loadu_ps (t + 4 * m + p), _mm_loadu_ps (t + 5 * m + p));

            _MM_TRANSPOSE4_PS (o.y0r, o.y1r, o.y2r, o.y3r);
            _MM_TRANSPOSE4_PS (o.y0i, o.y1i, o.y2i, o.y3i);

            auto* outR = yr + 4 * p;
            auto* outI = yi + 4 * p;
            _mm_storeu_ps (outR, o.y0r);
            _mm_storeu_ps (outR + 4, o.y1r);
            _mm_storeu_ps (outR + 8, o.y2r);
            _mm_storeu_ps (outR + 12, o.y3r);
            _mm_storeu_ps (outI, o.y0i);
            _mm_storeu_ps (outI + 4, o.y1i);
            _mm_storeu_ps (outI + 8, o.y2i);
            _mm_storeu_ps (outI + 12, o.y3i);
        }
    }

    // Stride 4 and up: one p at a time, its twiddles broadcast, four values of q at a time.
    static void radix4 (const float* xr, const float* xi, float* yr, float* yi, int m, int s, const float* t) noexcept
    {
        for (int p = 0; p < m; ++p)
        {
            const auto w1r = _mm_set1_ps (t[p]), w1i = _mm_set1_ps (t[m + p]);
            const auto w2r = _mm_set1_ps (t[2 * m + p]), w2i = _mm_set1_ps (t[3 * m + p]);
            const auto w3r = _mm_set1_ps (t[4 * m + p]), w3i = _mm_set1_ps (t[5 * m + p]);

            const auto in0 = s * p, in1 = s * (p + m), in2 = s * (p + 2 * m), in3 = s * (p + 3 * m);
            const auto out0 = s * 4 * p;

            for (int q = 0; q < s; q += 4)
            {
                const auto o = butterfly (_mm_loadu_ps (xr + in0 + q), _mm_loadu_ps (xi + in0 + q),
                                          _mm_loadu_ps (xr + in1 + q), _mm_loadu_ps (xi + in1 + q),
                                          _mm_loadu_ps (xr + in2 + q), _mm_loadu_ps (xi + in2 + q),
                                          _mm_loadu_ps (xr + in3 + q), _mm_loadu_ps (xi + in3 + q),
                                          w1r, w1i, w2r, w2i, w3r, w3i);

                _mm_storeu_ps (yr + out0 + q, o.y0r);
                _mm_storeu_ps (yi + out0 + q, o.y0i);
                _mm_storeu_ps (yr + out0 + s + q, o.y1r);
                _mm_storeu_ps (yi + out0 + s + q, o.y1i);
                _mm_storeu_ps (yr + out0 + 2 * s + q, o.y2r);
                _mm_storeu_ps (yi + out0 + 2 * s + q, o.y2i);
                _mm_storeu_ps (yr + out0 + 3 * s + q, o.y3r);
                _mm_storeu_ps (yi + out0 + 3 * s + q, o.y3i);
            }
        }
    }

    // The last stage of an odd order: length 2, stride size / 2, no twiddles.
    static void radix2 (const float* xr, const float* xi, float* yr, float* yi, int s) noexcept
    {
        for (int q = 0; q < s; q += 4)
        {
            const auto ar = _mm_loadu_ps (xr + q), ai = _mm_loadu_ps (xi + q);
            const auto br = _mm_loadu_ps (xr + q + s), bi = _mm_loadu_ps (xi + q + s);
            _mm_storeu_ps (yr + q, _mm_add_ps (ar, br));
            _mm_storeu_ps (yi + q, _mm_add_ps (ai, bi));
            _mm_storeu_ps (yr + q + s, _mm_sub_ps (ar, br));
            _mm_storeu_ps (yi + q + s, _mm_sub_ps (ai, bi));
        }
    }
   #endif

    int order = 0, size = 1;
    std::unique_ptr<juce::dsp::FFT> juceFft;
    std::vector<Stage> stages;
    std::vector<float> twiddles, bufferA, bufferB;
};

} // namespace fancy::dsp
