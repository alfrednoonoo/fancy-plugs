#pragma once

#include "ParamHelpers.h"

#include <fancy/dsp/DeEsser.h>

namespace fancy::deesser
{

// Fancy DS's continuous parameters, built in one place. Fancy DS adds them to its layout; Fancy
// DS-Mini builds private copies to round its mapped settings exactly as Fancy DS's parameters
// round them, so the two plugins sound the same, bit for bit (docs/ds-mini.md).

inline std::unique_ptr<juce::AudioParameterFloat> dbParameter (const char* id, const char* name, float low, float high, float defaultDb, int decimals = 1)
{
    return floatParameter (
        id, name, juce::NormalisableRange<float> (low, high, 0.1f), defaultDb,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("dB")
            .withStringFromValueFunction ([decimals] (float value, int) { return formatFixed (value, decimals) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return parseNumber (text); }));
}

inline std::unique_ptr<juce::AudioParameterFloat> msParameter (const char* id, const char* name, float low, float high, float defaultMs)
{
    return floatParameter (
        id, name, logRange (low, high, 0.1f), defaultMs,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float value, int) { return (value < 10.0f ? formatFixed (value, 1) : juce::String (juce::roundToInt (value))) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return parseMs (text); }));
}

inline std::unique_ptr<juce::AudioParameterFloat> threshold() { return dbParameter ("threshold", "Threshold", -60.0f, 0.0f, -28.0f); }

inline std::unique_ptr<juce::AudioParameterFloat> freq()
{
    return floatParameter (
        "freq", "Freq", logRange ((float) dsp::DeEsser::minHz, (float) dsp::DeEsser::maxHz, 1.0f), 6200.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float value, int) { return formatFixed (value / 1000.0f, 2) + " kHz"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return parseHz (text); }));
}

inline std::unique_ptr<juce::AudioParameterFloat> width()
{
    return floatParameter (
        "width", "Width", juce::NormalisableRange<float> (0.3f, 3.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes {}
            .withLabel ("oct")
            .withStringFromValueFunction ([] (float value, int) { return formatFixed (value, 1) + " oct"; })
            .withValueFromStringFunction ([] (const juce::String& text) { return parseNumber (text); }));
}

inline std::unique_ptr<juce::AudioParameterFloat> range() { return dbParameter ("range", "Range", -24.0f, 0.0f, -12.0f, 0); }
inline std::unique_ptr<juce::AudioParameterFloat> attack() { return msParameter ("attack", "Attack", 0.1f, 20.0f, 0.5f); }
inline std::unique_ptr<juce::AudioParameterFloat> release() { return msParameter ("release", "Release", 5.0f, 500.0f, 60.0f); }
inline std::unique_ptr<juce::AudioParameterFloat> output() { return levelParameter ("output", "Output", -24.0f, 12.0f, 0.0f, -100.0f); }

// The value Fancy DS reads after a host or a preset sets a parameter to `value`. setValue stores
// convertFrom0to1 (convertTo0to1 (value)); the value tree then holds convertFrom0to1 (getValue()),
// and getValue() is convertTo0to1 of what's stored. Only const calls, so any thread can use it.
inline float roundAsParameter (const juce::AudioParameterFloat& parameter, float value)
{
    const auto stored = parameter.convertFrom0to1 (parameter.convertTo0to1 (value));
    return parameter.convertFrom0to1 (parameter.convertTo0to1 (stored));
}

} // namespace fancy::deesser
