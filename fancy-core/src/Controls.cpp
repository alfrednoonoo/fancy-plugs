#include <fancy/ui/Controls.h>

#include <numbers>

namespace fancy::ui
{

namespace
{
// 300 degrees of travel from 7 o'clock, as the Figma progress arcs are drawn.
constexpr float arcStart = -5.0f / 6.0f * std::numbers::pi_v<float>;
constexpr float arcRange = 5.0f / 3.0f * std::numbers::pi_v<float>;
constexpr float defaultMarkDiameter = 6.0f;
constexpr float defaultMarkGap = 8.0f;
constexpr float bodyFontSize = 14.0f;
constexpr float fineDragDivisor = 8.0f;
constexpr float wheelStep = 0.1f;

void paintPill (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (area, 3.0f);
}

void paintDropdown (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text)
{
    paintPill (g, bounds, colours::bgBody);

    g.setFont (font (Weight::regular, bodyFontSize));
    g.setColour (colours::textPrimary);
    g.drawText (text, bounds.withTrimmedLeft (8.0f).withTrimmedRight (18.0f), juce::Justification::centredLeft, true);

    const auto area = juce::Rectangle<float> (7.0f, 10.0f)
                          .withCentre ({ bounds.getRight() - 9.5f, bounds.getCentreY() });
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.strokePath (icons::chevrons (area), juce::PathStrokeType (1.0f));
}
} // namespace

// ParamControl ----------------------------------------------------------------

ParamControl::ParamControl (juce::RangedAudioParameter& parameter)
    : param (parameter),
      attachment (parameter, [this] (float newValue)
                  {
                      value = newValue;
                      repaint();
                  })
{
    attachment.sendInitialUpdate();
}

juce::String ParamControl::getValueText() const
{
    return param.getText (getNormalised(), 32);
}

void ParamControl::setDenormalised (float newValue)
{
    attachment.setValueAsCompleteGesture (newValue);
}

void ParamControl::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        showHostMenu();
        return;
    }

    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        showValueEditor();
        return;
    }

    dragging = true;
    lastDragY = event.position.y;
    attachment.beginGesture();
}

void ParamControl::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;

    const auto divisor = event.mods.isShiftDown() ? getDragPixels() * fineDragDivisor : getDragPixels();
    const auto delta = (lastDragY - event.position.y) / divisor;
    lastDragY = event.position.y;

    const auto normalised = juce::jlimit (0.0f, 1.0f, getNormalised() + delta);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (normalised));
}

void ParamControl::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attachment.endGesture();

    dragging = false;
}

void ParamControl::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu())
        setDenormalised (param.convertFrom0to1 (param.getDefaultValue()));
}

void ParamControl::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto step = wheel.deltaY * (event.mods.isShiftDown() ? wheelStep / fineDragDivisor : wheelStep);
    const auto normalised = juce::jlimit (0.0f, 1.0f, getNormalised() + step);
    setDenormalised (param.convertFrom0to1 (normalised));
}

void ParamControl::showHostMenu()
{
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* context = editor->getHostContext())
            if (const auto menu = context->getContextMenuForParameter (&param))
                menu->showNativeMenu (editor->getMouseXYRelative());
}

void ParamControl::showValueEditor()
{
    valueEditor = std::make_unique<juce::TextEditor>();
    valueEditor->setJustification (juce::Justification::centred);
    valueEditor->setFont (font (Weight::regular, 12.0f));
    valueEditor->setText (getValueText(), false);
    valueEditor->setBounds (getValueEditorBounds());
    addAndMakeVisible (*valueEditor);
    valueEditor->selectAll();
    valueEditor->grabKeyboardFocus();

    valueEditor->onReturnKey = [this]
    {
        const auto normalised = param.getValueForText (valueEditor->getText());
        setDenormalised (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised)));
        hideValueEditor();
    };

    valueEditor->onEscapeKey = [this] { hideValueEditor(); };
    valueEditor->onFocusLost = [this] { hideValueEditor(); };
}

