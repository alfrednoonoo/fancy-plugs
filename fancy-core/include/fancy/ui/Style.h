#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fancy::ui
{

// BUILD_PROMPT §2.1. Figma values replace these when available.
namespace colours
{
inline const juce::Colour bgPage { 0xff121212 }, bgHeader { 0xff1e1e1e }, bgBody { 0xff303030 },
    bgSection { 0xff262626 }, bgWell { 0xff141414 }, bgVerb { 0xff292929 }, bgVerbAlt { 0xff1f1f1f };

inline const juce::Colour knobTop { 0xff4f4f4f }, knobBottom { 0xff454545 }, knobTrack { 0xff212121 };

inline const juce::Colour grey3 { 0xff282828 }, grey5 { 0xff434343 }, grey6 { 0xff4c4c4c };

inline const juce::Colour textSecondary { 0xff909090 }, textPrimary { 0xffededed };

inline const juce::Colour accentYellow { 0xfff1c315 }, accentOrange { 0xffe77e23 }, accentRed { 0xffe84c3c },
    accentPurple { 0xff965ab7 }, accentBlue { 0xff3798dc }, accentTeal { 0xff1ebaa1 }, accentGreen { 0xff2acc77 };

inline const juce::Colour uiTeal { 0xff00a884 }, uiTealBright { 0xff00b478 }, uiRed { 0xffe84e3c },
    uiBlue { 0xff3690cc }, uiPurple { 0xff9648ba }, uiPurpleIr { 0xff9858b8 }, uiEqBlue { 0xff24aec6 },
    uiEqGreen { 0xff0cc08a }, uiEqRed { 0xffc0363c };
} // namespace colours

enum class Weight { regular, medium, semiBold };

// Holds the embedded Inter typefaces. Editors keep one alive for their lifetime.
struct Typefaces
{
    Typefaces();

    juce::Typeface::Ptr regular, medium, semiBold;
};

juce::Font font (Weight weight, float height);

class FancyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FancyLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getPopupMenuFont() override;

private:
    juce::SharedResourcePointer<Typefaces> typefaces;
};

namespace icons
{
juce::Path power (juce::Rectangle<float> area);
juce::Path gear (juce::Rectangle<float> area);
juce::Path chevrons (juce::Rectangle<float> area);
} // namespace icons

} // namespace fancy::ui
