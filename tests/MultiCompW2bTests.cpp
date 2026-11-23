// Phase 4 Wave 2, items 23 to 26, for Fancy MultiComp: per-band lookahead, detector EQ,
// per-crossover slopes and the GR history (docs/multicomp.md, "Phase 4 Wave 2: per-band
// lookahead, detector EQ, crossover slope and GR history", section 6). The numbers in tests 3
// and 3b come from docs/research/multicomp-w2b-prepush.py.

#include "LegacyMultiCompW2Dsp.h"
#include "MultiCompDsp.h"
#include "MultiCompProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;
constexpr double pi = std::numbers::pi;

struct Stereo
{
    std::vector<float> left, right;

    explicit Stereo (int numSamples) : left ((size_t) numSamples, 0.0f), right ((size_t) numSamples, 0.0f) {}

    int size() const { return (int) left.size(); }
};

template <typename Dsp, typename Targets>
void prepare (Dsp& dsp, const Targets& targets)
{
    dsp.prepare (rate);
    dsp.setSynchronousDesign (true);
    dsp.snapToTargets (targets);
}

template <typename Dsp>
void process (Dsp& dsp, Stereo& signal, int start, int count, int blockSize = 256)
{
    for (int done = 0; done < count; done += blockSize)
    {
        float* main[] { signal.left.data() + start + done, signal.right.data() + start + done };
        const float* key[2] {};
        dsp.process (main, 2, key, 0, std::min (blockSize, count - done));
    }
}

template <typename Dsp, typename Targets>
Stereo render (const Targets& targets, Stereo signal)
{
    Dsp dsp;
    prepare (dsp, targets);
    process (dsp, signal, 0, signal.size());
    return signal;
}

// Tones, noise and a level swing, different in each channel (as MultiCompQ26Tests').
Stereo programme (int numSamples, unsigned seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<double> noise (-1.0, 1.0);
    Stereo signal (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envelope = 0.03 + 0.9 * std::pow (0.5 + 0.5 * std::sin (2.0 * pi * 2.0 * i / rate), 3.0);
        const auto tones = std::sin (2.0 * pi * 80.0 * i / rate) + 0.5 * std::sin (2.0 * pi * 900.0 * i / rate)
                         + 0.3 * std::sin (2.0 * pi * 7000.0 * i / rate);

        signal.left[(size_t) i] = (float) (envelope * (0.5 * tones + 0.2 * noise (random)));
        signal.right[(size_t) i] = (float) (envelope * (0.35 * tones + 0.3 * noise (random)));
    }

    return signal;
}

// A tone with a 20 ms raised-cosine start, so it has no energy outside its own band.
Stereo softTone (int numSamples, double hz, double amplitude, int start)
{
    Stereo signal (numSamples);
    const auto fade = (int) (0.02 * rate);

    for (int i = start; i < numSamples; ++i)
    {
        const auto n = i - start;
        const auto envelope = n < fade ? 0.5 - 0.5 * std::cos (pi * n / fade) : 1.0;
        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) (amplitude * envelope * std::sin (2.0 * pi * hz * n / rate));
    }

    return signal;
}

template <typename Targets>
void setBands (Targets& targets)
{
    constexpr double thresholds[] { -24.0, -18.0, -28.0, -32.0 };
    constexpr double ratios[] { 3.0, 0.7, 4.0, 2.0 };

    for (size_t b = 0; b < 4; ++b)
    {
        targets.bands[b].thresholdDb = thresholds[b];
        targets.bands[b].ratio = ratios[b];
        targets.bands[b].attackMs = 5.0;
        targets.bands[b].releaseMs = 80.0;
        targets.bands[b].autoMakeup = b == 2;
    }

    targets.bands[0].topHz = 150.0;
    targets.bands[1].topHz = 1500.0;
    targets.bands[2].topHz = 6000.0;
    targets.bands[1].channel = 3; // Mid
    targets.bands[3].upward = true;
    targets.wetGain = 0.9;
    targets.dryGain = 0.2;
}

