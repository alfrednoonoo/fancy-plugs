// Phase 4 Wave 1 for Fancy Comp: Hold, Range, Listen to removed and the level history
// (docs/comp.md, "Phase 4 Wave 1", section 7). The Hold margin comes from
// docs/research/comp-w1-model.py.

#include "CompDsp.h"
#include "CompProcessor.h"
#include "LegacyCompQ25Dsp.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <fancy/Decibels.h>
#include <fancy/ui/Controls.h>
#include <fancy/ui/LevelHistory.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <random>
#include <utility>
#include <vector>

namespace
{
constexpr double pi = std::numbers::pi;
constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples, float value = 0.0f) : left ((size_t) numSamples, value), right ((size_t) numSamples, value) {}

    int size() const { return (int) left.size(); }

    template <typename Dsp>
    void process (Dsp& dsp, Stereo* key = nullptr, int blockSize = 256)
    {
        for (int start = 0; start < size(); start += blockSize)
        {
            float* main[] { left.data() + start, right.data() + start };
            const float* keyChannels[2] {};

            if (key != nullptr)
            {
                keyChannels[0] = key->left.data() + start;
                keyChannels[1] = key->right.data() + start;
            }

            dsp.process (main, 2, keyChannels, key != nullptr ? 2 : 0, std::min (blockSize, size() - start));
        }
    }
};

// A signal with transients, a changing level and different channels, for comparing renders.
Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.05 + 0.9 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 3.0 * i / rate), 4.0);
        const auto tone = std::sin (2.0 * pi * 220.0 * i / rate) + 0.3 * std::sin (2.0 * pi * 3300.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.7 * tone + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.5 * tone + 0.3 * noise (random)));
    }

    return signal;
}

Stereo sine (double hz, double amplitude, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate));

    return signal;
}

// DC at one level, then another from `step` on: the output over the input is the gain itself.
Stereo stepped (double first, double second, int step, int numSamples)
{
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (i < step ? first : second);

    return signal;
}

double levelAt (const std::vector<float>& samples, int from, int count, double hz)
{
    std::complex<double> sum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto phase = 2.0 * pi * hz * i / rate;
        sum += (double) samples[(size_t) (from + i)] * std::complex<double> (std::cos (phase), -std::sin (phase));
    }

    return 2.0 * std::abs (sum) / (double) count;
}

// Harmonics 2 to 10 against the fundamental over a whole number of cycles at the end.
double thdDb (const std::vector<float>& samples, double hz)
{
    const auto count = (int) (0.5 * rate);
    const auto from = (int) samples.size() - count;
    const auto fundamental = levelAt (samples, from, count, hz);
    auto rest = 0.0;

    for (int k = 2; k <= 10; ++k)
        rest += std::pow (levelAt (samples, from, count, hz * k), 2.0);

    return 20.0 * std::log10 (std::max (std::sqrt (rest), 1.0e-12) / fundamental);
}

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets, double sampleRate = rate)
{
    dsp.prepare (sampleRate);
    dsp.snapToTargets (targets);
}

void setParameter (CompProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getApvts().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<float> render (CompProcessor& processor, int numSamples)
{
    juce::AudioBuffer<float> buffer (4, numSamples);
    buffer.clear();
    juce::MidiBuffer midi;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = (float) (0.5 * std::sin (2.0 * pi * 300.0 * i / rate) + 0.2 * std::sin (2.0 * pi * 4100.0 * i / rate));
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample * 0.7f);
    }

    processor.processBlock (buffer, midi);

    std::vector<float> out (buffer.getReadPointer (0), buffer.getReadPointer (0) + numSamples);
    out.insert (out.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + numSamples);
    return out;
}

// Infinite ratio at -40 dB, peak detection, Oversampling Off: a DC input of 1.0 is reduced by
// 40 dB, and the output over the input reads the gain directly.
CompDsp::Targets brickwall()
{
    CompDsp::Targets targets;
    targets.thresholdDb = -40.0;
    targets.ratio = CompDsp::infiniteRatio;
    targets.kneeDb = 0.0;
    targets.attackMs = 1.0;
    targets.rmsMs = 0.0;
    return targets;
}

