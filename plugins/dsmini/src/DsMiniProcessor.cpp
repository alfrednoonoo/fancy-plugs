#include "DsMiniProcessor.h"
#include "DsMiniEditor.h"

#include <fancy/plugin/DeEsserParameters.h>

// The brief's starting values (§6b.3). Captions name what each profile catches; the editor adds
// the band.
const std::array<DsMiniProcessor::Profile, 6> DsMiniProcessor::profiles { {
    { "Female voice",    false, 7000.0f, 1.0f, 0.5f, 60.0f,  "Sibilance" },
    { "Male voice",      false, 5000.0f, 1.0f, 0.5f, 60.0f,  "Sibilance" },
    { "Sibilance",       true,  5000.0f, 1.0f, 0.5f, 60.0f,  "Sibilance" },
    { "Harshness",       false, 3500.0f, 1.2f, 2.0f, 120.0f, "Harshness" },
    { "Acoustic guitar", false, 3000.0f, 1.0f, 1.0f, 80.0f,  "String squeak" },
    { "Bowed strings",   false, 4000.0f, 1.5f, 2.0f, 120.0f, "Rosin" },
} };

namespace
{
fancy::FancyProcessor::Layout createLayout()
{
    using Choice = juce::AudioParameterChoice;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;

    fancy::FancyProcessor::Layout layout;

    juce::StringArray names;

    for (const auto& profile : DsMiniProcessor::profiles)
        names.add (profile.name);

    layout.add (std::make_unique<Choice> (juce::ParameterID { "profile", 1 }, "Profile", names, 0));

    layout.add (fancy::floatParameter (
        "amount", "Amount", juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f,
        juce::AudioParameterFloatAttributes {}
            .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1); })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

    layout.add (std::make_unique<Choice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "Split", "Wide" }, 0));

    // It changes the latency, so it's a deliberate setting, not something automation flips.
    layout.add (std::make_unique<Choice> (juce::ParameterID { "lookahead", 1 }, "Lookahead", juce::StringArray { "Off", "2 ms" }, 0,
                                          ChoiceAttributes {}.withAutomatable (false)));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "link", 1 }, "Link channels", true));

    // Fancy DS's own Output, so a saved Fancy DS preset plays at the same level.
    layout.add (fancy::deesser::output());
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    constexpr float female = 0.0f, male = 1.0f, sibilance = 2.0f, harshness = 3.0f, guitar = 4.0f, strings = 5.0f, lookahead2 = 1.0f;

    std::vector<fancy::FactoryPreset> presets {
        { "Default", {} },
        { "Gentle female", { { "profile", female }, { "amount", 3.0f } } },
        { "Strong female", { { "profile", female }, { "amount", 7.0f } } },
        { "Gentle male", { { "profile", male }, { "amount", 3.0f } } },
        { "Strong male", { { "profile", male }, { "amount", 7.0f } } },
        { "Podcast", { { "profile", sibilance }, { "amount", 5.0f }, { "lookahead", lookahead2 } } },
        { "Harsh vocal", { { "profile", harshness }, { "amount", 4.0f } } },
        { "Acoustic guitar", { { "profile", guitar }, { "amount", 5.0f } } },
        { "Strings", { { "profile", strings }, { "amount", 4.0f } } },
    };

    fancy::setCategories (presets, {
        { "Gentle female", "Vocals" },
        { "Strong female", "Vocals" },
        { "Gentle male", "Vocals" },
        { "Strong male", "Vocals" },
        { "Podcast", "Vocals" },
        { "Harsh vocal", "Vocals" },
        { "Acoustic guitar", "Instruments" },
        { "Strings", "Instruments" },
    });

    return presets;
}
} // namespace

DsMiniProcessor::DsSettings DsMiniProcessor::mapProfile (int profileIndex, float amount)
{
    const auto& profile = profiles[(size_t) juce::jlimit (0, (int) profiles.size() - 1, profileIndex)];

    return { -4.0f * amount, profile.freqHz, profile.widthOctaves, -1.8f * amount, profile.attackMs, profile.releaseMs, profile.shelf };
}

DsMiniProcessor::DsMiniProcessor()
    : FancyProcessor ("Fancy DS-Mini", "FancyDSMini",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      createLayout(), createPresets()),
      dsThreshold (fancy::deesser::threshold()),
      dsFreq (fancy::deesser::freq()),
      dsWidth (fancy::deesser::width()),
      dsRange (fancy::deesser::range()),
      dsAttack (fancy::deesser::attack()),
      dsRelease (fancy::deesser::release())
{
}

