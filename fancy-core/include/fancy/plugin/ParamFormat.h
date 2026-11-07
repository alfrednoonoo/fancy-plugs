#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <string>

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

// Whole Hz, then one decimal of kHz, then whole kHz: for knobs that only need a rough reading.
inline juce::String formatFilterHz (float hz)
{
    if (hz < 1000.0f)
        return juce::String (juce::roundToInt (hz)) + " Hz";

    const auto kilohertz = hz / 1000.0f;
    return (kilohertz >= 10.0f ? juce::String (juce::roundToInt (kilohertz)) : formatFixed (kilohertz, 1)) + " kHz";
}

// Reads the first number in typed text, ignoring any unit around it. Accepts a typographic
// minus and a comma as either decimal point (2,5) or thousands separator (2,500).
inline float parseNumber (const juce::String& text)
{
    const auto typed = text.replaceCharacter ((juce::juce_wchar) 0x2212, '-').toStdString();
    const auto isDigit = [&] (size_t index) { return index < typed.size() && typed[index] >= '0' && typed[index] <= '9'; };
    const auto isPoint = [&] (size_t index) { return index < typed.size() && (typed[index] == '.' || typed[index] == ','); };

    auto start = typed.find_first_of ("0123456789");

    if (start == std::string::npos)
        return 0.0f;

    if (start > 0 && isPoint (start - 1))
        --start;

    std::string number (start > 0 && typed[start - 1] == '-' ? "-" : "");
    const auto hasWholePart = isDigit (start);
    auto seenPoint = false;

    for (auto i = start; i < typed.size(); ++i)
    {
        if (isDigit (i))
        {
            number += typed[i];
        }
        else if (isPoint (i) && ! seenPoint && isDigit (i + 1))
        {
            const auto isThousands = typed[i] == ','
                                     && isDigit (i + 2) && isDigit (i + 3) && ! isDigit (i + 4)
                                     && hasWholePart && typed[start] != '0';

            if (! isThousands)
            {
                number += '.';
                seenPoint = true;
            }
        }
        else
        {
            break;
        }
    }

    return juce::String (number).getFloatValue();
}

inline bool isInfinityText (const juce::String& text)
{
    return text.containsIgnoreCase ("inf") || text.containsChar ((juce::juce_wchar) 0x221e);
}

// Whether typed text holds a value at all, so that a typo can leave the control as it was.
inline bool isValueText (const juce::String& text)
{
    return text.containsAnyOf ("0123456789") || isInfinityText (text);
}

inline float parseHz (const juce::String& text)
{
    const auto value = parseNumber (text);
    return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
}

// Accepts 4, 4:1 and 1:4, which all mean the same ratio.
inline float parseRatio (const juce::String& text, float infiniteRatio)
{
    if (isInfinityText (text))
        return infiniteRatio;

    const auto first = parseNumber (text.upToFirstOccurrenceOf (":", false, false));
    const auto second = parseNumber (text.fromFirstOccurrenceOf (":", false, false));

    return std::min (first, second) > 0.0f ? std::max (first, second) / std::min (first, second)
                                           : std::max (first, second);
}

// For a ratio that can go below 1:1, where the order matters: a:b is a / b.
inline float parseExpansionRatio (const juce::String& text, float infiniteRatio)
{
    if (isInfinityText (text))
        return infiniteRatio;

    const auto first = parseNumber (text.upToFirstOccurrenceOf (":", false, false));
    const auto second = text.containsChar (':') ? parseNumber (text.fromFirstOccurrenceOf (":", false, false)) : 1.0f;

    return second > 0.0f ? first / second : first;
}

// Values at or below floorDb display as -inf.
inline juce::String formatDb (float db, float floorDb)
{
    return db <= floorDb ? juce::String ("-inf dB") : formatFixed (db, 1) + " dB";
}

inline float parseDb (const juce::String& text, float floorDb)
{
    return isInfinityText (text) ? floorDb : parseNumber (text);
}

} // namespace fancy
