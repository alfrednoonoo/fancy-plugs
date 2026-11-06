#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace fancy
{

inline juce::String formatFixed (float value, int decimals)
{
    const auto scale = std::pow (10.0f, (float) decimals);
    const auto rounded = std::round (value * scale) / scale;
    return juce::String (rounded == 0.0f ? 0.0f : rounded, decimals);
}

inline juce::String formatHz (float hz)
{
    return hz >= 1000.0f ? formatFixed (hz / 1000.0f, 2) + " kHz"
                         : formatFixed (hz, 1) + " Hz";
}

inline float parseHz (const juce::String& text)
{
    const auto value = text.getFloatValue();
    return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
}

// Values at or below floorDb display as -inf.
inline juce::String formatDb (float db, float floorDb)
{
    return db <= floorDb ? juce::String ("-inf dB") : formatFixed (db, 1) + " dB";
}

inline float parseDb (const juce::String& text, float floorDb)
{
    return text.containsIgnoreCase ("inf") ? floorDb : text.getFloatValue();
}

} // namespace fancy