DsMiniProcessor::~DsMiniProcessor() = default;

float DsMiniProcessor::read (const char* parameterId) const
{
    return const_cast<DsMiniProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool DsMiniProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

int DsMiniProcessor::getProfile() const
{
    return juce::jlimit (0, (int) profiles.size() - 1, juce::roundToInt (read ("profile")));
}

DsMiniProcessor::DeEsser::Targets DsMiniProcessor::targetsFor (int profile) const
{
    using fancy::deesser::roundAsParameter;

    const auto settings = mapProfile (profile, read ("amount"));

    // Read as Fancy DS reads its parameters: rounded floats, widened to double.
    DeEsser::Targets targets;
    targets.thresholdDb = (double) roundAsParameter (*dsThreshold, settings.threshold);
    targets.freqHz = (double) roundAsParameter (*dsFreq, settings.freq);
    targets.widthOctaves = (double) roundAsParameter (*dsWidth, settings.width);
    targets.shelf = settings.shelf;
    targets.rangeDb = (double) roundAsParameter (*dsRange, settings.range);
    targets.attackMs = (double) roundAsParameter (*dsAttack, settings.attack);
    targets.releaseMs = (double) roundAsParameter (*dsRelease, settings.release);
    targets.detector = DeEsser::mainLR;
    targets.wide = isOn ("mode");
    targets.relative = true;
    targets.listen = isListening();
    targets.link = isOn ("link");
    targets.lookahead = isOn ("lookahead") ? DeEsser::lookahead2 : DeEsser::lookaheadOff;
    targets.linearPhase = false;
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.bypass = isOn ("bypass");

    return targets;
}

void DsMiniProcessor::prepareAudio (double sampleRate, int)
{
    const auto profile = getProfile();

    for (auto& engine : engines)
    {
        engine.setTargets (targetsFor (profile));
        engine.setSynchronousDesign (true);
        engine.prepare (sampleRate);
    }

    engineProfiles = { profile, profile };
    active.store (0, std::memory_order_release);
    fadeLeft = 0;
    fadeLength = juce::jmax (1, (int) std::lround (fadeMs * 0.001 * sampleRate));
    scratch.setSize (2, scratchSize);

    setLatencySamples (engines[0].getLatencySamples());
}

void DsMiniProcessor::reset()
{
    for (auto& engine : engines)
        engine.reset();

    fadeLeft = 0;
}

bool DsMiniProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void DsMiniProcessor::setHistoryColumns (int columns) noexcept
{
    for (auto& engine : engines)
        engine.setHistoryColumns (columns);
}

float DsMiniProcessor::takeReduction() noexcept
{
    return juce::jmax (engines[0].takeReduction(), engines[1].takeReduction());
}

void DsMiniProcessor::startFade (int profile)
{
    // The idle engine picks up where the playing one is, on the new profile, and takes over
    // during the next 50 ms.
    const auto playing = active.load (std::memory_order_relaxed);
    const auto next = 1 - playing;
    auto& engine = engines[(size_t) next];

    engine.continueFrom (engines[(size_t) playing]);
    engine.snapToTargets (targetsFor (profile));

    engineProfiles[(size_t) next] = profile;
    active.store (next, std::memory_order_release);
    fadeLeft = fadeLength;
}

void DsMiniProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = mainBuffer.getNumSamples();

    if (numChannels == 0)
        return;

    auto* const* main = mainBuffer.getArrayOfWritePointers();
    const float* noSidechain[2] { nullptr, nullptr };

    inputMeter.push (main, numChannels, numSamples);

    const auto profile = getProfile();
    const auto current = active.load (std::memory_order_relaxed);

    // One change at a time: a profile picked during a fade starts when it's done.
    if (fadeLeft == 0 && profile != engineProfiles[(size_t) current])
        startFade (profile);

    const auto now = active.load (std::memory_order_relaxed);
    auto& engine = engines[(size_t) now];
    engine.setTargets (targetsFor (engineProfiles[(size_t) now]));

    if (fadeLeft == 0)
    {
        engine.process (main, numChannels, noSidechain, 0, numSamples);
    }
    else
    {
        auto& previous = engines[(size_t) (1 - now)];
        previous.setTargets (targetsFor (engineProfiles[(size_t) (1 - now)]));

        for (int start = 0; start < numSamples; start += scratchSize)
        {
            const auto count = juce::jmin (scratchSize, numSamples - start);
            float* newPieces[2] { main[0] + start, numChannels > 1 ? main[1] + start : nullptr };
            float* oldPieces[2] { scratch.getWritePointer (0), numChannels > 1 ? scratch.getWritePointer (1) : nullptr };

            for (int c = 0; c < numChannels; ++c)
                std::copy (newPieces[c], newPieces[c] + count, oldPieces[c]);

            engine.process (newPieces, numChannels, noSidechain, 0, count);
            previous.process (oldPieces, numChannels, noSidechain, 0, count);

            // Equal gain: both engines carry the same input.
            for (int i = 0; i < count && fadeLeft > 0; ++i, --fadeLeft)
            {
                const auto in = 1.0f - (float) fadeLeft / (float) fadeLength;

                for (int c = 0; c < numChannels; ++c)
                    newPieces[c][i] = oldPieces[c][i] + (newPieces[c][i] - oldPieces[c][i]) * in;
            }

            if (fadeLeft == 0)
            {
                // The rest of this block is the new engine's alone.
                for (int rest = start + count; rest < numSamples; rest += scratchSize)
                {
                    const auto restCount = juce::jmin (scratchSize, numSamples - rest);
                    float* restPieces[2] { main[0] + rest, numChannels > 1 ? main[1] + rest : nullptr };
                    engine.process (restPieces, numChannels, noSidechain, 0, restCount);
                }

                break;
            }
        }
    }

    outputMeter.push (main, numChannels, numSamples);

    if (getLatencySamples() != engine.getLatencySamples())
        setLatencySamples (engine.getLatencySamples());
}

