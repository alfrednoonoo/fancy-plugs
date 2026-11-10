// Fancy DS-Mini (docs/ds-mini.md): the §6b.4 tests of docs/BUILD_PROMPT_PHASE3_NEW_PLUGINS.md.

#include "DeEsserSignals.h"
#include "DsMiniProcessor.h"

#if FANCY_HAS_DS
 #include "DsProcessor.h"
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>

using namespace deesser_test;

namespace
{
// Renders through the processor, as a host would, in blocks of `blockSize`. `beforeBlock` runs
// before each block with the block's first sample.
Stereo render (fancy::FancyProcessor& processor, const Stereo& input, int blockSize = 256,
               const std::function<void (int)>& beforeBlock = {})
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

        if (beforeBlock)
            beforeBlock (start);

        processor.processBlock (buffer, midi);
        midi.clear();

        std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + count, output.left.data() + start);
        std::copy (buffer.getReadPointer (1), buffer.getReadPointer (1) + count, output.right.data() + start);
    }

    processor.releaseResources();
    return output;
}

bool identical (const Stereo& a, const Stereo& b)
{
    return a.left == b.left && a.right == b.right;
}

float amountOf (DsMiniProcessor& mini)
{
    return mini.getApvts().getRawParameterValue ("amount")->load();
}

#if FANCY_HAS_DS
// Fancy DS set to what DS-Mini maps a profile and Amount to, as a user would set it.
void setUpDs (DsProcessor& ds, int profile, float amount)
{
    const auto settings = DsMiniProcessor::mapProfile (profile, amount);

    ds.setParameter ("threshold", settings.threshold);
    ds.setParameter ("freq", settings.freq);
    ds.setParameter ("width", settings.width);
    ds.setParameter ("shape", settings.shelf ? 1.0f : 0.0f);
    ds.setParameter ("range", settings.range);
    ds.setParameter ("attack", settings.attack);
    ds.setParameter ("release", settings.release);
}
#endif
} // namespace

TEST_CASE ("DS-Mini: Amount 0 gives the input, bit for bit, in every profile", "[dsmini]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto profile = GENERATE (0, 1, 2, 3, 4, 5);
    const Voice voice (-12.0, 0.0, 2.0);

    DsMiniProcessor mini;
    mini.setParameter ("profile", (float) profile);
    mini.setParameter ("amount", 0.0f);

    INFO (DsMiniProcessor::profiles[(size_t) profile].name);
    CHECK (identical (render (mini, voice.audio), voice.audio));
}

#if FANCY_HAS_DS
TEST_CASE ("DS-Mini: each profile sounds exactly like Fancy DS at the mapped settings", "[dsmini]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto profile = GENERATE (0, 1, 2, 3, 4, 5);
    const auto amount = GENERATE (2.0f, 5.0f, 8.0f);
    const Voice voice (-18.0, -6.0, 2.0);

    DsMiniProcessor mini;
    mini.setParameter ("profile", (float) profile);
    mini.setParameter ("amount", amount);

    DsProcessor ds;
    setUpDs (ds, profile, amountOf (mini));

    INFO (DsMiniProcessor::profiles[(size_t) profile].name << ", Amount " << amount);
    CHECK (identical (render (mini, voice.audio), render (ds, voice.audio)));
}
#endif

TEST_CASE ("DS-Mini: the default cuts the test vocal's bursts by 6 dB or more and its vowels by under 0.5 dB", "[dsmini]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const Voice voice (-18.0);

    DsMiniProcessor mini;
    const auto output = render (mini, voice.audio);

    const auto bursts = changeDb (voice.audio, output, voice.bursts);
    const auto vowels = changeDb (voice.audio, output, voice.vowels);

    std::printf ("dsmini default (Female voice, Amount 5): bursts %.2f dB, vowels %.2f dB\n", bursts, vowels);
    CHECK (bursts <= -6.0);
    CHECK (std::abs (vowels) < 0.5);
}

