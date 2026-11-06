#include <fancy/ui/Style.h>

#include "FancyFontData.h"

#include <numbers>

namespace fancy::ui
{

Typefaces::Typefaces()
{
    using namespace FancyFontData;
    regular = juce::Typeface::createSystemTypefaceFor (InterRegular_ttf, (size_t) InterRegular_ttfSize);
    medium = juce::Typeface::createSystemTypefaceFor (InterMedium_ttf, (size_t) InterMedium_ttfSize);
    semiBold = juce::Typeface::createSystemTypefaceFor (InterSemiBold_ttf, (size_t) InterSemiBold_ttfSize);
}

juce::Font font (Weight weight, float height)
{
    const juce::SharedResourcePointer<Typefaces> typefaces;

    const auto typeface = weight == Weight::regular  ? typefaces->regular
                        : weight == Weight::medium   ? typefaces->medium
                                                     : typefaces->semiBold;

    return juce::Font (juce::FontOptions {}.withTypeface (typeface).withHeight (height));
}

FancyLookAndFeel::FancyLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::grey3);
    setColour (juce::PopupMenu::textColourId, colours::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::uiTeal);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, colours::bgWell);
    setColour (juce::TextEditor::textColourId, colours::textPrimary);
    setColour (juce::TextEditor::highlightColourId, colours::uiTeal);
    setColour (juce::TextEditor::outlineColourId, colours::grey5);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::uiTeal);
    setColour (juce::CaretComponent::caretColourId, colours::textPrimary);
    setColour (juce::AlertWindow::backgroundColourId, colours::bgBody);
    setColour (juce::AlertWindow::textColourId, colours::textPrimary);
    setColour (juce::AlertWindow::outlineColourId, colours::grey5);
    setColour (juce::TextButton::buttonColourId, colours::grey3);
    setColour (juce::TextButton::textColourOffId, colours::textPrimary);
    setColour (juce::ResizableWindow::backgroundColourId, colours::bgBody);
}

juce::Typeface::Ptr FancyLookAndFeel::getTypefaceForFont (const juce::Font& requested)
{
    if (requested.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typefaces->regular;

    return LookAndFeel_V4::getTypefaceForFont (requested);
}

juce::Font FancyLookAndFeel::getPopupMenuFont()
{
    return font (Weight::regular, 13.0f);
}

namespace icons
{

juce::Path power (juce::Rectangle<float> area)
{
    const auto centre = area.getCentre();
    const auto radius = area.getWidth() * 0.42f;
    const auto gap = 0.65f;

    juce::Path path;
    path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, gap, 2.0f * std::numbers::pi_v<float> - gap, true);
    path.startNewSubPath (centre.x, area.getY());
    path.lineTo (centre.x, centre.y);
    return path;
}

juce::Path gear (juce::Rectangle<float> area)
{
    constexpr int teeth = 8;
    const auto centre = area.getCentre();
    const auto outer = area.getWidth() * 0.5f;
    const auto inner = outer * 0.74f;
    const auto step = 2.0f * std::numbers::pi_v<float> / (float) teeth;

    struct Corner { float offset, radius; };
    const Corner profile[] { { -0.38f, inner }, { -0.24f, outer }, { 0.24f, outer }, { 0.38f, inner } };

    juce::Path path;

    for (int tooth = 0; tooth < teeth; ++tooth)
    {
        for (const auto& corner : profile)
        {
            const auto point = centre.getPointOnCircumference (corner.radius, ((float) tooth + corner.offset) * step);

            if (path.isEmpty())
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
    }

    path.closeSubPath();
    path.addEllipse (area.withSizeKeepingCentre (outer * 0.8f, outer * 0.8f));
    path.setUsingNonZeroWinding (false);
    return path;
}

juce::Path chevrons (juce::Rectangle<float> area)
{
    const auto x = area.getCentreX();
    const auto halfWidth = area.getWidth() * 0.5f;
    const auto top = area.getY();
    const auto bottom = area.getBottom();
    const auto height = area.getHeight() * 0.36f;

    juce::Path path;
    path.startNewSubPath (x - halfWidth, top + height);
    path.lineTo (x, top);
    path.lineTo (x + halfWidth, top + height);
    path.startNewSubPath (x - halfWidth, bottom - height);
    path.lineTo (x, bottom);
    path.lineTo (x + halfWidth, bottom - height);
    return path;
}

} // namespace icons

} // namespace fancy::ui
