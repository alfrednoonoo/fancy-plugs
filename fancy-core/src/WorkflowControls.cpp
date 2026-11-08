#include <fancy/ui/WorkflowControls.h>

#include <cmath>

namespace fancy::ui
{

namespace
{
constexpr float fontSize = 14.0f;
constexpr float radius = 3.0f;

const juce::Colour selectedFill { 0xff434343 };
const juce::PathStrokeType iconStroke (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
} // namespace

namespace icons
{
juce::Path undoArrow (juce::Rectangle<float> area)
{
    // An arc over the top from the lower right, ending in an arrowhead at the left.
    const auto r = area.getWidth() * 0.4f;
    const auto centre = area.getCentre().translated (area.getWidth() * 0.08f, area.getHeight() * 0.1f);

    juce::Path path;
    path.addCentredArc (centre.x, centre.y, r, r, 0.0f, juce::MathConstants<float>::pi * 0.8f,
                        -juce::MathConstants<float>::halfPi, true);

    const auto tip = juce::Point<float> (centre.x - r, centre.y + 0.5f);
    const auto head = r * 0.6f;
    path.startNewSubPath (tip.x - head, tip.y - head);
    path.lineTo (tip);
    path.lineTo (tip.x + head, tip.y - head);
    return path;
}

juce::Path redoArrow (juce::Rectangle<float> area)
{
    auto path = undoArrow (area);
    path.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, area.getCentreX(), area.getCentreY()));
    return path;
}

juce::Path rightArrow (juce::Rectangle<float> area)
{
    const auto y = area.getCentreY();
    const auto head = area.getHeight() * 0.35f;

    juce::Path path;
    path.startNewSubPath (area.getX(), y);
    path.lineTo (area.getRight(), y);
    path.startNewSubPath (area.getRight() - head, y - head);
    path.lineTo (area.getRight(), y);
    path.lineTo (area.getRight() - head, y + head);
    return path;
}

juce::Path approx (juce::Rectangle<float> area)
{
    // Two tildes, drawn rather than typed, so the glyph never depends on the font.
    juce::Path path;

    for (const auto y : { area.getY() + area.getHeight() * 0.3f, area.getY() + area.getHeight() * 0.7f })
    {
        const auto x = area.getX(), w = area.getWidth(), a = area.getHeight() * 0.18f;
        path.startNewSubPath (x, y + a * 0.4f);
        path.cubicTo (x + w * 0.3f, y - a * 1.6f, x + w * 0.7f, y + a * 1.6f, x + w, y - a * 0.4f);
    }

    return path;
}
} // namespace icons

// HeaderIconButton ---------------------------------------------------------------------------------

HeaderIconButton::HeaderIconButton (Icon iconToUse) : icon (iconToUse)
{
}

void HeaderIconButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::bgBody);
    g.fillRoundedRectangle (bounds, radius);

    const auto area = juce::Rectangle<float> (12.0f, 10.0f).withCentre (bounds.getCentre());
    const auto path = icon == Icon::undo ? icons::undoArrow (area) : icon == Icon::redo ? icons::redoArrow (area) : icons::rightArrow (area);

    g.setColour (isEnabled() ? colours::menuText : colours::textDisabled);
    g.strokePath (path, iconStroke);
}

void HeaderIconButton::mouseUp (const juce::MouseEvent& event)
{
    if (isEnabled() && onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// AbSwitch ---------------------------------------------------------------------------------------------

AbSwitch::AbSwitch()
{
    setTooltip ("Two complete sets of settings. Click the other letter to switch; both are saved with the project.");
}

void AbSwitch::setActive (int slot)
{
    if (slot != active)
    {
        active = slot;
        repaint();
    }
}

juce::Rectangle<float> AbSwitch::segmentBounds (int slot) const
{
    auto inner = getLocalBounds().toFloat().reduced (2.0f);
    const auto half = (inner.getWidth() - 2.0f) * 0.5f;
    return slot == 0 ? inner.withWidth (half) : inner.withTrimmedLeft (half + 2.0f);
}

void AbSwitch::paint (juce::Graphics& g)
{
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), radius + 1.0f);

    g.setFont (font (Weight::regular, fontSize));

    for (int slot = 0; slot < 2; ++slot)
    {
        const auto area = segmentBounds (slot);

        if (slot == active)
        {
            g.setColour (selectedFill);
            g.fillRoundedRectangle (area, radius);
        }

        g.setColour (slot == active ? colours::textPrimary : colours::menuHint);
        g.drawText (slot == 0 ? "A" : "B", area, juce::Justification::centred, false);
    }
}

void AbSwitch::mouseDown (const juce::MouseEvent& event)
{
    const auto slot = event.position.x < getWidth() * 0.5f ? 0 : 1;

    if (slot != active && onSelect != nullptr)
        onSelect (slot);
}

// CompareButton -------------------------------------------------------------------------------------

CompareButton::CompareButton()
{
    setTooltip ("Compare: hear the input, level-matched to the output, so a change in loudness can't fool you. "
                "Not saved, and separate from ON.");
}

juce::String CompareButton::formatGain (float gainDb)
{
    const auto rounded = std::round (gainDb * 10.0f) / 10.0f;
    return (rounded >= 0.0f ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"))) + juce::String (std::abs (rounded), 1) + " dB";
}

void CompareButton::setState (bool isOn, float gainDb)
{
    if (isOn != on || (isOn && std::abs (gainDb - gain) > 0.01f))
    {
        on = isOn;
        gain = gainDb;
        repaint();
    }
}

void CompareButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (on ? selectedFill : colours::bgBody);
    g.fillRoundedRectangle (bounds, radius);

    if (on)
    {
        g.setColour (colours::uiTeal);
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    }

    const auto ink = on ? colours::textPrimary : colours::menuText;
    const auto text = on ? formatGain (gain) : juce::String ("Compare");
    const auto textFont = font (Weight::regular, fontSize);
    const auto textWidth = juce::GlyphArrangement::getStringWidth (textFont, text);
    const auto iconWidth = 10.0f, gap = 5.0f;
    const auto left = bounds.getCentreX() - (iconWidth + gap + textWidth) * 0.5f;

    g.setColour (on ? colours::uiTeal : ink);
    g.strokePath (icons::approx ({ left, bounds.getCentreY() - 4.0f, iconWidth, 8.0f }), juce::PathStrokeType (1.3f));

    g.setColour (ink);
    g.setFont (textFont);
    g.drawText (text, juce::Rectangle<float> (left + iconWidth + gap, bounds.getY(), textWidth + 2.0f, bounds.getHeight()),
                juce::Justification::centredLeft, false);
}

void CompareButton::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// HeaderSeparator --------------------------------------------------------------------------------------

void HeaderSeparator::paint (juce::Graphics& g)
{
    g.setColour (selectedFill);
    g.fillRect (getLocalBounds().toFloat().withSizeKeepingCentre (1.0f, (float) getHeight()));
}

} // namespace fancy::ui