double blockDb (const std::vector<float>& x, const std::vector<bool>& mask)
{
    auto sum = 0.0;
    auto count = 0;

    for (size_t i = 0; i < x.size(); ++i)
    {
        if (mask[i])
        {
            sum += (double) x[i] * x[i];
            ++count;
        }
    }

    return 10.0 * std::log10 (std::max (sum / std::max (count, 1), 1.0e-30));
}

double levelDb (const std::vector<float>& x, int start, int count)
{
    auto sum = 0.0;

    for (int i = start; i < start + count; ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];

    return 10.0 * std::log10 (std::max (sum / count, 1.0e-30));
}

// Removes these parameters from a saved state, and from its A/B slot, as if saved before them.
void forget (juce::XmlElement& element, const std::function<bool (const juce::String&)>& matches)
{
    for (auto* child = element.getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();

        if (child->hasTagName ("PARAM") && matches (child->getStringAttribute ("id")))
            element.removeChildElement (child, true);
        else if (child->hasTagName ("WORKFLOW") || child->hasTagName ("SLOT"))
            forget (*child, matches);

        child = next;
    }
}

void setSaved (juce::XmlElement& element, const juce::String& id, double value)
{
    for (auto* child : element.getChildIterator())
        if (child->hasTagName ("PARAM") && child->getStringAttribute ("id") == id)
            child->setAttribute ("value", value);
}

juce::XmlElement* slotOf (juce::XmlElement& state)
{
    if (auto* workflow = state.getChildByName ("WORKFLOW"))
        return workflow->getChildByName ("SLOT");

    return nullptr;
}

float valueOf (MultiCompProcessor& processor, const juce::String& id)
{
    return processor.getApvts().getRawParameterValue (id)->load();
}

void gesture (MultiCompProcessor& processor, const juce::String& id, float value)
{
    auto& parameter = *processor.getApvts().getParameter (id);
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}
} // namespace

// 1. Legacy ---------------------------------------------------------------------------------------

TEST_CASE ("MultiComp W2b: with each band at the old Lookahead and the slopes from the old type, it renders as v1.7.0", "[multicomp][w2b]")
{
    const auto crossover = GENERATE (0, 1, 2, 3);
    const auto lookaheadMs = GENERATE (0.0, 3.0);
    const auto limit = GENERATE (false, true);

    LegacyMultiCompW2Dsp::Targets old;
    setBands (old);
    old.crossover = (legacymcw2::CrossoverType) crossover;
    old.lookaheadMs = lookaheadMs;
    old.limitOutput = limit;

    MultiCompDsp::Targets now;
    setBands (now);
    now.crossover = (multicomp::CrossoverType) crossover;
    now.lookaheadMs = 7.0; // ignored: every band has its own
    now.limitOutput = limit;
    now.perCrossoverSlopes = true;
    now.steepSlopes.fill (crossover == (int) multicomp::CrossoverType::lr48);

    for (auto& band : now.bands)
        band.lookaheadMs = lookaheadMs;

    const auto signal = programme (24000, 3);
    const auto a = render<LegacyMultiCompW2Dsp> (old, signal);
    const auto b = render<MultiCompDsp> (now, signal);

    INFO ("crossover " << crossover << ", lookahead " << lookaheadMs << " ms, limit " << limit);
    REQUIRE (a.left == b.left);
    REQUIRE (a.right == b.right);
}

