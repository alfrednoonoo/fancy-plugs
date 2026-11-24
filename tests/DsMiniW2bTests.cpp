// Phase 4 Wave 2 for Fancy DS-Mini, item 34: the Hiss profile (docs/ds.md, "Phase 4 Wave 2: Mid/Side
// reduction and Hiss mode", sections 3 and 5; docs/ds-mini.md, "Phase 4 Wave 2").

#include "DeEsserSignals.h"
#include "DsMiniProcessor.h"
#include "LegacyDeEsserW2.h"

#if FANCY_HAS_DS
 #include "DsProcessor.h"
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>

using namespace deesser_test;

namespace
{
Stereo render (fancy::FancyProcessor& processor, const Stereo& input, int blockSize = 256)
{
    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, blockSize);

    const auto channels = juce::jmax (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, blockSize);
    juce::MidiBuffer midi;
    auto output = input;

    for (int start = 0; start < input.size(); start += blockSize)
    {
        const auto count = std::min (blockSize, input.size() - start);
        buffer.setSize (channels, count, false, false, true);
        buffer.clear();
        buffer.copyFrom (0, 0, input.left.data() + start, count);
        buffer.copyFrom (1, 0, input.right.data() + start, count);
        processor.processBlock (buffer, midi);
        midi.clear();
        std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + count, output.left.data() + start);
        std::copy (buffer.getReadPointer (1), buffer.getReadPointer (1) + count, output.right.data() + start);
    }

    processor.releaseResources();
    return output;
}

template <typename E, typename Targets>
Stereo runEngine (const Targets& targets, Stereo signal)
{
    E engine;
    engine.prepare (rate);
    engine.setSynchronousDesign (true);
    engine.snapToTargets (targets);

    for (int start = 0; start < signal.size(); start += 256)
    {
        float* channels[] { signal.left.data() + start, signal.right.data() + start };
        const float* sidechain[] { nullptr, nullptr };
        engine.process (channels, 2, sidechain, 0, std::min (256, signal.size() - start));
    }

    return signal;
}

fancy::dsp::LegacyDeEsserW2::Targets toLegacy (const fancy::dsp::DeEsser::Targets& t)
{
    fancy::dsp::LegacyDeEsserW2::Targets old;
    old.thresholdDb = t.thresholdDb;
    old.freqHz = t.freqHz;
    old.widthOctaves = t.widthOctaves;
    old.shelf = t.shelf;
    old.rangeDb = t.rangeDb;
    old.attackMs = t.attackMs;
    old.releaseMs = t.releaseMs;
    old.detector = t.detector;
    old.wide = t.wide;
    old.relative = t.relative;
    old.listen = t.listen;
    old.link = t.link;
    old.lookahead = t.lookahead;
    old.linearPhase = t.linearPhase;
    old.outputGain = t.outputGain;
    old.bypass = t.bypass;
    old.slopeStages = t.slopeStages;
    old.dual = t.dual;
    return old;
}

bool identical (const Stereo& a, const Stereo& b)
{
    return a.left == b.left && a.right == b.right;
}
} // namespace

TEST_CASE ("DS-Mini's six earlier profiles render exactly as on the v1.7.0 engine", "[dsmini][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto profile = GENERATE (0, 1, 2, 3, 4, 5);
    const auto amount = GENERATE (2.0f, 5.0f, 8.0f);
    const Voice voice (-18.0, -6.0, 2.0);

    DsMiniProcessor mini;
    mini.setParameter ("profile", (float) profile);
    mini.setParameter ("amount", amount);

    const auto targets = mini.targetsFor (profile);
    CHECK (! targets.hiss);

    INFO (DsMiniProcessor::profiles[(size_t) profile].name << ", Amount " << amount);
    CHECK (identical (runEngine<fancy::dsp::DeEsser> (targets, voice.audio),
                      runEngine<fancy::dsp::LegacyDeEsserW2> (toLegacy (targets), voice.audio)));
}

TEST_CASE ("DS-Mini's Hiss profile maps Amount onto Fancy DS's Hiss mode", "[dsmini][w2b]")
{
    const auto settings = DsMiniProcessor::mapProfile (DsMiniProcessor::hissProfile, 5.0f);

    CHECK (juce::String (DsMiniProcessor::profiles[(size_t) DsMiniProcessor::hissProfile].name) == "Hiss");
    CHECK (settings.hiss);
    CHECK (settings.shelf);
    CHECK (settings.freq == 5000.0f);
    CHECK (settings.threshold == -36.0f);
    CHECK (settings.range == -9.0f);
}

TEST_CASE ("DS-Mini's Hiss profile at Amount 0 gives the input, bit for bit", "[dsmini][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const Voice voice (-12.0, 0.0, 2.0);

    DsMiniProcessor mini;
    mini.setParameter ("profile", (float) DsMiniProcessor::hissProfile);
    mini.setParameter ("amount", 0.0f);

    CHECK (identical (render (mini, voice.audio), voice.audio));
}

#if FANCY_HAS_DS
TEST_CASE ("DS-Mini's Hiss profile saved as a Fancy DS preset loads as Hiss mode and sounds the same", "[dsmini][w2b]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fancy-dsmini-hiss-presets");
    root.deleteRecursively();
    fancy::PresetManager::setUserRootForTests (root);

    const auto wide = GENERATE (false, true);
    const Voice voice (-30.0, -6.0, 2.0);

    DsMiniProcessor mini;
    mini.setParameter ("profile", (float) DsMiniProcessor::hissProfile);
    mini.setParameter ("amount", 6.3f);
    mini.setParameter ("mode", wide ? 1.0f : 0.0f);
    REQUIRE (mini.saveAsDsPreset ("Hiss from the Mini"));

    DsProcessor ds;
    const auto entries = ds.getPresets().getEntries();
    const auto entry = std::find_if (entries.begin(), entries.end(), [] (const auto& e) { return e.isUser && e.name == "Hiss from the Mini"; });
    REQUIRE (entry != entries.end());
    ds.getPresets().load (*entry);

    CHECK (ds.readTargets().hiss);
    CHECK (ds.readTargets().shelf);

    const auto a = render (mini, voice.audio);
    const auto b = render (ds, voice.audio);
    auto worst = 0.0;

    for (int i = 0; i < a.size(); ++i)
        worst = std::max ({ worst, std::abs ((double) a.left[(size_t) i] - b.left[(size_t) i]), std::abs ((double) a.right[(size_t) i] - b.right[(size_t) i]) });

    INFO ((wide ? "Wide" : "Split") << ", worst difference " << 20.0 * std::log10 (worst + 1.0e-30) << " dB");
    CHECK (20.0 * std::log10 (worst + 1.0e-30) < -100.0);

    fancy::PresetManager::setUserRootForTests ({});
    root.deleteRecursively();
}
#endif
