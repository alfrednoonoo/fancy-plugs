// The platform layer for Mac (docs/mac.md, step 1): the operating-system helpers that FancyBench and
// the CPU tests use, and Fancy Verb's portable convolver kernels, which must match the SSE ones bit
// for bit so that Apple silicon renders what x86 renders.

#include "Platform.h"

#if FANCY_HAS_VERB
 #include "ConvolverKernels.h"
#endif

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <random>
#include <vector>

TEST_CASE ("platform layer: CPU time and cycle counters move forward", "[portability]")
{
    const auto threadBefore = bench::platform::threadCpuSeconds();
    const auto processBefore = bench::platform::processCpuSeconds();
    const auto cyclesBefore = bench::platform::threadCycles();

    const auto rate = bench::platform::cyclesPerSecond(); // spins for 200 ms

    const auto threadSpent = bench::platform::threadCpuSeconds() - threadBefore;
    const auto processSpent = bench::platform::processCpuSeconds() - processBefore;

    INFO ("thread " << threadSpent << " s, process " << processSpent << " s, " << rate << " cycles/s");
    CHECK (bench::platform::threadCycles() > cyclesBefore);
    CHECK (rate > 1.0e6);
    CHECK (threadSpent > 0.05);
    CHECK (processSpent >= threadSpent - 0.02); // the two clocks tick at different times
}

TEST_CASE ("platform layer: audio priority can be raised and dropped", "[portability]")
{
    {
        const bench::platform::ScopedAudioPriority priority;
        bench::platform::yieldThread();
        bench::platform::sleepBriefly();
    }

    const bench::platform::ScopedTimerResolution resolution;
    const auto start = std::chrono::steady_clock::now();
    bench::platform::sleepBriefly();
    CHECK (std::chrono::steady_clock::now() - start < std::chrono::milliseconds (100));
}

#if FANCY_HAS_VERB && FANCY_VERB_SSE
TEST_CASE ("Verb's portable convolver kernels match SSE bit for bit", "[portability][verb]")
{
    std::mt19937 random (2026);
    std::uniform_real_distribution<float> uniform (-1.0f, 1.0f);
    using Complex = juce::dsp::Complex<float>;

    for (const auto count : { 4, 8, 64, 128, 1024, 4096 })
    {
        alignas (16) static float aligned[4096];
        std::vector<float> other ((size_t) count + 1);

        for (int i = 0; i < count; ++i) aligned[i] = uniform (random);
        for (auto& value : other) value = uniform (random) * 1000.0f;

        // other + 1: unaligned, as the convolver's history is.
        const auto sse = verb::kernels::dotSse (aligned, other.data() + 1, count);
        const auto portable = verb::kernels::dotPortable (aligned, other.data() + 1, count);

        INFO ("dot, count " << count);
        CHECK (std::memcmp (&sse, &portable, sizeof (float)) == 0);
    }

    for (const auto count : { 1, 2, 3, 17, 64, 65, 257, 1025, 16385 })
    {
        std::vector<Complex> a ((size_t) count), b ((size_t) count), outSse ((size_t) count);

        for (size_t i = 0; i < a.size(); ++i)
        {
            a[i] = { uniform (random), uniform (random) };
            b[i] = { uniform (random) * 30.0f, uniform (random) * 30.0f };
            outSse[i] = { uniform (random), uniform (random) };
        }

        auto outPortable = outSse;

        // Twice, so the accumulation itself is compared too.
        for (int pass = 0; pass < 2; ++pass)
        {
            verb::kernels::multiplyAccumulateSse (outSse.data(), a.data(), b.data(), count);
            verb::kernels::multiplyAccumulatePortable (outPortable.data(), a.data(), b.data(), count);
        }

        INFO ("multiply-accumulate, count " << count);
        CHECK (std::memcmp (outSse.data(), outPortable.data(), sizeof (Complex) * (size_t) count) == 0);
    }
}
#endif