double gainDbAt (const Stereo& in, const Stereo& out, int index)
{
    return 20.0 * std::log10 (std::abs ((double) out.left[(size_t) index] / (double) in.left[(size_t) index]));
}
} // namespace

// 1. Compatibility ------------------------------------------------------------

TEST_CASE ("Wave 1 at its defaults renders exactly as Comp did in v1.6.0", "[comp][w1]")
{
    const auto oversampling = GENERATE (0, 1, 2, 3);
    const auto variant = GENERATE (0, 1, 2, 3, 4, 5, 6, 7, 8);

    LegacyCompQ25Dsp::Targets old;
    old.thresholdDb = -24.0;
    old.ratio = 4.0;
    old.kneeDb = 3.0;
    old.attackMs = 5.0;
    old.releaseMs = 120.0;
    old.oversampling = oversampling;
    old.wetGain = 0.8;
    old.dryGain = 0.3;

    switch (variant)
    {
        case 1: old.highpassHz = 150.0; old.lowpassHz = 6000.0; break;
        case 2: old.classic = true; old.autoRelease = true; old.weirdKnee = true; break;
        case 3: old.feedback = true; old.linkChannels = false; old.preCompMs = 3.0; break;
        case 4: old.detector = LegacyCompQ25Dsp::sideLR; old.limitOutput = true; old.minimumPhase = true; break;
        case 5: old.autoMakeup = true; old.linearRelease = true; old.attack90 = true; old.detector = LegacyCompQ25Dsp::mid; old.rmsMs = 30.0; break;
        case 6: old.character = LegacyCompQ25Dsp::warm; old.characterPost = false; old.driveDb = 12.0; break;
        case 7: old.upward = true; old.autoRelease = true; old.classic = true; break;
        case 8: old.autoThreshold = true; old.eq[LegacyCompQ25Dsp::bell].gainDb = 6.0; old.autoMakeup = true; break;
        default: break;
    }

    CompDsp::Targets now;
    now.thresholdDb = old.thresholdDb;
    now.ratio = old.ratio;
    now.kneeDb = old.kneeDb;
    now.attackMs = old.attackMs;
    now.releaseMs = old.releaseMs;
    now.preCompMs = old.preCompMs;
    now.rmsMs = old.rmsMs;
    now.lowpassHz = old.lowpassHz;
    now.highpassHz = old.highpassHz;
    now.wetGain = old.wetGain;
    now.dryGain = old.dryGain;
    now.outputGain = old.outputGain;
    now.linkAmount = old.linkAmount;
    now.detector = old.detector;
    now.oversampling = old.oversampling;
    now.weirdKnee = old.weirdKnee;
    now.classic = old.classic;
    now.autoRelease = old.autoRelease;
    now.linearRelease = old.linearRelease;
    now.attack90 = old.attack90;
    now.minimumPhase = old.minimumPhase;
    now.linkChannels = old.linkChannels;
    now.feedback = old.feedback;
    now.autoMakeup = old.autoMakeup;
    now.previewFilter = old.previewFilter;
    now.limitOutput = old.limitOutput;
    now.bypass = old.bypass;
    now.character = old.character;
    now.characterPost = old.characterPost;
    now.driveDb = old.driveDb;
    now.upward = old.upward;
    now.upThresholdDb = old.upThresholdDb;
    now.upRatio = old.upRatio;
    now.maxLiftDb = old.maxLiftDb;
    now.floorDb = old.floorDb;
    now.autoThreshold = old.autoThreshold;

    for (size_t b = 0; b < (size_t) CompDsp::numEqBands; ++b)
    {
        now.eq[b].frequencyHz = old.eq[b].frequencyHz;
        now.eq[b].gainDb = old.eq[b].gainDb;
        now.eq[b].q = old.eq[b].q;
    }

    LegacyCompQ25Dsp legacy;
    CompDsp current;
    prepare (legacy, old);
    prepare (current, now);

    REQUIRE (current.getLatencySamples() == legacy.getLatencySamples());

    auto a = programme (24000, 7);
    auto b = a;
    auto keyA = programme (24000, 8);
    auto keyB = keyA;

    a.process (legacy, &keyA, 333);
    b.process (current, &keyB, 333);

    INFO ("oversampling " << oversampling << ", variant " << variant);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("a v1.6.0 state loads with Hold and Range at their defaults, and renders the same", "[comp][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor saved;
    setParameter (saved, "threshold", -22.0f);
    setParameter (saved, "makeup", 1.0f);

    juce::MemoryBlock state;
    saved.getStateInformation (state);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    auto removed = 0;

    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();

        if (child->hasTagName ("PARAM") && (child->getStringAttribute ("id") == "hold" || child->getStringAttribute ("id") == "range"))
        {
            xml->removeChildElement (child, true);
            ++removed;
        }

        child = next;
    }

    CHECK (removed == 2);

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    CompProcessor loaded;
    setParameter (loaded, "hold", 120.0f);
    setParameter (loaded, "range", 6.0f);
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    auto& apvts = loaded.getApvts();
    CHECK (apvts.getRawParameterValue ("hold")->load() == 0.0f);
    CHECK (apvts.getParameter ("range")->getCurrentValueAsText() == "Off");

    saved.prepareToPlay (rate, 4096);
    loaded.prepareToPlay (rate, 4096);

    CHECK (render (saved, 4096) == render (loaded, 4096));
}

