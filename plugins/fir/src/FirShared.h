#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace fir
{

enum Mode { eq, gate, compressor, subtract, convolve };

// Subtract's noise profile (Phase 2.1 A).
enum Source { learnedSource, adaptiveSource, bothSources };
enum EditMode { smoothPoints, flatPoints, freehand };

constexpr int numCurves = 3; // EQ, Gate and Compressor

constexpr int numSizes = 9;
constexpr int sizes[numSizes] { 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };

// At and above this size, a frame's work is spread over half a hop.
constexpr int firstStagedSize = 8192;

constexpr double curveLowHz = 20.0, curveHighHz = 24000.0;
constexpr int curvePoints = 512;

constexpr double profileHighHz = 24000.0;
constexpr int profilePoints = 2049;

constexpr int displayColumns = 128;

inline double curveHz (int index) noexcept
{
    return curveLowHz * std::pow (curveHighHz / curveLowHz, (double) index / (double) (curvePoints - 1));
}

inline double curvePosition (double hz) noexcept
{
    return std::clamp (std::log (hz / curveLowHz) / std::log (curveHighHz / curveLowHz) * (double) (curvePoints - 1), 0.0, (double) (curvePoints - 1));
}

inline double profileHz (int index) noexcept
{
    return profileHighHz * (double) index / (double) (profilePoints - 1);
}

inline double displayHz (int column) noexcept
{
    return curveLowHz * std::pow (1000.0, (double) column / (double) displayColumns);
}

// What the message thread and the audio thread share, all lock-free.
struct Shared
{
    Shared()
    {
        for (auto& curve : curves)
            for (auto& value : curve)
                value.store (0.0f);

        for (auto& value : profile)
            value.store (0.0f);

        for (auto& value : reduction)
            value.store (0.0f);

        for (auto& value : adaptive)
            value.store (0.0f);
    }

    // The active curve of each mode, in dB at curveHz (i).
    std::array<std::array<std::atomic<float>, curvePoints>, numCurves> curves;
    std::array<std::atomic<int>, numCurves> curveVersions {};

    // Learned noise power at profileHz (i), divided by the analysis window's energy.
    std::array<std::atomic<float>, profilePoints> profile;
    std::atomic<int> profileVersion { 0 };
    std::atomic<bool> hasProfile { false };
    std::atomic<float> profileSeconds { 0.0f };

    // Learning, started by the message thread and finished by the audio thread.
    std::atomic<bool> learnRequested { false }, learning { false };
    std::atomic<int> learnedSamples { 0 }, learnTarget { 0 };

    // Gain reduction in dB, the most in each of displayColumns log-spaced bands.
    std::array<std::atomic<float>, displayColumns> reduction;

    // Phase 2.1 A: the adaptive noise estimate, on the profile's grid and in its units, written
    // by the engine that's heard. Not saved. A new engine (after an FFT size change) starts
    // from it while hasAdaptive is set.
    std::array<std::atomic<float>, profilePoints> adaptive;
    std::atomic<bool> hasAdaptive { false };
    std::atomic<int> adaptiveVersion { 0 };

    // For the card: the estimate's average from 100 Hz to 8 kHz in the graph's dB, and the share
    // of bins below 8 kHz being held as signal.
    std::atomic<float> adaptiveFloorDb { -200.0f }, heldFraction { 0.0f };
};

} // namespace fir