TEST_CASE ("MultiComp W2b: a state saved before Wave 2 gives every band its Lookahead and every crossover its type's slope", "[multicomp][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor fresh;
    fresh.getWorkflow().copyToOtherSlot(); // so the state carries a B slot

    juce::MemoryBlock state;
    fresh.getStateInformation (state);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);

    const auto isNew = [] (const juce::String& id)
    {
        return (id.startsWith ("b") && (id.endsWith ("look") || id.endsWith ("dhp") || id.endsWith ("dlp") || id.endsWith ("tilt")))
            || (id.startsWith ("x") && id.endsWith ("slope"));
    };

    forget (*xml, isNew);
    setSaved (*xml, "lookahead", 3.0);
    setSaved (*xml, "crossover", 1.0); // LR 48

    auto* slot = slotOf (*xml);
    REQUIRE (slot != nullptr);
    setSaved (*slot, "lookahead", 5.0);
    setSaved (*slot, "crossover", 0.0);

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    MultiCompProcessor loaded;
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    for (int band = 0; band < multicomp::maxBands; ++band)
    {
        CHECK (valueOf (loaded, MultiCompProcessor::id (band, "look")) == Catch::Approx (3.0f));
        CHECK (valueOf (loaded, MultiCompProcessor::id (band, "dhp")) == Catch::Approx (MultiCompDsp::detectorOffHpHz));
        CHECK (valueOf (loaded, MultiCompProcessor::id (band, "dlp")) == Catch::Approx (MultiCompDsp::detectorOffLpHz));
        CHECK (valueOf (loaded, MultiCompProcessor::id (band, "tilt")) == 0.0f);
    }

    for (int crossover = 0; crossover < multicomp::maxCrossovers; ++crossover)
        CHECK (valueOf (loaded, MultiCompProcessor::slopeId (crossover)) == 1.0f);

    loaded.prepareToPlay (rate, 256);
    CHECK (loaded.getLatencySamples() == 144); // 3 ms at 48 kHz

    // The B slot is migrated from its own values.
    loaded.getWorkflow().switchSlot();
    CHECK (valueOf (loaded, MultiCompProcessor::id (0, "look")) == Catch::Approx (5.0f));
    CHECK (valueOf (loaded, MultiCompProcessor::id (7, "look")) == Catch::Approx (5.0f));
    CHECK (valueOf (loaded, MultiCompProcessor::slopeId (0)) == 0.0f);

    // Lookahead Glue sets each band's Look.
    auto& presets = loaded.getPresets();
    presets.load (presets.getNames().indexOf ("Lookahead Glue"));
    CHECK (valueOf (loaded, MultiCompProcessor::id (0, "look")) == Catch::Approx (2.0f));
    CHECK (valueOf (loaded, MultiCompProcessor::id (3, "look")) == Catch::Approx (2.0f));
}

TEST_CASE ("MultiComp W2b: Set all bands and the Crossover menu move every band's Look and every slope, and automation doesn't", "[multicomp][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    MultiCompProcessor processor;

    gesture (processor, "lookahead", 4.0f);

    for (int band = 0; band < multicomp::maxBands; ++band)
        CHECK (valueOf (processor, MultiCompProcessor::id (band, "look")) == Catch::Approx (4.0f));

    // A host writing the parameter isn't a person turning the knob.
    auto& lookahead = *processor.getApvts().getParameter ("lookahead");
    lookahead.setValueNotifyingHost (lookahead.convertTo0to1 (7.0f));
    CHECK (valueOf (processor, MultiCompProcessor::id (0, "look")) == Catch::Approx (4.0f));

    gesture (processor, "crossover", 1.0f); // LR 48

    for (int crossover = 0; crossover < multicomp::maxCrossovers; ++crossover)
        CHECK (valueOf (processor, MultiCompProcessor::slopeId (crossover)) == 1.0f);

    gesture (processor, "crossover", 2.0f); // Linear phase keeps the slopes
    CHECK (valueOf (processor, MultiCompProcessor::slopeId (0)) == 1.0f);

    gesture (processor, "crossover", 0.0f);
    CHECK (valueOf (processor, MultiCompProcessor::slopeId (2)) == 0.0f);

    // A deleted band's place comes back with Set all bands' value.
    processor.resetBand (5);
    CHECK (valueOf (processor, MultiCompProcessor::id (5, "look")) == Catch::Approx (7.0f));
}

