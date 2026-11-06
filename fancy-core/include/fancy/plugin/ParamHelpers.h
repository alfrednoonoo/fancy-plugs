#pragma once

#include "ParamFormat.h"

#include <fancy/Decibels.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <memory>

namespace fancy
{

inline juce::NormalisableRange<float> logRange (float low, float high, float interval)
{
    return { low, high,
             [] (float start, float end, float normalised) { return start * std::pow (end / start, normalised); },
             [] (float start, float end, float value) { return std::log (value / start) / std::log (end / start); },
             [interval] (float start, float end, float value) { return juce::jlimit (start, end, std::round (value / interval) / (1.0f / interval)); } };
}

// A parameter's stored value is snapped each time it is set. A default that isn't already
// on that grid would move when a host writes the same value back, changing the audio.
inline float legalDefault (const juce::NormalisableRange<float>& range, float value)
{
    auto legal = range.snapToLegalValue (value);

    for (int i = 0; i < 3; ++i)
        legal = range.snapToLegalValue (range.convertFrom0to1 (range.convertTo0to1 (legal)));

    return legal;
}

inline std::unique_ptr<juce::AudioParameterFloat> floatParameter (const juce::String& id, const juce::String& name,
                                                                  const juce::NormalisableRange<float>& range, float defaultValue,
                                                                  const juce::AudioParameterFloatAttributes& attributes)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, legalDefault (range, defaultValue), attributes);
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
            .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); }));
}

// Silence at or below floorDb, otherwise the linear gain.
inline double levelToGain (float db, float floorDb)
{
    return db <= floorDb ? 0.0 : dbToGain ((double) db);
}

} // namespace fancy
