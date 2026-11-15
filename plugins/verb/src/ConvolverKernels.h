#pragma once

// The convolver's two inner loops (docs/mac.md, step 1). On x86 (Windows, Intel Macs) they use
// SSE, as they always have. Elsewhere (Apple silicon) they use the portable versions, which do
// the same float operations in the same order, so they give bit-identical results as long as the
// compiler doesn't fuse multiplies and adds (clang builds use -ffp-contract=off). A test runs
// both on random data and requires identical output.

#include <juce_dsp/juce_dsp.h>

#if defined (__x86_64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86)
 #define FANCY_VERB_SSE 1
 #include <immintrin.h>
#else
 #define FANCY_VERB_SSE 0
#endif

namespace verb::kernels
{

// Sum of aligned[i] * other[i]; count is a multiple of 4. Four running sums, one per SSE lane,
// added in lane order at the end.
inline float dotPortable (const float* aligned, const float* other, int count) noexcept
{
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;

    for (int i = 0; i < count; i += 4)
    {
        s0 = s0 + aligned[i] * other[i];
        s1 = s1 + aligned[i + 1] * other[i + 1];
        s2 = s2 + aligned[i + 2] * other[i + 2];
        s3 = s3 + aligned[i + 3] * other[i + 3];
    }

    return s0 + s1 + s2 + s3;
}

// out += a * b for interleaved single-precision complex numbers.
inline void multiplyAccumulatePortable (juce::dsp::Complex<float>* out, const juce::dsp::Complex<float>* a,
                                        const juce::dsp::Complex<float>* b, int count) noexcept
{
    auto* o = reinterpret_cast<float*> (out);
    const auto* x = reinterpret_cast<const float*> (a);
    const auto* y = reinterpret_cast<const float*> (b);

    for (int i = 0; i < count; ++i)
    {
        const auto xr = x[i * 2], xi = x[i * 2 + 1];
        const auto yr = y[i * 2], yi = y[i * 2 + 1];
        o[i * 2] = o[i * 2] + (xr * yr - xi * yi);
        o[i * 2 + 1] = o[i * 2 + 1] + (xr * yi + xi * yr);
    }
}

#if FANCY_VERB_SSE
inline float dotSse (const float* aligned, const float* other, int count) noexcept
{
    auto sum = _mm_setzero_ps();

    for (int i = 0; i < count; i += 4)
        sum = _mm_add_ps (sum, _mm_mul_ps (_mm_load_ps (aligned + i), _mm_loadu_ps (other + i)));

    alignas (16) float parts[4];
    _mm_store_ps (parts, sum);
    return parts[0] + parts[1] + parts[2] + parts[3];
}

inline void multiplyAccumulateSse (juce::dsp::Complex<float>* out, const juce::dsp::Complex<float>* a,
                                   const juce::dsp::Complex<float>* b, int count) noexcept
{
    auto* o = reinterpret_cast<float*> (out);
    const auto* x = reinterpret_cast<const float*> (a);
    const auto* y = reinterpret_cast<const float*> (b);

    int i = 0;

    for (; i + 2 <= count; i += 2)
    {
        const auto va = _mm_loadu_ps (x + i * 2);
        const auto vb = _mm_loadu_ps (y + i * 2);
        const auto real = _mm_moveldup_ps (va);
        const auto imaginary = _mm_movehdup_ps (va);
        const auto swapped = _mm_shuffle_ps (vb, vb, _MM_SHUFFLE (2, 3, 0, 1));
        const auto product = _mm_addsub_ps (_mm_mul_ps (real, vb), _mm_mul_ps (imaginary, swapped));
        _mm_storeu_ps (o + i * 2, _mm_add_ps (_mm_loadu_ps (o + i * 2), product));
    }

    for (; i < count; ++i)
        out[i] += a[i] * b[i];
}
#endif

inline float dot (const float* aligned, const float* other, int count) noexcept
{
   #if FANCY_VERB_SSE
    return dotSse (aligned, other, count);
   #else
    return dotPortable (aligned, other, count);
   #endif
}

inline void multiplyAccumulate (juce::dsp::Complex<float>* out, const juce::dsp::Complex<float>* a,
                                const juce::dsp::Complex<float>* b, int count) noexcept
{
   #if FANCY_VERB_SSE
    multiplyAccumulateSse (out, a, b, count);
   #else
    multiplyAccumulatePortable (out, a, b, count);
   #endif
}

} // namespace verb::kernels