// 2. Per-band lookahead -----------------------------------------------------------------------

TEST_CASE ("MultiComp W2b: each band looks its own amount ahead, and the audio waits for the largest", "[multicomp][w2b]")
{
    const auto crossover = GENERATE (0, 1);

    const auto targetsWith = [crossover] (double lowMs, double highMs)
    {
        MultiCompDsp::Targets targets;
        targets.numBands = 2;
        targets.crossover = (multicomp::CrossoverType) crossover;
        targets.bands[0].topHz = 1000.0;

        for (auto& band : targets.bands)
        {
            band.thresholdDb = -30.0;
            band.ratio = 4.0;
            band.attackMs = 1.0;
            band.releaseMs = 50.0;
        }

        targets.bands[0].lookaheadMs = lowMs;
        targets.bands[1].lookaheadMs = highMs;
        return targets;
    };

    const auto perBand = targetsWith (0.0, 5.0);
    const auto allAhead = targetsWith (5.0, 5.0);
    const auto none = targetsWith (0.0, 0.0);

    {
        MultiCompDsp dsp;
        prepare (dsp, perBand);
        CHECK (dsp.getLatencySamples() == 240); // the largest, 5 ms at 48 kHz
    }

    const auto worstAgainst = [] (const Stereo& a, const Stereo& b, int shift)
    {
        auto worst = 0.0;

        for (int i = shift; i < a.size(); ++i)
            worst = std::max (worst, std::abs ((double) a.left[(size_t) i] - (double) b.left[(size_t) (i - shift)]));

        return worst;
    };

    // Band 1 at 0 ms: its gain moves with its audio, as with no lookahead at all, 5 ms later.
    {
        const auto low = softTone (24000, 100.0, 0.5, 4800);
        const auto a = render<MultiCompDsp> (perBand, low);
        const auto c = render<MultiCompDsp> (none, low);

        INFO ("band 1: worst difference from no lookahead, 240 samples later: " << worstAgainst (a, c, 240));
        CHECK (worstAgainst (a, c, 240) < 1.0e-5);
        CHECK (levelDb (a.left, 14400, 4800) < levelDb (low.left, 14400 - 240, 4800) - 3.0); // it compresses
    }

    // Band 2 at 5 ms: its gain moves 5 ms ahead, as when every band looks 5 ms ahead, and not
    // as without lookahead.
    {
        const auto high = softTone (24000, 5000.0, 0.5, 4800);
        const auto a = render<MultiCompDsp> (perBand, high);
        const auto b = render<MultiCompDsp> (allAhead, high);
        const auto c = render<MultiCompDsp> (none, high);

        INFO ("band 2: worst difference from every band at 5 ms: " << worstAgainst (a, b, 0)
              << ", from no lookahead 240 samples later: " << worstAgainst (a, c, 240));
        CHECK (worstAgainst (a, b, 0) < 1.0e-5);
        CHECK (worstAgainst (a, c, 240) > 1.0e-3);
    }

    // At 1:1 the bands sum as they do with one lookahead for all.
    {
        auto flat = perBand, flatAll = allAhead;

        for (auto* targets : { &flat, &flatAll })
            for (auto& band : targets->bands)
                band.ratio = 1.0;

        const auto signal = programme (24000, 9);
        const auto a = render<MultiCompDsp> (flat, signal);
        const auto b = render<MultiCompDsp> (flatAll, signal);

        CHECK (a.left == b.left);
        CHECK (a.right == b.right);
    }
}

// 3. Detector EQ ----------------------------------------------------------------------------------