TEST_CASE ("Range shows and takes Off at the top of its travel", "[comp][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor processor;
    auto* range = processor.getApvts().getParameter ("range");

    CHECK (range->getCurrentValueAsText() == "Off");
    CHECK (range->getText (range->convertTo0to1 (12.0f), 0) == "12.0 dB");
    CHECK (range->convertFrom0to1 (range->getValueForText ("off")) == Catch::Approx (41.0f));
    CHECK (range->convertFrom0to1 (range->getValueForText ("9")) == Catch::Approx (9.0f));
}

// 3 and 4. Hold ---------------------------------------------------------------

TEST_CASE ("Hold keeps the reduction for its time, then releases", "[comp][w1]")
{
    constexpr int step = 9600; // 0.2 s at 0 dBFS, then -40 dBFS
    const auto in = stepped (1.0, 0.01, step, step + 24000);

    SECTION ("Hold 100 ms")
    {
        auto targets = brickwall();
        targets.holdMs = 100.0;

        CompDsp dsp;
        prepare (dsp, targets);

        auto out = in;
        out.process (dsp);

        CHECK (gainDbAt (in, out, step - 1) == Catch::Approx (-40.0).margin (0.05));
        CHECK (gainDbAt (in, out, step + 4752) == Catch::Approx (-40.0).margin (0.1)); // 99 ms
        CHECK (gainDbAt (in, out, step + 4900) > -39.9);                                // 102 ms
    }

    SECTION ("Hold 0 releases at once")
    {
        CompDsp dsp;
        prepare (dsp, brickwall());

        auto out = in;
        out.process (dsp);

        CHECK (gainDbAt (in, out, step + 480) > -39.0); // 10 ms: about 2 dB back at a 200 ms release
    }
}

TEST_CASE ("Hold takes the distortion off a low note with fast settings", "[comp][w1]")
{
    const auto thdWith = [] (double holdMs)
    {
        CompDsp::Targets targets;
        targets.thresholdDb = -24.0;
        targets.ratio = 4.0;
        targets.kneeDb = 0.0;
        targets.attackMs = 1.0;
        targets.releaseMs = 20.0;
        targets.rmsMs = 0.0;
        targets.holdMs = holdMs;

        CompDsp dsp;
        prepare (dsp, targets);

        auto signal = sine (50.0, fancy::dbToGain (-12.0), (int) rate);
        signal.process (dsp);
        return thdDb (signal.left, 50.0);
    };

    const auto without = thdWith (0.0);
    const auto with = thdWith (30.0);

    INFO ("THD at Hold 0: " << without << " dB, at Hold 30 ms: " << with << " dB");
    CHECK (with < without - 30.0);
    CHECK (with < -60.0);
}

