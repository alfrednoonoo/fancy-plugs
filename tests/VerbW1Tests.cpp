// Phase 4 Wave 1 for Fancy Verb (docs/verb.md, "Phase 4 Wave 1", section 4): Equal loudness,
// the EQ module, Export IR, drag and drop, and library favourites.

#include "IrBuilder.h"
#include "IrFiles.h"
#include "LibraryPicker.h"
#include "VerbChain.h"
#include "VerbEditor.h"
#include "VerbLibrary.h"
#include "VerbProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <numbers>
#include <random>
#include <vector>

namespace
{
constexpr double rate = 48000.0;

using verb::ModuleSpec;
using verb::ModuleType;

ModuleSpec moduleOf (ModuleType type)
{
    ModuleSpec module;
    module.type = type;
    module.seed = 11;

    const auto& settings = verb::settingsOf (type);

    for (size_t i = 0; i < settings.size(); ++i)
        module.values[i] = settings[i].defaultValue;

    return module;
}

ModuleSpec libraryModule (const juce::String& key, int gainMode)
{
    auto module = moduleOf (ModuleType::file);
    module.factory = key;
    module.mono = false;
    module.gainMode = gainMode;
    return module;
}

bool sameAudio (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return false;

    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        if (! std::equal (a.getReadPointer (ch), a.getReadPointer (ch) + a.getNumSamples(), b.getReadPointer (ch)))
            return false;

    return true;
}

// What Equal loudness measures: each output's pink-weighted energy, averaged.
double loudnessOf (const juce::AudioBuffer<float>& ir)
{
    const auto channel = [&ir] (int ch)
    {
        std::vector<double> samples ((size_t) ir.getNumSamples());

        for (int i = 0; i < ir.getNumSamples(); ++i)
            samples[(size_t) i] = ir.getSample (ch, i);

        return verb::pinkEnergy (samples, rate);
    };

    auto total = 0.0;

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        total += channel (ch);

    return 10.0 * std::log10 (total * 0.5);
}

double plainEnergyDb (const juce::AudioBuffer<float>& ir)
{
    auto total = 0.0;

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        for (int i = 0; i < ir.getNumSamples(); ++i)
            total += (double) ir.getSample (ch, i) * ir.getSample (ch, i);

    return 10.0 * std::log10 (total * 0.5);
}

verb::BuiltIr build (verb::ChainSpec chain)
{
    verb::IrResolver resolver;
    resolver.resolve (chain, juce::File());
    return verb::buildIr (chain, rate);
}

double magnitudeDbAt (const juce::AudioBuffer<float>& ir, int channel, double hz)
{
    std::complex<double> sum;

    for (int i = 0; i < ir.getNumSamples(); ++i)
        sum += (double) ir.getSample (channel, i) * std::polar (1.0, -2.0 * std::numbers::pi * hz * i / rate);

    return 20.0 * std::log10 (std::abs (sum));
}

juce::File tempFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fancy-verb-w1");
    folder.createDirectory();
    return folder;
}

juce::File writeNoiseWav (const juce::String& name, int channels)
{
    const auto file = tempFolder().getChildFile (name);
    file.deleteFile();

    juce::AudioBuffer<float> audio (channels, 24000);
    std::mt19937 generator (5);
    std::uniform_real_distribution<float> distribution (-0.5f, 0.5f);

    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample (ch, i, distribution (generator) * std::exp (-(float) i / 6000.0f));

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    juce::WavAudioFormat format;

    if (const auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                               .withSampleRate (rate)
                                                               .withNumChannels (channels)
                                                               .withBitsPerSample (24)))
        writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());

    return file;
}

void prepareOffline (VerbProcessor& processor)
{
    processor.setNonRealtime (true);
    processor.prepareToPlay (rate, 512);
    REQUIRE (processor.waitForBuild (20000));
}
} // namespace

// 1. Legacy ----------------------------------------------------------------------------------