TEST_CASE ("MultiComp W2b: a detector high-pass at 80 Hz lets the bass, not the kick, set the low band's gain", "[multicomp][w2b]")
{
    // The model's band (docs/research/multicomp-w2-model.py, item 24), as the pre-push model
    // measures it: the reduction is each window's level at 1:1 minus its level compressed.
    const auto n = (int) (2.0 * rate);
    Stereo signal (n);
    std::vector<bool> kicks ((size_t) n), basses ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = i / rate;
        const auto beat = std::fmod (t, 0.5);
        auto x = beat >= 0.25 ? 0.25 * std::sin (2.0 * pi * 100.0 * t) : 0.0;

        for (int k = 0; k < 4; ++k)
        {
            const auto m = i - (int) (k * 0.5 * rate);

            if (m >= 0 && m < (int) (0.3 * rate))
                x += 0.8 * std::sin (2.0 * pi * 50.0 * m / rate) * std::exp (-m / (0.08 * rate));
        }

        signal.left[(size_t) i] = signal.right[(size_t) i] = (float) x;
        kicks[(size_t) i] = t >= 0.5 && beat < 0.15;
        basses[(size_t) i] = t >= 0.5 && beat >= 0.3 && beat < 0.45;
    }

    const auto targetsWith = [] (double ratio, double hpHz)
    {
        MultiCompDsp::Targets targets;
        targets.numBands = 2;
        targets.bands[0].topHz = 150.0;
        targets.bands[0].thresholdDb = -24.0;
        targets.bands[0].ratio = ratio;
        targets.bands[0].kneeDb = 0.0;
        targets.bands[0].attackMs = 5.0;
        targets.bands[0].releaseMs = 120.0;
        targets.bands[0].autoRelease = false;
        targets.bands[0].detectorHpHz = hpHz;
        targets.bands[1].ratio = 1.0;
        return targets;
    };

    const auto reference = render<MultiCompDsp> (targetsWith (1.0, MultiCompDsp::detectorOffHpHz), signal);
    const auto whole = render<MultiCompDsp> (targetsWith (4.0, MultiCompDsp::detectorOffHpHz), signal);
    const auto highPassed = render<MultiCompDsp> (targetsWith (4.0, 80.0), signal);

    const auto reduction = [&reference] (const Stereo& out, const std::vector<bool>& mask) { return blockDb (reference.left, mask) - blockDb (out.left, mask); };

    const auto kicksWhole = reduction (whole, kicks), kicksHp = reduction (highPassed, kicks);
    const auto bassWhole = reduction (whole, basses), bassHp = reduction (highPassed, basses);

    INFO ("kicks: " << kicksWhole << " dB whole band, " << kicksHp << " dB with HP 80 Hz (model 10.54, 4.65); bass: "
          << bassWhole << " dB, " << bassHp << " dB (model 4.61, 4.23)");
    CHECK (kicksHp < 0.5 * kicksWhole);
    CHECK (std::abs (kicksWhole - 10.54) < 1.0);
    CHECK (std::abs (kicksHp - 4.65) < 1.0);
    CHECK (std::abs (bassHp - bassWhole) < 3.0);

    // The audio isn't filtered: with the detector EQ on and 1:1, the output is the reference.
    auto eqOnly = targetsWith (1.0, 80.0);
    eqOnly.bands[0].detectorLpHz = 120.0;
    eqOnly.bands[0].detectorTilt = 3.0;
    CHECK (render<MultiCompDsp> (eqOnly, signal).left == reference.left);
}

