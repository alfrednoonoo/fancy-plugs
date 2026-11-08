#include "MultiCompProcessor.h"
#include "MultiCompEditor.h"

#include <fancy/plugin/ParamHelpers.h>

#include <cmath>

using multicomp::maxBands;

namespace
{
constexpr float mixFloorDb = -60.0f;
constexpr int designIntervalMs = 30;

// The parameters Link bands moves together.
const char* const linkedSuffixes[] { "thr", "ratio", "att", "rel", "gain" };

// Every parameter of a band except its top frequency, which belongs to the crossover.
const char* const bandSuffixes[] { "on", "solo", "thr", "ratio", "knee", "att", "rel", "rms", "gain", "mk", "ar", "fb", "sc" };

juce::NormalisableRange<float> skewedRange (float high, float interval, float centre)
{
    juce::NormalisableRange<float> range (0.0f, high, interval);
    range.setSkewForCentre (centre);
    return range;
}

std::unique_ptr<juce::AudioParameterFloat> timeParameter (const juce::String& id, const juce::String& name, float high, float centre, float defaultMs)
{
    return fancy::floatParameter (
        id, name, skewedRange (high, 0.1f, centre), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return (value < 100.0f ? fancy::formatFixed (value, 1) : juce::String (juce::roundToInt (value))) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseMs (text); }));
}

juce::String ratioText (float ratio)
{
    return ratio >= (float) MultiCompDsp::infiniteRatio ? juce::String ("inf:1") : fancy::formatFixed (ratio, 1) + ":1";
}

juce::String signedDb (float db)
{
    return (db > 0.0f ? "+" : "") + fancy::formatFixed (db, 1) + " dB";
}

fancy::FancyProcessor::Layout createLayout (const fancy::SwitchableFrequencyParameter::Flag& logScale)
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using BoolAttributes = juce::AudioParameterBoolAttributes;
    using FloatAttributes = juce::AudioParameterFloatAttributes;

    fancy::FancyProcessor::Layout layout;

    const auto toggle = [] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue);
    };

    const auto option = [] (const char* id, const char* name, bool defaultValue)
    {
        return std::make_unique<Bool> (juce::ParameterID { id, 1 }, name, defaultValue, BoolAttributes {}.withAutomatable (false));
    };

    constexpr float defaultTops[] { 120.0f, 1200.0f, 6000.0f, 8000.0f, 10000.0f, 12000.0f, 15000.0f, 20000.0f };

    for (int band = 0; band < maxBands; ++band)
    {
        const auto prefix = "Band " + juce::String (band + 1) + " ";
        const auto id = [band] (const char* suffix) { return MultiCompProcessor::id (band, suffix); };

        layout.add (toggle (id ("on"), prefix + "Active", true));
        layout.add (toggle (id ("solo"), prefix + "Solo", false));

        layout.add (std::make_unique<fancy::SwitchableFrequencyParameter> (id ("top"), prefix + "Top frequency", logScale,
                                                                           (float) multicomp::lowestHz, (float) multicomp::highestHz, defaultTops[band]));

        layout.add (fancy::levelParameter (id ("thr"), prefix + "Threshold", -60.0f, 0.0f, 0.0f, -100.0f));

        layout.add (fancy::floatParameter (
            id ("ratio"), prefix + "Ratio", fancy::logRange (0.5f, (float) MultiCompDsp::infiniteRatio, 0.01f), 2.0f,
            FloatAttributes {}
                .withStringFromValueFunction ([] (float value, int) { return ratioText (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseExpansionRatio (text, (float) MultiCompDsp::infiniteRatio); })));

        layout.add (fancy::floatParameter (
            id ("knee"), prefix + "Knee", skewedRange (24.0f, 0.1f, 6.0f), 6.0f,
            FloatAttributes {}
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float value, int) { return fancy::formatFixed (value, 1) + " dB"; })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

        layout.add (timeParameter (id ("att"), prefix + "Attack", 500.0f, 30.0f, 10.0f));
        layout.add (timeParameter (id ("rel"), prefix + "Release", 5000.0f, 300.0f, 120.0f));
        layout.add (timeParameter (id ("rms"), prefix + "RMS size", 250.0f, 30.0f, 5.0f));

        layout.add (fancy::floatParameter (
            id ("gain"), prefix + "Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            FloatAttributes {}
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float value, int) { return signedDb (value); })
                .withValueFromStringFunction ([] (const juce::String& text) { return fancy::parseNumber (text); })));

        layout.add (toggle (id ("mk"), prefix + "Auto make-up", false));
        layout.add (toggle (id ("ar"), prefix + "Auto release", true));
        layout.add (toggle (id ("fb"), prefix + "Feedback detect", false));
        layout.add (toggle (id ("sc"), prefix + "Sidechain", false));
    }

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "bands", 1 }, "Bands", 1, maxBands, 4,
                                                           juce::AudioParameterIntAttributes {}.withAutomatable (false)));

    layout.add (std::make_unique<Choice> (
        juce::ParameterID { "detector", 1 }, "Detector",
        juce::StringArray { "Main input L+R", "Main L", "Main R", "Sidechain L+R", "Sidechain L", "Sidechain R" }, 0));

    const auto dot = juce::String::charToString (0x00b7);

    layout.add (std::make_unique<Choice> (
        juce::ParameterID { "crossover", 1 }, "Crossover",
        juce::StringArray { "LR 24 dB " + dot + " Min phase", "LR 48 dB " + dot + " Min phase", "Linear phase" }, 0));

    layout.add (toggle ("link", "Link bands", false));
    layout.add (toggle ("limit", "Limit output", false));
    layout.add (toggle ("attack90", "Attack = 90 % rise", false));

    layout.add (fancy::levelParameter ("wet", "Wet", mixFloorDb, 12.0f, 0.0f, mixFloorDb));
    layout.add (fancy::levelParameter ("dry", "Dry", mixFloorDb, 12.0f, mixFloorDb, mixFloorDb));
    layout.add (fancy::levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f));

    layout.add (option ("logFreq", "Log-scale automated freq.", true));
    layout.add (option ("showSpectrum", "Show spectrum", true));
    layout.add (option ("showGrid", "Show grid", true));

    layout.add (toggle ("bypass", "Bypass", false));

    return layout;
}