TEST_CASE ("Verb W1: every factory preset builds the same with Equal loudness off and a flat EQ module", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    auto& presets = processor.getPresets();
    const auto names = presets.getNames();

    for (int index = 0; index < names.size(); ++index)
    {
        INFO (names[index]);
        presets.load (index);

        // A saved project from before Wave 1 has no eqLoud property, and reads as off.
        CHECK_FALSE (processor.getChain().hasProperty (verb::ids::equalLoudness));

        auto chain = verb::readChain (processor.getChain());
        CHECK_FALSE (chain.equalLoudness);

        const auto plain = build (chain);
        CHECK (plain.preview.loudnessTrimDb == 0.0);

        chain.modules.push_back (moduleOf (ModuleType::eq));
        const auto flat = build (chain);

        REQUIRE (sameAudio (plain.audio, flat.audio));
        REQUIRE (sameAudio (plain.tailA, flat.tailA));
    }

    // Switching it on and off again leaves the state as it was.
    processor.setEqualLoudness (true);
    CHECK (processor.getChain().hasProperty (verb::ids::equalLoudness));
    processor.setEqualLoudness (false);
    CHECK_FALSE (processor.getChain().hasProperty (verb::ids::equalLoudness));
}

// 2. Equal loudness ---------------------------------------------------------------------------

TEST_CASE ("Verb W1: Equal loudness brings two library IRs 10 dB apart to within 1 dB", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    verb::ChainSpec loud, quiet;
    loud.modules.push_back (libraryModule ("Halls/Concert Hall", 0));  // 0 dB
    quiet.modules.push_back (libraryModule ("Rooms/Booth", 2));         // -12 dB

    const auto loudOff = build (loud), quietOff = build (quiet);
    const auto apart = loudnessOf (loudOff.audio) - loudnessOf (quietOff.audio);
    INFO ("off: " << apart << " dB apart");
    REQUIRE (std::abs (apart) > 8.0);

    loud.equalLoudness = quiet.equalLoudness = true;
    const auto loudOn = build (loud), quietOn = build (quiet);

    const auto after = loudnessOf (loudOn.audio) - loudnessOf (quietOn.audio);
    INFO ("on: " << after << " dB apart, trims " << loudOn.preview.loudnessTrimDb << " and " << quietOn.preview.loudnessTrimDb);
    CHECK (std::abs (after) < 1.0);

    // The trim shown is the gain applied.
    INFO ("plain energy apart: " << plainEnergyDb (loudOn.audio) - plainEnergyDb (quietOn.audio) << " dB");
    CHECK (loudnessOf (loudOn.audio) - loudnessOf (loudOff.audio) == Catch::Approx (loudOn.preview.loudnessTrimDb).margin (0.05));

    // A reverb generator at its defaults, which is what the reference was chosen to sit near.
    verb::ChainSpec generator;
    generator.modules.push_back (moduleOf (ModuleType::reverb));
    generator.equalLoudness = true;
    CHECK (std::abs (build (generator).preview.loudnessTrimDb) < verb::maxLoudnessTrimDb);
}

TEST_CASE ("Verb W1: Equal loudness in the processor is undoable chain state and shows its trim", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    const auto uid = processor.addModule (ModuleType::file, 0);
    REQUIRE (processor.loadLibrary (uid, "Rooms/Booth"));
    prepareOffline (processor);
    CHECK (processor.getPreview().loudnessTrimDb == 0.0);

    processor.setEqualLoudness (true);
    REQUIRE (processor.waitForBuild (20000));
    CHECK (processor.getPreview().loudnessTrimDb != 0.0);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    VerbProcessor copy;
    copy.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (copy.getEqualLoudness());
}

// 3. The EQ module ----------------------------------------------------------------------------