// Deferred, because this runs inside the text editor's own callbacks.
void ParamControl::hideValueEditor()
{
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ParamControl> (this)]
    {
        if (safeThis != nullptr)
            safeThis->valueEditor.reset();
    });
}

// ParamClickControl -----------------------------------------------------------

void ParamClickControl::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        ParamControl::mouseDown (event);
    else
        clicked();
}

void ParamClickControl::clicked()
{
    setDenormalised (isOn() ? 0.0f : 1.0f);
}

// Knob ------------------------------------------------------------------------

Knob::Knob (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent, const Style& style, bool bipolar)
    : ParamControl (parameter), labelText (label), accentColour (accent), valueColour (accent), knobStyle (style), isBipolar (bipolar)
{
}

juce::Rectangle<int> Knob::getValueEditorBounds() const
{
    const auto height = (int) knobStyle.valueHeight;
    return getLocalBounds().removeFromBottom (height).withSizeKeepingCentre (juce::jmin (getWidth(), 75), height);
}

void Knob::setShowDefaultMark (bool shouldShow)
{
    showDefaultMark = shouldShow;
    repaint();
}

void Knob::setValueColour (juce::Colour colour)
{
    valueColour = colour;
    repaint();
}

void Knob::setValueSuffix (const juce::String& suffix)
{
    if (suffix != valueSuffix)
    {
        valueSuffix = suffix;
        repaint();
    }
}