TEST_CASE ("MultiComp W2b: Tilt makes the detector hear more of one side of the band's centre", "[multicomp][w2b]")
{
    // One band, 20 Hz to 20 kHz, centre 632 Hz; tones 2 octaves either side, well over the
    // threshold at 4:1. Tilt +6 dB/oct gives the detector about +12 dB at the high tone and
    // -12 dB at the low one, so the reduction changes by about 9 dB (the pre-push model).
    const auto reductionChange = [] (double hz)
    {
        const auto levelWith = [hz] (double tilt)
        {
            MultiCompDsp::Targets targets;
            targets.numBands = 1;
            targets.bands[0].thresholdDb = -40.0;
            targets.bands[0].ratio = 4.0;
            targets.bands[0].kneeDb = 0.0;
            targets.bands[0].detectorTilt = tilt;

            const auto out = render<MultiCompDsp> (targets, softTone (24000, hz, 0.5, 0));
            return levelDb (out.left, 14400, 9600);
        };

        return levelWith (0.0) - levelWith (6.0); // more reduction is positive
    };

    const auto high = reductionChange (632.46 * 4.0);
    const auto low = reductionChange (632.46 / 4.0);

    INFO ("reduction change with Tilt +6: high tone " << high << " dB, low tone " << low << " dB (model +8.99, -8.95)");
    CHECK (high > 5.0);
    CHECK (low < -5.0);
}

// 4. Per-crossover slopes -------------------------------------------------------------------------

TEST_CASE ("MultiComp W2b: mixed crossover slopes sum flat, and changing one doesn't click", "[multicomp][w2b]")
{
    MultiCompDsp::Targets targets;
    targets.numBands = 4;
    targets.bands[0].topHz = 150.0;
    targets.bands[1].topHz = 1500.0;
    targets.bands[2].topHz = 6000.0;
    targets.perCrossoverSlopes = true;
    targets.steepSlopes[0] = true;
    targets.steepSlopes[1] = false;
    targets.steepSlopes[2] = true;

    for (auto& band : targets.bands)
        band.ratio = 1.0;

    // 48, 24, 48 dB/oct: the impulse response is an all-pass.
    {
        Stereo impulse (32768);
        impulse.left[0] = impulse.right[0] = 1.0f;
        const auto out = render<MultiCompDsp> (targets, impulse);

        auto worst = 0.0;

        for (int k = 0; k <= 60; ++k)
        {
            const auto hz = 20.0 * std::pow (1000.0, k / 60.0);
            std::complex<double> sum;

            for (size_t i = 0; i < out.left.size(); ++i)
                sum += (double) out.left[i] * std::polar (1.0, -2.0 * pi * hz * (double) i / rate);

            worst = std::max (worst, std::abs (20.0 * std::log10 (std::abs (sum))));
        }

        INFO ("largest deviation from flat, 20 Hz to 20 kHz: " << worst << " dB");
        CHECK (worst < 0.01);
    }

    // A sine through a change of crossover 2's slope: the switch fades out and back in, with no
    // step larger than twice the sine's own.
    {
        const auto n = (int) rate;
        auto signal = softTone (n, 1000.0, 0.5, 0);
        const auto ownStep = 0.5 * 2.0 * pi * 1000.0 / rate;

        MultiCompDsp dsp;
        prepare (dsp, targets);
        process (dsp, signal, 0, n / 2);

        auto changed = targets;
        changed.steepSlopes[1] = true;
        dsp.setTargets (changed);
        process (dsp, signal, n / 2, n - n / 2);

        auto worst = 0.0;

        for (int i = 1; i < n; ++i)
            worst = std::max (worst, std::abs ((double) signal.left[(size_t) i] - (double) signal.left[(size_t) (i - 1)]));

        INFO ("largest step " << worst << ", the sine's own " << ownStep);
        CHECK (worst < 2.0 * ownStep);

        // And it did switch: the last 0.2 s is the 48, 48, 48 render's.
        auto steep = targets;
        steep.steepSlopes[1] = true;
        const auto reference = render<MultiCompDsp> (steep, softTone (n, 1000.0, 0.5, 0));
        auto late = 0.0;

        for (int i = n - 9600; i < n; ++i)
            late = std::max (late, std::abs ((double) signal.left[(size_t) i] - (double) reference.left[(size_t) i]));

        CHECK (late < 1.0e-3);
    }
}

// 5. GR history -----------------------------------------------------------------------------------