TEST_CASE ("Verb W1: the EQ module's bell raises the IR by its gain, and 0 dB changes nothing", "[verb][w1]")
{
    auto impulse = std::make_shared<verb::IrAudio>();
    impulse->sampleRate = rate;
    impulse->audio.setSize (1, 12000);
    impulse->audio.clear();
    impulse->audio.setSample (0, 100, 1.0f);

    verb::ChainSpec chain;
    auto file = moduleOf (ModuleType::file);
    file.file = impulse;
    file.gainMode = 0;
    chain.modules.push_back (file);

    const auto plain = verb::buildIr (chain, rate);

    chain.modules.push_back (moduleOf (ModuleType::eq));
    const auto flat = verb::buildIr (chain, rate);
    REQUIRE (sameAudio (plain.audio, flat.audio));

    auto& eq = chain.modules.back();
    eq.values[3] = 6.0f; // bell gain, at 2 kHz and Q 1 by default
    const auto boosted = verb::buildIr (chain, rate);

    const auto rise = magnitudeDbAt (boosted.audio, 0, 2000.0) - magnitudeDbAt (plain.audio, 0, 2000.0);
    CHECK (rise == Catch::Approx (6.0).margin (0.3));

    // Far from the bell, nearly nothing.
    CHECK (std::abs (magnitudeDbAt (boosted.audio, 0, 60.0) - magnitudeDbAt (plain.audio, 0, 60.0)) < 0.3);

    // The shelves.
    eq.values[3] = 0.0f;
    eq.values[1] = -6.0f; // low shelf at 120 Hz
    eq.values[6] = 4.0f;  // high shelf at 8 kHz
    const auto shelved = verb::buildIr (chain, rate);
    CHECK (magnitudeDbAt (shelved.audio, 0, 30.0) - magnitudeDbAt (plain.audio, 0, 30.0) == Catch::Approx (-6.0).margin (0.4));
    CHECK (magnitudeDbAt (shelved.audio, 0, 18000.0) - magnitudeDbAt (plain.audio, 0, 18000.0) == Catch::Approx (4.0).margin (0.4));
}

TEST_CASE ("Verb W1: an EQ module is stored, restored and named", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    const auto uid = processor.addModule (ModuleType::eq, 0);
    REQUIRE (uid > 0);
    processor.setModuleValue (uid, "midGain", 4.5f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    VerbProcessor copy;
    copy.setStateInformation (state.getData(), (int) state.getSize());
    const auto module = copy.getChain().getChildWithProperty (verb::ids::uid, uid);
    REQUIRE (module.isValid());
    CHECK (module[verb::ids::type].toString() == "eq");
    CHECK ((float) module["midGain"] == 4.5f);
    CHECK (verb::titleOf (ModuleType::eq) == "EQ");
    CHECK (verb::typeFromStoredName ("eq") == ModuleType::eq);
}

// 4. Export -----------------------------------------------------------------------------------

TEST_CASE ("Verb W1: Export IR writes the finished IR as a 24-bit WAV", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    for (const auto trueStereo : { false, true })
    {
        INFO ("true stereo " << trueStereo);

        VerbProcessor processor;
        const auto uid = processor.addModule (ModuleType::reverb, 0);
        processor.setModuleValue (uid, "length", 0.8f);
        processor.setModuleValue (uid, "trueStereo", trueStereo ? 1.0f : 0.0f);

        // Well under full scale, so the file holds every sample unclipped.
        const auto normalize = processor.addModule (ModuleType::normalize, 1);
        processor.setModuleValue (normalize, "peak", -6.0f);
        processor.setTrim (0.05, 0.6);
        prepareOffline (processor);

        const auto file = tempFolder().getChildFile ("export.wav");
        REQUIRE (processor.exportIr (file));

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        REQUIRE (reader != nullptr);
        CHECK (reader->sampleRate == rate);
        CHECK (reader->bitsPerSample == 24);

        const auto expected = verb::exportableIr (build (verb::readChain (processor.getChain())));
        REQUIRE ((int) reader->numChannels == expected.getNumChannels());
        CHECK ((int) reader->numChannels == (trueStereo ? 4 : 2));
        REQUIRE ((int) reader->lengthInSamples == expected.getNumSamples());

        juce::AudioBuffer<float> written ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&written, 0, written.getNumSamples(), 0, true, true);

        auto worst = 0.0f;

        for (int ch = 0; ch < written.getNumChannels(); ++ch)
            for (int i = 0; i < written.getNumSamples(); ++i)
                worst = std::max (worst, std::abs (written.getSample (ch, i) - expected.getSample (ch, i)));

        CHECK (worst <= 1.0f / 8388608.0f * 1.01f);
    }
}

