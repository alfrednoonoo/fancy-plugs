#pragma once

#include "EqFilters.h"

#include <fancy/dsp/Analyzer.h>

#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// EQ Match (Phase 3 Q2.4): learns the long-term average spectrum of a reference and of the current
// material, and fits ordinary bells and shelves to the difference. Nothing here is in the audio
// path: the audio thread only pushes samples into two FIFOs while the Match card is learning.
namespace eq::match
{

// Every curve is in dB on this grid: 48 points to the octave from 20 Hz to just under 20 kHz.
constexpr int pointsPerOctave = 48;
constexpr double gridLowHz = 20.0;
constexpr int numPoints = 479;

using Curve = std::array<double, numPoints>;

inline double hzAt (int point) noexcept
{
    return gridLowHz * std::pow (2.0, (double) point / (double) pointsPerOctave);
}

struct Settings
{
    double smoothingOctaves = 1.0 / 3.0;
    double amount = 1.0;          // 0 to 1
    int maxBands = 8;
    double lowHz = 30.0, highHz = 18000.0;

    bool operator== (const Settings&) const = default;
};

constexpr double minSeconds = 3.0;      // of each spectrum, before Match is offered
constexpr double maxGainDb = 12.0;
constexpr double minQ = 0.3, maxQ = 8.0;

// The long-term average power spectrum of what it's given, on the grid. Frames under -60 dBFS
// are left out, so the gaps in a recording don't pull the average down.
class Learner
{
public:
    // Allocates. What has been learned is kept, as it's stored by frequency.
    void prepare (double sampleRate);

    void clear() noexcept;
    void addSamples (const float* samples, int numSamples);

    double getSeconds() const noexcept { return seconds; }
    Curve getSpectrumDb() const;

    // Puts back a saved spectrum, to match again or to go on learning.
    void restore (const Curve& spectrumDb, double learnedSeconds);

private:
    struct Span
    {
        int first = 0, last = 0;    // bins averaged, when the point is wider than a bin
        double fraction = 0.0;      // otherwise, between first and first + 1
        bool average = false;
    };

    void analyse();

    double rate = 0.0;
    int size = 0, hop = 0, fill = 0;
    double scale = 1.0;
    double seconds = 0.0;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, input, frame;
    std::array<Span, numPoints> spans {};
    std::array<double, numPoints> meanPower {};
};

// A moving average over the given width, with a raised-cosine weight.
Curve smooth (const Curve& curve, double octaves);

// What the new bands should add: reference minus current, smoothed, with its mean between 200 Hz
// and 5 kHz taken out (a difference in loudness isn't a tone), scaled by Amount, held to
// +/-12 dB, less what the bands that stay already do, and faded to 0 outside the range.
Curve makeTarget (const Curve& referenceDb, const Curve& currentDb, const Curve& existingDb, const Settings& settings);

// The response of a set of bands in dB, on the grid.
Curve responseOf (const std::vector<BandSpec>& bands, double sampleRate);

// Low and high shelves at the target's ends if it needs them, then a bell at the largest remaining
// error until maxBands or until another band hardly helps, with every band's Freq, Gain and Q
// refined together (Levenberg-Marquardt) after each one. Deterministic. Sorted by frequency.
std::vector<BandSpec> fit (const Curve& targetDb, const Settings& settings, double sampleRate);

// The RMS of a curve, or of the difference of two, between two frequencies.
double rmsBetween (const Curve& curve, double lowHz, double highHz);
double rmsDifference (const Curve& a, const Curve& b, double lowHz, double highHz);

// The capture and the worker thread behind the Match card. The audio thread pushes; the worker
// runs the FFTs and the fit; the message thread reads the results.
class Engine final : private juce::Thread
{
public:
    enum class Source { sidechain, captured };

    // both: the sidechain is the reference and the main input the current, at once.
    enum class Learning { none, both, reference, current };

    // One band the last Match wrote, as its parameters read back.
    struct Written
    {
        int band = 0, type = 0;
        float frequencyHz = 0.0f, gainDb = 0.0f, q = 0.0f;
    };

    Engine();
    ~Engine() override;

    // Audio thread. They do nothing unless the card is learning.
    void pushMain (const float* const* channels, int numChannels, int numSamples) noexcept;
    void pushSidechain (const float* const* channels, int numChannels, int numSamples) noexcept;
    bool isLearningFromSidechain() const noexcept { return learning.load() == (int) Learning::both; }

    // Message thread ---------------------------------------------------------------------

    // The worker runs only while the card is open. Closing it stops any learning.
    void setPanelOpen (bool shouldBeOpen);
    bool isPanelOpen() const noexcept { return isThreadRunning(); }

    void setSampleRate (double newRate) noexcept { sampleRate.store (newRate, std::memory_order_relaxed); }

    void setLearning (Learning newLearning) noexcept { learning.store ((int) newLearning); }
    Learning getLearning() const noexcept { return (Learning) learning.load(); }

    void clear();

    double getReferenceSeconds() const;
    double getCurrentSeconds() const;
    bool isReady() const { return getReferenceSeconds() >= minSeconds && getCurrentSeconds() >= minSeconds; }

    Curve getReferenceDb() const;
    Curve getCurrentDb() const;

    // Changes whenever either spectrum does.
    int getVersion() const noexcept { return version.load (std::memory_order_relaxed); }

    // Starts a fit on the worker, replacing one that hasn't started. takeFit() gives the result
    // once, when it's there.
    void requestFit (const Curve& target, const Settings& fitSettings, double rate);
    bool isFitting() const;
    bool takeFit (std::vector<BandSpec>& bands);

    // For tests and screenshots: learns from samples on the calling thread, or takes spectra as
    // they are.
    void learnNow (bool reference, const float* samples, int numSamples, double rate);
    void restoreSpectra (const Curve& referenceDb, const Curve& currentDb, double seconds);

    Settings settings;
    Source source = Source::sidechain;

    std::vector<Written> getWritten() const;
    void setWritten (std::vector<Written> newWritten);

    // Saved with the project, so it can match again without learning again.
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree& tree);

    static const juce::Identifier treeType;

private:
    void run() override;
    void prepareLearners (double rate);

    // Made when the card first opens, so an instance that never uses Match doesn't carry them.
    // The audio thread reads the pointers, which are set once and stay.
    std::unique_ptr<fancy::dsp::SampleFifo> mainStore, sidechainStore;
    std::atomic<fancy::dsp::SampleFifo*> mainFifo { nullptr }, sidechainFifo { nullptr };
    std::atomic<int> learning { (int) Learning::none };
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<int> version { 0 };

    juce::CriticalSection dataLock;
    std::vector<Written> written;
    Learner reference, current;
    double preparedRate = 0.0;
    std::vector<float> scratch;

    // Requests are numbered: the worker starts the newest, and a result counts only if nothing
    // newer was asked for meanwhile.
    juce::CriticalSection fitLock;
    int fitRequested = 0, fitStarted = 0, fitFinished = 0, fitTaken = 0;
    Curve fitTarget {};
    Settings fitSettings;
    double fitRate = 48000.0;
    std::vector<BandSpec> fitResult;
};

} // namespace eq::match