TEST_CASE ("Hold doesn't hold the upward lift", "[comp][w1]")
{
    // A quiet passage lifts; then a louder one below Threshold, so nothing is reduced and only
    // the lift moves. It falls back at once, Hold or not.
    auto targets = brickwall();
    targets.thresholdDb = -10.0;
    targets.ratio = 2.0;
    targets.upward = true;
    targets.upThresholdDb = -30.0;
    targets.holdMs = 400.0;

    constexpr int step = 9600;
    const auto in = stepped (0.003, 0.2, step, step + 9600);

    CompDsp held, plain;
    prepare (held, targets);
    targets.holdMs = 0.0;
    prepare (plain, targets);

    auto a = in, b = in;
    a.process (held);
    b.process (plain);

    const auto liftBefore = gainDbAt (in, a, step - 1);
    CHECK (liftBefore > 8.0);

    // 50 ms into the louder part both have released by the same amount.
    CHECK (gainDbAt (in, a, step + 2400) == Catch::Approx (gainDbAt (in, b, step + 2400)).margin (0.01));
    CHECK (gainDbAt (in, a, step + 2400) < liftBefore - 1.0);
}

// 5. Range --------------------------------------------------------------------

TEST_CASE ("Range caps the reduction", "[comp][w1]")
{
    const auto classic = GENERATE (false, true);
    const auto in = stepped (1.0, 1.0, 0, 24000);

    for (const auto range : { 6.0, 12.0, 24.0, 0.0 })
    {
        auto targets = brickwall();
        targets.classic = classic;
        targets.rangeDb = range;

        CompDsp dsp;
        prepare (dsp, targets);

        auto out = in;
        out.process (dsp);

        INFO ("Classic " << classic << ", Range " << range);
        CHECK (gainDbAt (in, out, 23999) == Catch::Approx (range > 0.0 ? -range : -40.0).margin (0.1));
    }
}

TEST_CASE ("Auto make-up counts only what Range lets Comp take off", "[comp][w1]")
{
    const auto in = stepped (1.0, 1.0, 0, 24000);

    auto targets = brickwall();
    targets.autoMakeup = true;
    targets.rangeDb = 6.0;

    CompDsp dsp;
    prepare (dsp, targets);

    auto out = in;
    out.process (dsp);

    // -6 dB of reduction and half of 6 dB of make-up.
    CHECK (gainDbAt (in, out, 23999) == Catch::Approx (-3.0).margin (0.1));
}

// 6. Listen to removed ---------------------------------------------------------

TEST_CASE ("Listen to removed plus the normal output is the input", "[comp][w1]")
{
    const auto oversampling = GENERATE (0, 2);

    CompDsp::Targets targets;
    targets.thresholdDb = -30.0;
    targets.ratio = 4.0;
    targets.attackMs = 2.0;
    targets.releaseMs = 80.0;
    targets.oversampling = oversampling;
    targets.wetGain = 0.5;  // ignored while listening
    targets.dryGain = 0.25; // ignored while listening

    const auto in = programme (24000, 11);

    // The normal output, with Wet 1 and Dry off so it is only the compressed signal.
    auto normalTargets = targets;
    normalTargets.wetGain = 1.0;
    normalTargets.dryGain = 0.0;

    CompDsp normal, removed, reference;
    prepare (normal, normalTargets);

    targets.listenRemoved = true;
    prepare (removed, targets);

    // The input through the same oversampler with nothing compressed.
    auto transparentTargets = normalTargets;
    transparentTargets.ratio = 1.0;
    prepare (reference, transparentTargets);

    auto a = in, b = in, c = in;
    a.process (normal);
    b.process (removed);
    c.process (reference);

    auto worst = 0.0, removedEnergy = 0.0;

    for (int i = 0; i < in.size(); ++i)
    {
        for (auto channel : { &Stereo::left, &Stereo::right })
        {
            const auto sum = (double) (a.*channel)[(size_t) i] + (double) (b.*channel)[(size_t) i];
            worst = std::max (worst, std::abs (sum - (double) (c.*channel)[(size_t) i]));
            removedEnergy = std::max (removedEnergy, std::abs ((double) (b.*channel)[(size_t) i]));
        }
    }

    INFO ("oversampling " << oversampling << ", worst " << 20.0 * std::log10 (std::max (worst, 1.0e-30)) << " dBFS");
    CHECK (worst < fancy::dbToGain (oversampling == 0 ? -120.0 : -100.0));
    CHECK (removedEnergy > 0.05); // and it isn't silence
}