std::vector<fancy::FactoryPreset> createPresets()
{
    using Values = std::vector<std::pair<juce::String, float>>;

    const auto everyBand = [] (std::initializer_list<std::pair<const char*, float>> values)
    {
        Values result;

        for (int band = 0; band < 4; ++band)
            for (const auto& [suffix, value] : values)
                result.emplace_back (MultiCompProcessor::id (band, suffix), value);

        return result;
    };

    const auto with = [] (Values values, std::initializer_list<std::pair<const char*, float>> extra)
    {
        for (const auto& [id, value] : extra)
            values.emplace_back (id, value);

        return values;
    };

    return {
        { "Default", {} },
        { "Gentle Glue", everyBand ({ { "thr", -24.0f }, { "ratio", 1.5f }, { "knee", 12.0f }, { "att", 30.0f }, { "rel", 200.0f }, { "mk", 1.0f } }) },
        { "Drum Bus", with (everyBand ({ { "thr", -20.0f }, { "ratio", 3.0f }, { "att", 15.0f }, { "rel", 120.0f } }),
                            { { "b1thr", -24.0f }, { "b1att", 30.0f }, { "b4thr", -28.0f }, { "b4ratio", 2.0f } }) },
        { "Vocal Smoother", with (everyBand ({ { "thr", -26.0f }, { "ratio", 2.5f }, { "att", 8.0f }, { "rel", 90.0f } }),
                                  { { "b1ratio", 1.0f }, { "b1top", 200.0f }, { "b2top", 2000.0f }, { "b3top", 7000.0f }, { "b4thr", -34.0f }, { "b4ratio", 4.0f } }) },
        { "Master Linear", with (everyBand ({ { "thr", -14.0f }, { "ratio", 1.5f }, { "knee", 12.0f }, { "att", 40.0f }, { "rel", 300.0f }, { "mk", 1.0f } }),
                                 { { "crossover", 2.0f } }) },
        { "Multiband Limiter", with (everyBand ({ { "thr", -6.0f }, { "ratio", (float) MultiCompDsp::infiniteRatio }, { "knee", 0.0f }, { "att", 0.0f }, { "rel", 80.0f }, { "rms", 0.0f } }),
                                     { { "limit", 1.0f } }) },
    };
}
} // namespace