// 5. Drop -------------------------------------------------------------------------------------

TEST_CASE ("Verb W1: a dropped WAV adds or replaces a File module, and other files are refused", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    processor.addModule (ModuleType::lpHp, 0);

    const auto wav = writeNoiseWav ("dropped.wav", 2);
    REQUIRE (processor.dropFile (wav));
    REQUIRE (processor.getChain().getNumChildren() == 2);

    auto added = processor.getChain().getChild (1);
    CHECK (added[verb::ids::type].toString() == "file");
    CHECK (juce::File (added[verb::ids::path].toString()) == wav);

    // On a File module, the file is replaced.
    const auto other = writeNoiseWav ("replacement.wav", 2);
    REQUIRE (processor.dropFile (other, (int) added[verb::ids::uid]));
    CHECK (processor.getChain().getNumChildren() == 2);
    CHECK (juce::File (processor.getChain().getChild (1)[verb::ids::path].toString()) == other);

    const auto text = tempFolder().getChildFile ("notes.txt");
    text.replaceWithText ("not audio");
    CHECK_FALSE (processor.dropFile (text));
    CHECK (processor.getChain().getNumChildren() == 2);
}

// 6. Favourites and the picker -----------------------------------------------------------------

TEST_CASE ("Verb W1: favourites survive a restart and stay out of the saved state", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::String key = "Plates/Bright Plate";
    const auto* entry = verb::Library::builtIn().find (key);
    const auto chosen = entry != nullptr ? key : verb::Library::builtIn().getEntries().front().key();

    {
        VerbProcessor processor;
        processor.setFavourite (chosen, true);
        CHECK (processor.isFavourite (chosen));

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        CHECK_FALSE (state.toString().contains ("favourites"));
    }

    {
        VerbProcessor restarted;
        CHECK (restarted.isFavourite (chosen));
        restarted.setFavourite (chosen, false);
    }

    VerbProcessor again;
    CHECK_FALSE (again.isFavourite (chosen));
}

TEST_CASE ("Verb W1: the library picker filters as you type and lists favourites first", "[verb][w1]")
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    VerbProcessor processor;
    const std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditorAndMakeActive());
    auto* editor = dynamic_cast<VerbEditor*> (base.get());
    REQUIRE (editor != nullptr);

    editor->showPreview (VerbEditor::Preview::picker);

    auto* picker = LibraryPicker::findOpen (editor->getPopupParent());
    REQUIRE (picker != nullptr);

    const auto rows = picker->getRows();
    REQUIRE (rows.size() >= 3);
    CHECK (rows[0].kind == LibraryPicker::Row::Kind::section);
    CHECK (rows[0].text == "Favourites");
    CHECK (rows[1].key == "Halls/Concert Hall");

    for (const auto& row : rows)
        if (row.kind == LibraryPicker::Row::Kind::library)
            CHECK (row.key.containsIgnoreCase ("hall"));

    picker->setSearch ("no such reverb anywhere");
    CHECK (picker->getRows().empty());

    picker->setSearch ({});
    CHECK (picker->getRows().size() > rows.size());

    processor.setFavourite ("Halls/Concert Hall", false);
    editor->closePopups();
    processor.editorBeingDeleted (editor);
}