juce::ValueTree DsMiniProcessor::createDsPresetState() const
{
    const auto settings = mapProfile (getProfile(), read ("amount"));

    // Fancy DS's state type and stateVersion; Fancy DS fills anything missing with its default.
    juce::ValueTree state ("FancyDS");
    state.setProperty ("stateVersion", FancyProcessor::stateVersion, nullptr);

    const auto add = [&state] (const char* id, float value)
    {
        juce::ValueTree parameter ("PARAM");
        parameter.setProperty ("id", id, nullptr);
        parameter.setProperty ("value", value, nullptr);
        state.appendChild (parameter, nullptr);
    };

    // The values Fancy DS-Mini maps to, before rounding: Fancy DS rounds them the same way.
    add ("threshold", settings.threshold);
    add ("freq", settings.freq);
    add ("width", settings.width);
    add ("shape", settings.shelf ? 1.0f : 0.0f);
    add ("range", settings.range);
    add ("attack", settings.attack);
    add ("release", settings.release);
    add ("detector", 0.0f);
    add ("mode", isOn ("mode") ? 1.0f : 0.0f);
    add ("relative", 1.0f);
    add ("link", isOn ("link") ? 1.0f : 0.0f);
    add ("lookahead", isOn ("lookahead") ? (float) DeEsser::lookahead2 : (float) DeEsser::lookaheadOff);
    add ("splitPhase", 0.0f);
    add ("output", read ("output"));
    add ("bypass", 0.0f);

    return state;
}

juce::File DsMiniProcessor::getDsPresetFile (const juce::String& presetName) const
{
    // Fancy DS's preset folder sits beside this plugin's: <root>/Fancy DS/Presets.
    const auto root = getPresets().getUserDirectory().getParentDirectory().getParentDirectory();

    return root.getChildFile ("Fancy DS").getChildFile ("Presets").getChildFile (dsPresetFolder)
        .getChildFile (juce::File::createLegalFileName (presetName.trim()) + ".fancypreset");
}

bool DsMiniProcessor::saveAsDsPreset (const juce::String& presetName)
{
    if (juce::File::createLegalFileName (presetName.trim()).isEmpty())
        return false;

    const auto file = getDsPresetFile (presetName);

    if (! file.getParentDirectory().createDirectory())
        return false;

    const auto xml = createDsPresetState().createXml();
    return xml != nullptr && xml->writeTo (file);
}

void DsMiniProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

juce::AudioProcessorEditor* DsMiniProcessor::createEditor()
{
    return new DsMiniEditor (*this);
}