// 7. Two listens ----------------------------------------------------------------

TEST_CASE ("Preview filter and Listen to removed are exclusive and never saved", "[comp][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    CompProcessor processor;
    processor.editorOpened();

    juce::MemoryBlock before;
    processor.getStateInformation (before);

    fancy::ui::ListenCheckbox preview (processor, "Preview filter");
    fancy::ui::ListenCheckbox removed (processor, "Listen to removed", fancy::ui::colours::uiTeal, 1);

    CHECK_FALSE (preview.isOn());
    CHECK_FALSE (removed.isOn());

    removed.click();
    CHECK (removed.isOn());
    CHECK_FALSE (preview.isOn());

    preview.click();
    CHECK (preview.isOn());
    CHECK_FALSE (removed.isOn());

    removed.click();
    CHECK (removed.isOn());

    juce::MemoryBlock after;
    processor.getStateInformation (after);
    CHECK (before == after);

    processor.editorClosed();
    CHECK_FALSE (preview.isOn());
    CHECK_FALSE (removed.isOn());
}

// 8. History ----------------------------------------------------------------------

TEST_CASE ("the history makes 476 columns per 1.5 s at any rate and block size", "[comp][w1]")
{
    const auto sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto blockSize = GENERATE (1, 333, 4096);
    const auto oversampling = GENERATE (0, 2);

    auto targets = brickwall();
    targets.oversampling = oversampling;

    CompDsp dsp;
    prepare (dsp, targets, sampleRate);

    const auto numSamples = (int) (3.0 * sampleRate);
    auto signal = stepped (1.0, 1.0, 0, numSamples);
    signal.process (dsp, nullptr, blockSize);

    std::vector<fancy::dsp::LevelColumn> columns (8192);
    const auto count = dsp.popHistory (columns.data(), (int) columns.size());

    INFO ("rate " << sampleRate << ", block " << blockSize << ", oversampling " << oversampling);
    CHECK (std::abs (count - 952) <= 1);

    // After the attack, every column shows the 40 dB reduction and the threshold in use.
    CHECK (columns[(size_t) (count - 1)].reductionDb[0] == Catch::Approx (40.0f).margin (0.1f));
    CHECK (columns[(size_t) (count - 1)].thresholdDb == Catch::Approx (-40.0f));
    CHECK (columns[(size_t) (count - 1)].inputPeak == Catch::Approx (1.0f).margin (0.01f));
    CHECK (columns[(size_t) (count - 1)].outputPeak == Catch::Approx (0.01f).margin (0.001f));
}

TEST_CASE ("the history marks beats and bars from the host's tempo", "[comp][w1]")
{
    CompDsp dsp;
    prepare (dsp, brickwall());

    CompDsp::Transport transport;
    transport.playing = true;
    transport.bpm = 120.0;
    transport.ppq = 0.0;

    // 2.2 s in blocks of 480, the position advancing as a host's does.
    constexpr int blockSize = 480;
    auto signal = stepped (0.1, 0.1, 0, (int) (2.2 * rate));

    for (int start = 0; start < signal.size(); start += blockSize)
    {
        transport.ppq = (double) start / rate * 2.0;
        dsp.setTransport (transport);

        float* main[] { signal.left.data() + start, signal.right.data() + start };
        dsp.process (main, 2, nullptr, 0, std::min (blockSize, signal.size() - start));
    }

    std::vector<fancy::dsp::LevelColumn> columns (8192);
    const auto count = dsp.popHistory (columns.data(), (int) columns.size());
    const auto secondsPerColumn = 1.5 / 476.0;

    std::vector<std::pair<int, int>> marks;

    for (int c = 0; c < count; ++c)
        if (columns[(size_t) c].mark != fancy::dsp::LevelColumn::none)
            marks.emplace_back (c, columns[(size_t) c].mark);

    REQUIRE (marks.size() == 4);

    for (size_t m = 0; m < marks.size(); ++m)
    {
        const auto expected = (int) std::floor (0.5 * (double) (m + 1) / secondsPerColumn);
        CHECK (std::abs (marks[m].first - expected) <= 1);
        CHECK (marks[m].second == (m == 3 ? fancy::dsp::LevelColumn::bar : fancy::dsp::LevelColumn::beat));
    }
}