TEST_CASE ("DS-Mini: switching profile every 100 ms never steps further than the input does", "[dsmini]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // Full level: the vowels peak near 0 dBFS, and the bursts with them.
    const Voice voice (-6.0, 0.0, 3.0);
    constexpr int switchEvery = (int) (0.1 * rate);

    DsMiniProcessor mini;
    mini.setParameter ("amount", 8.0f);
    auto lastSwitch = 0;

    const auto output = render (mini, voice.audio, 64, [&] (int start)
    {
        if (start / switchEvery != lastSwitch)
        {
            lastSwitch = start / switchEvery;
            mini.setParameter ("profile", (float) (lastSwitch % (int) DsMiniProcessor::profiles.size()));
        }
    });

    const auto largestStep = [] (const std::vector<float>& x)
    {
        auto largest = 0.0f;

        for (size_t i = 1; i < x.size(); ++i)
            largest = std::max (largest, std::abs (x[i] - x[i - 1]));

        return largest;
    };

    const auto in = std::max (largestStep (voice.audio.left), largestStep (voice.audio.right));
    const auto out = std::max (largestStep (output.left), largestStep (output.right));

    std::printf ("dsmini profile switching: largest step %.4f, input's %.4f (limit 1.5 x)\n", out, in);
    CHECK (out <= 1.5f * in);
}

#if FANCY_HAS_DS
TEST_CASE ("DS-Mini: a saved Fancy DS preset sounds exactly like DS-Mini in Fancy DS", "[dsmini]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fancy-dsmini-presets");
    root.deleteRecursively();
    fancy::PresetManager::setUserRootForTests (root);

    const auto profile = GENERATE (1, 2, 5);
    const auto wide = GENERATE (false, true);
    const Voice voice (-18.0, -6.0, 2.0);

    DsMiniProcessor mini;
    mini.setParameter ("profile", (float) profile);
    mini.setParameter ("amount", 6.3f);
    mini.setParameter ("mode", wide ? 1.0f : 0.0f);
    mini.setParameter ("output", -2.5f);
    REQUIRE (mini.saveAsDsPreset ("From the Mini"));

    DsProcessor ds;
    const auto entries = ds.getPresets().getEntries();
    const auto entry = std::find_if (entries.begin(), entries.end(), [] (const auto& e) { return e.isUser && e.name == "From the Mini"; });
    REQUIRE (entry != entries.end());
    CHECK (entry->category == DsMiniProcessor::dsPresetFolder);
    ds.getPresets().load (*entry);

    INFO (DsMiniProcessor::profiles[(size_t) profile].name << (wide ? ", Wide" : ", Split"));
    CHECK (identical (render (mini, voice.audio), render (ds, voice.audio)));

    fancy::PresetManager::setUserRootForTests ({});
    root.deleteRecursively();
}

TEST_CASE ("dsmini CPU measurement", "[.measure]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    constexpr int block = 128, blocks = 3000;

    // Noise at -20 dBFS, so the de-esser works.
    Stereo noise (block * blocks);
    std::mt19937 random (8);
    std::uniform_real_distribution<float> uniform (-0.1f, 0.1f);

    for (int i = 0; i < noise.size(); ++i)
        noise.left[(size_t) i] = noise.right[(size_t) i] = uniform (random);

    const auto measure = [&] (fancy::FancyProcessor& processor)
    {
        processor.prepareToPlay (rate, block);
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        std::chrono::duration<double> spent {};

        for (int b = 0; b < blocks; ++b)
        {
            buffer.copyFrom (0, 0, noise.left.data() + b * block, block);
            buffer.copyFrom (1, 0, noise.right.data() + b * block, block);

            const auto start = std::chrono::steady_clock::now();
            processor.processBlock (buffer, midi);
            spent += std::chrono::steady_clock::now() - start;
        }

        processor.releaseResources();
        return 100.0 * spent.count() / ((double) blocks * block / rate);
    };

    // Taken in turns, so both see the same machine.
    std::vector<double> miniRuns, dsRuns;

    for (int run = 0; run < 5; ++run)
    {
        DsMiniProcessor mini;
        miniRuns.push_back (measure (mini));

        DsProcessor ds;
        setUpDs (ds, 0, 5.0f);
        dsRuns.push_back (measure (ds));
    }

    std::sort (miniRuns.begin(), miniRuns.end());
    std::sort (dsRuns.begin(), dsRuns.end());
    const auto mini = miniRuns[2], ds = dsRuns[2];

    std::printf ("dsmini cpu (median of 5, in turns with Fancy DS): DS-Mini default %.3f %%, Fancy DS at the same settings %.3f %%, "
                 "ratio %.3f (limit 1.10)\n", mini, ds, mini / ds);
    CHECK (mini / ds < 1.10);
}
#endif