void Knob::setStyle (const Style& style)
{
    knobStyle = style;
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto style = knobStyle;
    const auto knobTop = style.showText ? style.labelHeight + style.labelGap : (bounds.getHeight() - style.outerDiameter) * 0.5f;
    const juce::Point<float> centre (bounds.getCentreX(), knobTop + style.outerDiameter * 0.5f);
    const auto outerRadius = style.outerDiameter * 0.5f;
    const auto faceRadius = style.faceDiameter * 0.5f;
    const auto arcRadius = outerRadius - style.arcThickness * 0.5f;
    const auto valueAngle = arcStart + getNormalised() * arcRange;

    if (style.showText)
    {
        const auto weight = style.mediumWeight ? Weight::medium : Weight::regular;

        g.setFont (font (weight, style.fontSize));
        g.setColour (colours::textPrimary);

        if (style.labelHeight > 0.0f)
            g.drawText (labelText, bounds.withHeight (style.labelHeight), juce::Justification::centred, false);

        const auto valueArea = bounds.withTop (knobTop + style.outerDiameter + style.valueGap).withHeight (style.valueHeight);

        if (valueSuffix.isEmpty())
        {
            g.setColour (valueColour);
            g.drawText (getValueText(), valueArea, juce::Justification::centred, false);
        }
        else
        {
            const auto textWidth = [&] (const juce::String& text)
            {
                juce::GlyphArrangement glyphs;
                glyphs.addLineOfText (g.getCurrentFont(), text, 0.0f, 0.0f);
                return glyphs.getBoundingBox (0, -1, true).getWidth();
            };

            const auto value = getValueText() + " ";
            const auto valueWidth = textWidth (value + "|") - textWidth ("|");
            const auto suffixWidth = textWidth (valueSuffix);
            auto x = valueArea.getCentreX() - 0.5f * (valueWidth + suffixWidth);

            g.setColour (valueColour);
            g.drawText (value, valueArea.withX (x).withWidth (valueWidth + 1.0f), juce::Justification::centredLeft, false);

            x += valueWidth;

            g.setColour (colours::textPrimary);
            g.drawText (valueSuffix, valueArea.withX (x).withWidth (suffixWidth + 1.0f), juce::Justification::centredLeft, false);
        }
    }

    if (showDefaultMark)
    {
        const auto defaultAngle = arcStart + param.getDefaultValue() * arcRange;
        const auto mark = centre.getPointOnCircumference (outerRadius + defaultMarkGap, defaultAngle);

        g.setColour (colours::grey6);
        g.fillEllipse (juce::Rectangle<float> (defaultMarkDiameter, defaultMarkDiameter).withCentre (mark));
    }

    const auto litFrom = isBipolar ? arcStart + 0.5f * arcRange : arcStart;

    if (std::abs (valueAngle - litFrom) > 0.001f)
    {
        juce::Path lit;
        lit.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, litFrom, valueAngle, true);
        g.setColour (accentColour);
        g.strokePath (lit, juce::PathStrokeType (style.arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const auto face = juce::Rectangle<float> (style.faceDiameter, style.faceDiameter).withCentre (centre);

    g.setGradientFill (juce::ColourGradient (colours::knobTop, centre.x, centre.y - faceRadius,
                                             colours::knobBottom, centre.x, centre.y + faceRadius, false));
    g.fillEllipse (face);

    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.drawEllipse (face, 1.0f);

    // An equilateral triangle pointing outwards along the value angle.
    const auto side = style.triangleSide;
    const auto height = side * 0.866f;

    juce::Path triangle;
    triangle.addTriangle (0.0f, -height * 0.667f, -side * 0.5f, height * 0.333f, side * 0.5f, height * 0.333f);

    const auto rounded = style.outerDiameter > 30.0f ? triangle.createPathWithRoundedCorners (1.0f) : triangle;

    g.setColour (accentColour);
    g.fillPath (rounded, juce::AffineTransform::translation (0.0f, -style.triangleDistance)
                             .rotated (valueAngle)
                             .translated (centre.x, centre.y));
}

// Fader -----------------------------------------------------------------------

Fader::Fader (juce::RangedAudioParameter& parameter, juce::Colour accent)
    : ParamControl (parameter), accentColour (accent)
{
}

juce::Rectangle<int> Fader::getValueEditorBounds() const
{
    return getLocalBounds().removeFromBottom (23);
}

void Fader::paint (juce::Graphics& g)
{
    // Sizes from the Figma Slider/Vertical component.
    constexpr float handleWidth = 22.0f, handleHeight = 12.0f;
    constexpr float trackWidth = 2.0f, fillWidth = 6.0f;
    constexpr float readoutHeight = 23.0f, readoutGap = 8.0f;

    auto bounds = getLocalBounds().toFloat();
    const auto readout = bounds.removeFromBottom (readoutHeight);
    bounds.removeFromBottom (readoutGap);

    const auto centreX = bounds.getCentreX();
    const auto travel = bounds.reduced (0.0f, handleHeight * 0.5f);
    const auto yFor = [&] (float normalised) { return travel.getBottom() - normalised * travel.getHeight(); };
    const auto handleY = yFor (getNormalised());

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds.withSizeKeepingCentre (trackWidth, bounds.getHeight()), 1.0f);

    g.fillEllipse (juce::Rectangle<float> (defaultMarkDiameter, defaultMarkDiameter)
                       .withCentre ({ centreX + 8.0f, yFor (param.getDefaultValue()) }));

    g.setColour (accentColour);
    g.fillRoundedRectangle (bounds.withSizeKeepingCentre (fillWidth, bounds.getHeight()).withTop (handleY), 2.0f);

    const auto handle = juce::Rectangle<float> (handleWidth, handleHeight).withCentre ({ centreX, handleY });

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (handle, 2.0f);
    g.setColour (accentColour);
    g.drawRoundedRectangle (handle.reduced (0.5f), 2.0f, 1.0f);
    g.fillRoundedRectangle (handle.withSizeKeepingCentre (12.0f, 2.0f), 1.0f);

    g.setFont (font (Weight::regular, bodyFontSize));
    g.setColour (colours::textPrimary);
    g.drawText (getValueText().upToFirstOccurrenceOf (" ", false, false), readout, juce::Justification::centred, false);
}

// ParamCheckbox ---------------------------------------------------------------

ParamCheckbox::ParamCheckbox (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent)
    : ParamClickControl (parameter), labelText (label), accentColour (accent)
{
}

void ParamCheckbox::paint (juce::Graphics& g)
{
    constexpr float size = 14.0f;

    const auto bounds = getLocalBounds().toFloat();
    const auto box = bounds.withWidth (size).withSizeKeepingCentre (size, size);

    if (isOn())
    {
        g.setColour (accentColour);
        g.fillRoundedRectangle (box, 3.0f);

        juce::Path tick;
        tick.startNewSubPath (box.getX() + 3.0f, box.getCentreY() + 0.5f);
        tick.lineTo (box.getX() + 5.75f, box.getBottom() - 3.5f);
        tick.lineTo (box.getRight() - 3.0f, box.getY() + 3.5f);
        g.setColour (colours::bgWell);
        g.strokePath (tick, juce::PathStrokeType (1.5f));
    }
    else
    {
        g.setColour (colours::bgHeader);
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (colours::textSecondary);
        g.drawRoundedRectangle (box.reduced (0.25f), 3.0f, 0.5f);
    }

    g.setFont (font (Weight::regular, bodyFontSize));
    g.setColour (colours::textPrimary);
    g.drawText (labelText, bounds.withTrimmedLeft (size + 6.0f), juce::Justification::centredLeft, false);
}

// PowerPill -------------------------------------------------------------------

void PowerPill::paint (juce::Graphics& g)
{
    const auto active = ! isOn();
    const auto bounds = getLocalBounds().toFloat();
    const auto ink = active ? juce::Colours::black : colours::textSecondary;

    paintPill (g, bounds, active ? colours::uiTeal : colours::grey5);

    // The wide header pads the same content a little further in.
    const auto inset = bounds.getWidth() > 50.0f ? 11.0f : 7.0f;

    g.setColour (ink);
    g.strokePath (icons::power (juce::Rectangle<float> (bounds.getX() + inset, bounds.getY() + 6.0f, 11.0f, 12.0f)),
                  juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (font (Weight::regular, bodyFontSize));
    g.drawText ("ON", bounds.withTrimmedLeft (inset + 15.0f), juce::Justification::centredLeft, false);
}

// ParamDropdown ---------------------------------------------------------------

void ParamDropdown::paint (juce::Graphics& g)
{
    paintDropdown (g, getLocalBounds().toFloat(), getValueText());
}

void ParamDropdown::clicked()
{
    const auto choices = param.getAllValueStrings();
    const auto last = juce::jmax (1, choices.size() - 1);

    juce::PopupMenu menu;

    for (int i = 0; i < choices.size(); ++i)
        menu.addItem (i + 1, choices[i], true, choices[i] == getValueText());

    menu.showMenuAsync (juce::PopupMenu::Options {}.withTargetComponent (this),
                        [safeThis = juce::Component::SafePointer<ParamDropdown> (this), last] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->setDenormalised (safeThis->param.convertFrom0to1 ((float) (result - 1) / (float) last));
                        });
}

// PillButton ------------------------------------------------------------------

PillButton::PillButton (const juce::String& text)
    : buttonText (text)
{
}

void PillButton::setText (const juce::String& text)
{
    if (text != buttonText)
    {
        buttonText = text;
        repaint();
    }
}

void PillButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    paintPill (g, bounds, colours::bgBody);

    g.setFont (font (Weight::regular, bodyFontSize));
    g.setColour (colours::textPrimary);

    if (! buttonText.startsWithChar ('+'))
    {
        g.drawText (buttonText, bounds, juce::Justification::centred, false);
        return;
    }

    const auto label = buttonText.substring (1).trim();
    const auto iconCentre = label.isEmpty() ? bounds.getCentre() : juce::Point<float> (bounds.getX() + 14.0f, bounds.getCentreY());

    g.strokePath (icons::plus (juce::Rectangle<float> (10.0f, 10.0f).withCentre (iconCentre)), juce::PathStrokeType (1.5f));
    g.drawText (label, bounds.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
}

void PillButton::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// PillDropdown ----------------------------------------------------------------

void PillDropdown::setText (const juce::String& text)
{
    if (text != dropdownText)
    {
        dropdownText = text;
        repaint();
    }
}

void PillDropdown::paint (juce::Graphics& g)
{
    paintDropdown (g, getLocalBounds().toFloat(), dropdownText);
}

void PillDropdown::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

} // namespace fancy::ui