MultiCompProcessor::MultiCompProcessor()
    : MultiCompProcessor (std::make_shared<std::atomic<bool>> (true))
{
}

MultiCompProcessor::MultiCompProcessor (SharedFlag logScaleFlag)
    : FancyProcessor ("Fancy MultiComp", "FancyMultiComp",
                      BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false),
                      createLayout (logScaleFlag), createPresets()),
      logScale (std::move (logScaleFlag)),
      designThread (*this)
{
    auto& apvts = getApvts();

    for (int band = 0; band < maxBands; ++band)
    {
        const auto value = [&] (const char* suffix) { return apvts.getRawParameterValue (id (band, suffix)); };

        bandValues[(size_t) band] = { value ("on"), value ("solo"), value ("top"), value ("thr"), value ("ratio"), value ("knee"), value ("att"),
                                      value ("rel"), value ("rms"), value ("gain"), value ("mk"), value ("ar"), value ("fb"), value ("sc") };

        for (const auto* suffix : linkedSuffixes)
            apvts.getParameter (id (band, suffix))->addListener (this);
    }

    apvts.addParameterListener ("logFreq", this);
}

MultiCompProcessor::~MultiCompProcessor()
{
    auto& apvts = getApvts();

    apvts.removeParameterListener ("logFreq", this);

    for (int band = 0; band < maxBands; ++band)
        for (const auto* suffix : linkedSuffixes)
            apvts.getParameter (id (band, suffix))->removeListener (this);

    designThread.stopThread (2000);
}

void MultiCompProcessor::parameterChanged (const juce::String&, float newValue)
{
    logScale->store (newValue >= 0.5f, std::memory_order_relaxed);

    // Each band keeps its Hz, and the host learns where that now sits in 0-1.
    for (int band = 0; band < maxBands; ++band)
        if (auto* frequency = dynamic_cast<juce::AudioParameterFloat*> (getApvts().getParameter (id (band, "top"))))
            frequency->setValueNotifyingHost (frequency->convertTo0to1 (frequency->get()));
}

void MultiCompProcessor::parameterGestureChanged (int parameterIndex, bool gestureIsStarting)
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
        return;

    if (gestureIsStarting)
    {
        gestureParameter = parameterIndex;
        gestureValue = getParameters()[parameterIndex]->getValue();
    }
    else if (gestureParameter == parameterIndex)
    {
        gestureParameter = -1;
    }
}

void MultiCompProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    // Automation, presets and state load set each band to its own value, so only a person
    // moving a control counts.
    if (propagating || parameterIndex != gestureParameter || ! juce::MessageManager::existsAndIsCurrentThread())
        return;

    const auto delta = newValue - gestureValue;
    gestureValue = newValue;

    if (! isOn ("link") || delta == 0.0f || juce::ModifierKeys::currentModifiers.isAltDown())
        return;

    const auto* changed = dynamic_cast<juce::RangedAudioParameter*> (getParameters()[parameterIndex]);

    if (changed == nullptr)
        return;

    const auto changedId = changed->getParameterID();
    const auto suffix = changedId.substring (2);
    const auto numBands = getNumBands();

    const juce::ScopedValueSetter<bool> guard (propagating, true);

    for (int band = 0; band < numBands; ++band)
    {
        const auto otherId = id (band, suffix.toRawUTF8());

        if (otherId == changedId)
            continue;

        auto& other = *getApvts().getParameter (otherId);

        other.beginChangeGesture();
        other.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, other.getValue() + delta));
        other.endChangeGesture();
    }
}

float MultiCompProcessor::read (const char* parameterId) const
{
    return const_cast<MultiCompProcessor*> (this)->getApvts().getRawParameterValue (parameterId)->load (std::memory_order_relaxed);
}