TEST_CASE ("the history's peak read-out is the largest reduction in view since the last clear", "[comp][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::vector<fancy::dsp::LevelColumn> columns (1500);

    for (size_t c = 0; c < columns.size(); ++c)
        columns[c].reductionDb[0] = columns[c].reductionDb[1] = c == 100 ? 18.0f : (c == 1400 ? 5.0f : 1.0f);

    fancy::ui::LevelHistory history;
    history.setSize (476, 260);
    history.show (columns);

    history.setZoom (0); // 476 columns: only the 5 dB peak is in view
    CHECK (history.getPeakReduction() == 5.0f);

    history.setZoom (2); // 1904 columns: the 18 dB one too
    CHECK (history.getPeakReduction() == 18.0f);

    history.clearPeak();
    history.setPaused (false);
    std::vector<fancy::dsp::LevelColumn> next (10);
    next[3].reductionDb[1] = 2.5f;
    history.push (next.data(), (int) next.size());
    CHECK (history.getPeakReduction() == 2.5f);
}

TEST_CASE ("the history draws levels in dB from -60 at the centre to 0 dBFS at the edge", "[comp][w1]")
{
    using fancy::ui::LevelHistory;
    constexpr float amplitude = 122.0f; // Comp's graph: 260 px high, 8 px margins

    CHECK (LevelHistory::levelDistance (1.0f, amplitude) == Catch::Approx (amplitude).margin (0.5));
    CHECK (LevelHistory::levelDistance (2.0f, amplitude) == Catch::Approx (amplitude).margin (0.5));
    CHECK (LevelHistory::levelDistance (std::pow (10.0f, -30.0f / 20.0f), amplitude) == Catch::Approx (amplitude / 2.0f).margin (0.5));
    CHECK (LevelHistory::levelDistance (std::pow (10.0f, -60.0f / 20.0f), amplitude) == Catch::Approx (0.0f).margin (0.5));
    CHECK (LevelHistory::levelDistance (1.0e-5f, amplitude) == 0.0f);
    CHECK (LevelHistory::levelDistance (0.0f, amplitude) == 0.0f);
}

// 10. CPU ---------------------------------------------------------------------------

TEST_CASE ("comp Wave 1 CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s of audio
    constexpr int runs = 5;

    const auto timeOf = [] (const CompDsp::Targets& targets)
    {
        std::vector<double> results;

        for (int run = 0; run < runs; ++run)
        {
            CompDsp dsp;
            prepare (dsp, targets);

            auto signal = programme (blockSize * 64, 99);
            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < numBlocks; ++i)
            {
                const auto offset = (i % 64) * blockSize;
                float* channels[] { signal.left.data() + offset, signal.right.data() + offset };
                dsp.process (channels, 2, nullptr, 0, blockSize);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            results.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
        }

        std::sort (results.begin(), results.end());
        return results[runs / 2];
    };

    const CompDsp::Targets defaults;
    const auto reference = timeOf (defaults);

    auto wave1 = defaults;
    wave1.holdMs = 30.0;
    wave1.rangeDb = 12.0;

    const auto withWave1 = timeOf (wave1);

    std::printf ("Comp Wave 1 CPU, 48 kHz, 128 samples, stereo, median of %d runs:\n", runs);
    std::printf ("  defaults (history on)    %.3f %%\n", reference);
    std::printf ("  Hold 30 ms, Range 12 dB  %.3f %%, adds %.3f %% (target < 0.1 %%)\n", withWave1, withWave1 - reference);
}