TEST_CASE ("MultiComp W2b: the GR history records each band's reduction, and nothing while it's off", "[multicomp][w2b]")
{
    MultiCompDsp::Targets targets;
    targets.numBands = 2;
    targets.bands[0].topHz = 1000.0;
    targets.bands[0].thresholdDb = -30.0;
    targets.bands[0].ratio = 4.0;
    targets.bands[1].ratio = 1.0;

    MultiCompDsp dsp;
    prepare (dsp, targets);

    std::vector<MultiCompDsp::HistoryColumn> columns (4096);
    auto signal = softTone ((int) rate, 100.0, 0.5, 0);

    process (dsp, signal, 0, (int) rate / 2);
    CHECK (dsp.popHistory (columns.data(), (int) columns.size()) == 0);

    dsp.setHistoryActive (true);
    process (dsp, signal, (int) rate / 2, (int) rate / 2);

    const auto count = dsp.popHistory (columns.data(), (int) columns.size());
    const auto expected = MultiCompDsp::historyColumns / MultiCompDsp::historySeconds * 0.5; // 0.5 s

    CHECK (std::abs (count - expected) <= 1.0);
    REQUIRE (count > 10);

    const auto& last = columns[(size_t) count - 1];
    INFO ("band 1's reduction in the last column: " << last.reductionDb[0] << " dB");
    CHECK (last.reductionDb[0] > 5.0);
    CHECK (last.reductionDb[1] == 0.0f);

    for (int band = 2; band < multicomp::maxBands; ++band)
        CHECK (last.reductionDb[(size_t) band] == 0.0f);
}

// 6. CPU (section 0.4 of the Phase 2.1 brief) -----------------------------------------------------

TEST_CASE ("multicomp W2b CPU measurement", "[.measure]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 7500; // 20 s
    constexpr int runs = 5;

    const auto timeOf = [] (const MultiCompDsp::Targets& targets)
    {
        std::vector<double> results;

        for (int run = 0; run < runs; ++run)
        {
            MultiCompDsp dsp;
            prepare (dsp, targets);

            const auto signal = programme (blockSize * 64, 77);
            std::vector<float> left ((size_t) blockSize), right ((size_t) blockSize);
            const auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < numBlocks; ++i)
            {
                const auto offset = (size_t) ((i % 64) * blockSize);
                std::copy_n (signal.left.begin() + (std::ptrdiff_t) offset, blockSize, left.begin());
                std::copy_n (signal.right.begin() + (std::ptrdiff_t) offset, blockSize, right.begin());
                float* channels[] { left.data(), right.data() };
                const float* key[2] {};
                dsp.process (channels, 2, key, 0, blockSize);
            }

            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
            results.push_back (100.0 * elapsed.count() / ((double) (numBlocks * blockSize) / rate));
        }

        std::sort (results.begin(), results.end());
        return results[runs / 2];
    };

    // MultiComp at its defaults: 4 bands, LR 24.
    MultiCompDsp::Targets defaults;
    defaults.bands[0].topHz = 120.0;
    defaults.bands[1].topHz = 1200.0;
    defaults.bands[2].topHz = 6000.0;

    auto everything = defaults;
    everything.perCrossoverSlopes = true;
    everything.steepSlopes.fill (true);

    for (size_t b = 0; b < 4; ++b)
    {
        everything.bands[b].lookaheadMs = 1.0 + (double) b;
        everything.bands[b].detectorHpHz = 30.0 * (double) (b + 1);
        everything.bands[b].detectorLpHz = 15000.0;
        everything.bands[b].detectorTilt = 2.0;
    }

    const auto defaultsPercent = timeOf (defaults);
    const auto everythingPercent = timeOf (everything);

    std::printf ("multicomp w2b cpu, 4 bands, 48 kHz, 128 samples, median of %d: defaults %.3f %%, with lookahead, detector EQ and 48 dB/oct slopes %.3f %% (%.2fx)\n",
                 runs, defaultsPercent, everythingPercent, everythingPercent / defaultsPercent);
}
