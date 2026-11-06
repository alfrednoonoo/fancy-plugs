#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fancy::ui
{

// Exact fills from the Figma file (design/figma-spec.json). Colours Figma doesn't use keep
// their BUILD_PROMPT §2.1 values.
namespace colours
{
inline const juce::Colour bgPage { 0xff121212 }, bgHeader { 0xff1e1e1e }, bgBody { 0xff303030 },
    bgSection { 0xff262626 }, bgWell { 0xff141414 }, bgVerb { 0xff292929 }, bgVerbAlt { 0xff1f1f1f };

inline const juce::Colour knobTop { 0xff4f4f4f }, knobBottom { 0xff454545 };

inline const juce::Colour grey5 { 0xff424242 }, grey6 { 0xff4c4c4c };

inline const juce::Colour textSecondary { 0xff8d8d8d }, textPrimary { 0xffffffff };

inline const juce::Colour accentYellow { 0xfff1c315 }, accentOrange { 0xffe77e23 }, accentRed { 0xffe74d3d },
    accentPurple { 0xff9a59b5 }, accentBlue { 0xff3598db }, accentTeal { 0xff1bbc9c }, accentGreen { 0xff2dcc70 };

inline const juce::Colour uiTeal { 0xff00a985 }, uiTealBright { 0xff1bbc9c }, uiRed { 0xffe74d3d },
    uiBlue { 0xff3598db }, uiPurple { 0xff9a59b5 }, uiEqBlue { 0xff24aec6 },
    uiEqGreen { 0xff0cc08a }, uiEqRed { 0xffc0363c };
} // namespace colours

enum class Weight { regular, medium, semiBold };

// Holds the embedded Inter typefaces. Editors keep one alive for their lifetime.
struct Typefaces
{
    Typefaces();

    juce::Typeface::Ptr regular, medium, semiBold;
};

// Height is the em size, matching font sizes in the Figma file.
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
juce::Path plus (juce::Rectangle<float> area);
} // namespace icons

} // namespace fancy::ui