bool MultiCompProcessor::isOn (const char* parameterId) const
{
    return read (parameterId) >= 0.5f;
}

int MultiCompProcessor::getNumBands() const
{
    return juce::jlimit (1, maxBands, juce::roundToInt (read ("bands")));
}

MultiCompDsp::BandSpec MultiCompProcessor::readBand (int band) const
{
    const auto& values = bandValues[(size_t) band];

    const auto load = [] (const std::atomic<float>* value) { return value->load (std::memory_order_relaxed); };
    const auto on = [&] (const std::atomic<float>* value) { return load (value) >= 0.5f; };

    MultiCompDsp::BandSpec spec;

    spec.topHz = (double) load (values.top);
    spec.thresholdDb = (double) load (values.threshold);
    spec.ratio = (double) load (values.ratio);
    spec.kneeDb = (double) load (values.knee);
    spec.attackMs = (double) load (values.attack);
    spec.releaseMs = (double) load (values.release);
    spec.rmsMs = (double) load (values.rms);
    spec.gainDb = (double) load (values.gain);
    spec.active = on (values.on);
    spec.solo = on (values.solo);
    spec.autoMakeup = on (values.makeup);
    spec.autoRelease = on (values.autoRelease);
    spec.feedback = on (values.feedback);
    spec.sidechain = on (values.sidechain);

    return spec;
}

MultiCompDsp::Targets MultiCompProcessor::readTargets() const
{
    MultiCompDsp::Targets targets;

    for (int band = 0; band < maxBands; ++band)
        targets.bands[(size_t) band] = readBand (band);

    targets.numBands = getNumBands();
    targets.detector = juce::roundToInt (read ("detector"));
    targets.crossover = (multicomp::CrossoverType) juce::jlimit (0, 2, juce::roundToInt (read ("crossover")));
    targets.wetGain = fancy::levelToGain (read ("wet"), mixFloorDb);
    targets.dryGain = fancy::levelToGain (read ("dry"), mixFloorDb);
    targets.outputGain = fancy::dbToGain ((double) read ("output"));
    targets.limitOutput = isOn ("limit");
    targets.attack90 = isOn ("attack90");
    targets.bypass = isOn ("bypass");

    return targets;
}

double MultiCompProcessor::bandBottomHz (int band) const
{
    if (band <= 0)
        return multicomp::lowestHz;

    std::array<double, multicomp::maxCrossovers> hz {};
    const auto count = getNumBands() - 1;

    for (int i = 0; i < count; ++i)
        hz[(size_t) i] = readBand (i).topHz;

    multicomp::sanitiseCrossovers (hz.data(), count);

    return hz[(size_t) band - 1];
}

double MultiCompProcessor::bandTopHz (int band) const
{
    return band >= getNumBands() - 1 ? multicomp::highestHz : bandBottomHz (band + 1);
}

void MultiCompProcessor::DesignThread::run()
{
    while (! threadShouldExit())
    {
        // An offline render designs on its own thread, which keeps it repeatable.
        if (! processor.isNonRealtime())
            processor.dsp.updateLinearDesign (processor.readTargets());

        wait (designIntervalMs);
    }
}

void MultiCompProcessor::prepareToPlay (double sampleRate, int)
{
    designThread.stopThread (2000);

    dsp.prepare (sampleRate);
    dsp.setSynchronousDesign (isNonRealtime());
    dsp.snapToTargets (readTargets());
    setLatencySamples (dsp.getLatencySamples());

    if (! isNonRealtime())
        designThread.startThread (juce::Thread::Priority::low);
}

void MultiCompProcessor::releaseResources()
{
    designThread.stopThread (2000);
}

void MultiCompProcessor::reset()
{
    dsp.reset();
}

bool MultiCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    const auto sidechain = layouts.getChannelSet (true, 1);

    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

void MultiCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    auto mainBuffer = getBusBuffer (buffer, true, 0);
    auto sidechainBuffer = getBusBuffer (buffer, true, 1);

    const auto numChannels = juce::jmin (mainBuffer.getNumChannels(), 2);
    const auto numSamples = mainBuffer.getNumSamples();

    if (numChannels == 0)
        return;

    const auto main = mainBuffer.getArrayOfWritePointers();
    const auto numSidechain = juce::jmin (sidechainBuffer.getNumChannels(), 2);

    inputMeter.push (main, numChannels, numSamples);

    if (analyzerActive.load (std::memory_order_relaxed))
        analyzerFifo.push (main, numChannels, numSamples);

    dsp.setSynchronousDesign (isNonRealtime());
    dsp.setTargets (readTargets());
    dsp.process (main, numChannels, sidechainBuffer.getArrayOfReadPointers(), numSidechain, numSamples);

    outputMeter.push (main, numChannels, numSamples);

    if (getLatencySamples() != dsp.getLatencySamples())
        setLatencySamples (dsp.getLatencySamples());
}

void MultiCompProcessor::setParameter (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);

    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
    parameter.endChangeGesture();
}

// Not a gesture, because rearranging bands isn't a move Link bands should repeat.
void MultiCompProcessor::writeValue (const juce::String& parameterId, float value)
{
    auto& parameter = *getApvts().getParameter (parameterId);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void MultiCompProcessor::copyBand (int from, int to)
{
    for (const auto* suffix : bandSuffixes)
    {
        const auto& source = *getApvts().getParameter (id (from, suffix));
        getApvts().getParameter (id (to, suffix))->setValueNotifyingHost (source.getValue());
    }
}

void MultiCompProcessor::resetBand (int band)
{
    for (const auto* suffix : bandSuffixes)
    {
        auto& parameter = *getApvts().getParameter (id (band, suffix));
        parameter.setValueNotifyingHost (parameter.getDefaultValue());
    }
}

int MultiCompProcessor::splitBand (int band, double hz)
{
    const auto numBands = getNumBands();

    if (numBands >= maxBands)
        return -1;

    const auto bottom = bandBottomHz (band), top = bandTopHz (band);
    const auto lowest = bottom * multicomp::minCrossoverRatio, highest = top / multicomp::minCrossoverRatio;

    if (lowest > highest)
        return -1;

    const auto crossover = juce::jlimit (lowest, highest, hz);

    std::array<float, maxBands> tops {};

    for (int b = 0; b < numBands; ++b)
        tops[(size_t) b] = (float) readBand (b).topHz;

    for (int b = numBands - 1; b > band; --b)
    {
        copyBand (b, b + 1);
        writeValue (id (b + 1, "top"), tops[(size_t) b]);
    }

    // Both halves keep the band's settings, so splitting doesn't change the sound.
    copyBand (band, band + 1);
    writeValue (id (band + 1, "top"), tops[(size_t) band]);
    writeValue (id (band, "top"), (float) crossover);
    writeValue ("bands", (float) (numBands + 1));

    return band + 1;
}

void MultiCompProcessor::removeBand (int band)
{
    const auto numBands = getNumBands();

    if (numBands <= 1)
        return;

    std::array<float, maxBands> tops {};

    for (int b = 0; b < numBands; ++b)
        tops[(size_t) b] = (float) readBand (b).topHz;

    for (int b = band; b < numBands - 1; ++b)
    {
        copyBand (b + 1, b);
        writeValue (id (b, "top"), tops[(size_t) b + 1]);
    }

    resetBand (numBands - 1);
    writeValue ("bands", (float) (numBands - 1));
}

void MultiCompProcessor::setCrossover (int crossover, double hz)
{
    const auto lowest = bandBottomHz (crossover) * multicomp::minCrossoverRatio;
    const auto highest = (crossover + 2 >= getNumBands() ? multicomp::highestHz : bandTopHz (crossover + 1) / multicomp::minCrossoverRatio);

    setParameter (id (crossover, "top"), (float) juce::jlimit (juce::jmin (lowest, highest), highest, hz));
}

juce::AudioProcessorEditor* MultiCompProcessor::createEditor()
{
    return new MultiCompEditor (*this);
}
