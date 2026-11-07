#pragma once

#include "ParamFormat.h"

#include <fancy/Decibels.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>
#include <memory>

namespace fancy
{

inline juce::NormalisableRange<float> logRange (float low, float high, float interval)
{
    return { low, high,
             [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
             [] (float start, float end, float value) { return std::log (juce::jlimit (start, end, value) / start) / std::log (end / start); },
             [interval] (float start, float end, float value) { return juce::jlimit (start, end, std::round (value / interval) / (1.0f / interval)); } };
}

inline std::unique_ptr<juce::AudioParameterFloat> floatParameter (const juce::String& id, const juce::String& name,
                                                                  const juce::NormalisableRange<float>& range, float defaultValue,
                                                                  const juce::AudioParameterFloatAttributes& attributes)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue, attributes);
}

// Values at or below floorDb display as -inf. Pass a floor below the range for a level
// that never reaches silence.
inline std::unique_ptr<juce::AudioParameterFloat> levelParameter (const juce::String& id, const juce::String& name,
                                                                  float low, float high, float defaultDb, float floorDb)
{
    return floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([floorDb] (float value, int) { return formatDb (value, floorDb); })
            .withValueFromStringFunction ([floorDb] (const juce::String& text) { return parseDb (text, floorDb); }));
}

inline std::unique_ptr<juce::AudioParameterFloat> percentParameter (const juce::String& id, const juce::String& name, float defaultPercent)
{
    return floatParameter (
        id, name, juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), defaultPercent,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return parseNumber (text); }));
}

// A frequency whose 0-1 mapping for automation can switch between log and linear. The Hz
// value doesn't change when the flag does, only its 0-1 position for the host. JUCE caches the
// default as a 0-1 value, so it's recalculated here for the current mapping.
class SwitchableFrequencyParameter final : public juce::AudioParameterFloat
{
public:
    using Flag = std::shared_ptr<std::atomic<bool>>;

    SwitchableFrequencyParameter (const juce::String& id, const juce::String& name, const Flag& logScale,
                                  float lowHz, float highHz, float defaultHz)
        : AudioParameterFloat (juce::ParameterID { id, 1 }, name, makeRange (logScale, lowHz, highHz), defaultHz,
                               juce::AudioParameterFloatAttributes {}
                                   .withLabel ("Hz")
                                   .withStringFromValueFunction ([] (float value, int) { return formatHz (value); })
                                   .withValueFromStringFunction ([] (const juce::String& text) { return parseHz (text); })),
          defaultValue (defaultHz)
    {
    }

    float getDefaultValue() const override { return convertTo0to1 (defaultValue); }

private:
    static juce::NormalisableRange<float> makeRange (const Flag& logScale, float lowHz, float highHz)
    {
        return { lowHz, highHz,
                 [logScale] (float start, float end, float normalised)
                 {
                     return logScale->load (std::memory_order_relaxed) ? start * std::pow (end / start, normalised)
                                                                       : start + (end - start) * normalised;
                 },
                 [logScale] (float start, float end, float value)
                 {
                     return logScale->load (std::memory_order_relaxed) ? std::log (juce::jlimit (start, end, value) / start) / std::log (end / start)
                                                                       : (value - start) / (end - start);
                 },
                 [] (float start, float end, float value)
                 {
                     return juce::jlimit (start, end, std::round (value * 10.0f) / 10.0f);
                 } };
    }

    float defaultValue;
};

// Silence at or below floorDb, otherwise the linear gain.
inline double levelToGain (float db, float floorDb)
{
    return db <= floorDb ? 0.0 : dbToGain ((double) db);
}

} // namespace fancy
