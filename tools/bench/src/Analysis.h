#pragma once

// FancyBench's signal generation and analysis (docs/phase3-Q1.md §4). Plain C++ with no JUCE, so
// the Catch2 suite tests it on synthetic signals with known answers (tests/BenchTests.cpp).

#include <complex>
#include <cstdint>
#include <vector>

namespace bench
{

using Signal = std::vector<float>;

constexpr double floorDb = -400.0; // what a level of exactly zero reads as

double toDb (double amplitude) noexcept;
double powerToDb (double power) noexcept;

// In-place radix-2 FFT; the size must be a power of two.
void fft (std::vector<std::complex<double>>& data, bool inverse);
int nextPowerOfTwo (int n) noexcept;

// Generators. Levels are dBFS: RMS for noise, peak for sines and sweeps.
Signal whiteNoise (int numSamples, double rmsDb, uint32_t seed);
Signal pinkNoise (int numSamples, double rmsDb, uint32_t seed);
Signal uniformNoise (int numSamples, float amplitude, uint32_t seed); // as the Catch2 CPU tests use
Signal sine (int numSamples, double hz, double rate, double peakDb, double phase = 0.0);

// The THD and aliasing analysis length at a rate: the power of two at or above one second.
int analysisLength (double rate) noexcept;
// A frequency moved to the nearest analysis bin, so a sine and its harmonics fall on exact bins.
double binCentred (double hz, double rate) noexcept;

struct Sweep
{
    Signal signal;
    double startHz = 0.0, endHz = 0.0, rate = 0.0;
};

// Exponential sine sweep (Farina), with 10 ms and 5 ms fades.
Sweep logSweep (double startHz, double endHz, double seconds, double rate, double peakDb);

// Latency: the lag of the input-output cross-correlation peak, from minLag to maxLag samples.
// Returns minLag - 1 if the output is silent.
int measureDelay (const Signal& input, const Signal& output, int minLag, int maxLag);

struct ResponsePoint
{
    double hz = 0.0, db = 0.0, degrees = 0.0;
};

std::vector<double> logFrequencies (double lowHz, double highHz, int perOctave);

// Frequency response from a sweep and the plugin's output (starting where the sweep started, and
// running on past it for the tail). The impulse response is windowed from `latency - pre` to
// `latency + post`, which removes the sweep's harmonic responses, and the phase is relative to
// `latency`.
std::vector<ResponsePoint> sweepResponse (const Sweep& sweep, const Signal& output, int latency,
                                          const std::vector<double>& frequencies);

struct Distortion
{
    double fundamentalDbfs = floorDb;
    double thdDb = floorDb, thdPercent = 0.0;
    double thdnDb = floorDb, thdnPercent = 0.0;
};

// `output` is analysisLength (rate) samples of a sine at the bin-centred `hz`.
Distortion distortion (const Signal& output, double hz, double rate);

struct Aliasing
{
    double fundamentalDbfs = floorDb;
    double worstHz = 0.0, worstDbfs = floorDb, worstRelativeDb = floorDb;
    double totalRelativeDb = floorDb;
    int count = 0;
    std::vector<float> spectrumDbfs; // one value per bin up to Nyquist
};

Aliasing aliasing (const Signal& output, double hz, double rate);

struct Null
{
    double rmsDbfs = floorDb, peakDbfs = floorDb, relativeDb = floorDb;
};

// output[n] - input[n - delay], from `start` to the end of the output.
Null nullTest (const Signal& input, const Signal& output, int delay, int start);

struct Clicks
{
    double peakDbfs = floorDb;
    int count = 0;
};

// Clicks on a sine: the output with the sine notched out and high-passed at 4 kHz, counted as
// bursts over thresholdDb, bursts closer than 10 ms counted as one. The first 100 ms is skipped.
Clicks clicks (const Signal& output, double sineHz, double rate, double thresholdDb);

double percentile (std::vector<double> values, double fraction);
double median (std::vector<double> values);

} // namespace bench
