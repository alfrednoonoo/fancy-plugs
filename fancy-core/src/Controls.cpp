#include <fancy/ui/Controls.h>

#include <numbers>

namespace fancy::ui
{

namespace
{
constexpr float arcStart = -0.75f * std::numbers::pi_v<float>;
constexpr float arcRange = 1.5f * std::numbers::pi_v<float>;
constexpr float fineDragDivisor = 8.0f;
constexpr float wheelStep = 0.1f;

void paintPill (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (area, 3.0f);
}

void paintDropdown (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text)
{
    paintPill (g, bounds, colours::grey3);

    g.setFont (font (Weight::regular, 12.0f));
    g.setColour (colours::textPrimary);
    g.drawText (text, bounds.withTrimmedLeft (6.0f).withTrimmedRight (15.0f), juce::Justification::centredLeft, true);

    const auto area = juce::Rectangle<float> (6.0f, 9.0f)
                          .withCentre ({ bounds.getRight() - 8.0f, bounds.getCentreY() });
    g.setColour (colours::textSecondary);
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
    : ParamControl (parameter), labelText (label), accentColour (accent), knobStyle (style), isBipolar (bipolar)
{
}

juce::Rectangle<int> Knob::getValueEditorBounds() const
{
    return getLocalBounds().removeFromBottom (18).withSizeKeepingCentre (juce::jmin (getWidth(), 70), 18);
}

void Knob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto faceRadius = knobStyle.faceDiameter * 0.5f;
    const auto arcRadius = faceRadius + knobStyle.arcThickness * 0.5f + 0.8f;
    const auto valueAngle = arcStart + getNormalised() * arcRange;

    if (knobStyle.showText)
    {
        g.setFont (font (Weight::medium, knobStyle.labelHeight));
        g.setColour (colours::textPrimary);
        g.drawText (labelText, bounds.withHeight (16.0f), juce::Justification::centred, false);

        g.setFont (font (Weight::regular, knobStyle.valueHeight));
        g.setColour (accentColour);
        g.drawText (getValueText(), bounds.withTop (bounds.getBottom() - 16.0f), juce::Justification::centred, false);
    }

    const juce::PathStrokeType stroke (knobStyle.arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, arcStart, arcStart + arcRange, true);
    g.setColour (colours::knobTrack);
    g.strokePath (track, stroke);

    const auto litFrom = isBipolar ? arcStart + 0.5f * arcRange : arcStart;

    if (std::abs (valueAngle - litFrom) > 0.001f)
    {
        juce::Path lit;
        lit.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, litFrom, valueAngle, true);
        g.setColour (accentColour);
        g.strokePath (lit, stroke);
    }

    g.setGradientFill (juce::ColourGradient (colours::knobTop, centre.x, centre.y - faceRadius,
                                             colours::knobBottom, centre.x, centre.y + faceRadius, false));
    g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius, knobStyle.faceDiameter, knobStyle.faceDiameter);

    // An equilateral triangle pointing outwards along the value angle.
    const auto side = knobStyle.triangleSide;
    const auto height = side * 0.866f;

    juce::Path triangle;
    triangle.addTriangle (0.0f, -height * 0.667f, -side * 0.5f, height * 0.333f, side * 0.5f, height * 0.333f);

    g.setColour (accentColour);
    g.fillPath (triangle, juce::AffineTransform::translation (0.0f, -knobStyle.triangleDistance)
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
    return getLocalBounds().removeFromBottom (18);
}

void Fader::paint (juce::Graphics& g)
{
    constexpr float capHeight = 9.0f;
    constexpr float capWidth = 14.0f;
    constexpr float trackWidth = 4.0f;
    constexpr float readoutHeight = 16.0f;

    auto bounds = getLocalBounds().toFloat();
    const auto readout = bounds.removeFromBottom (readoutHeight);
    const auto track = bounds.withSizeKeepingCentre (trackWidth, bounds.getHeight() - capHeight);
    const auto capY = track.getBottom() - getNormalised() * track.getHeight();

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (track, 1.5f);

    g.setColour (accentColour);
    g.fillRoundedRectangle (track.withTop (capY), 1.5f);

    const auto cap = juce::Rectangle<float> (capWidth, capHeight).withCentre ({ track.getCentreX(), capY });
    g.setColour (colours::grey6);
    g.fillRoundedRectangle (cap, 1.5f);
    g.setColour (accentColour);
    g.fillRect (cap.withSizeKeepingCentre (capWidth - 4.0f, 1.5f));

    g.setFont (font (Weight::regular, 11.5f));
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
    constexpr float size = 13.0f;

    const auto bounds = getLocalBounds().toFloat();
    const auto box = bounds.withWidth (size).withSizeKeepingCentre (size, size);

    if (isOn())
    {
        g.setColour (accentColour);
        g.fillRoundedRectangle (box, 2.5f);

        juce::Path tick;
        tick.startNewSubPath (box.getX() + 3.0f, box.getCentreY());
        tick.lineTo (box.getX() + 5.5f, box.getBottom() - 3.5f);
        tick.lineTo (box.getRight() - 3.0f, box.getY() + 3.5f);
        g.setColour (juce::Colours::white);
        g.strokePath (tick, juce::PathStrokeType (1.6f));
    }
    else
    {
        g.setColour (colours::textSecondary);
        g.drawRoundedRectangle (box.reduced (0.5f), 2.5f, 1.0f);
    }

    g.setFont (font (Weight::regular, 12.5f));
    g.setColour (colours::textPrimary);
    g.drawText (labelText, bounds.withTrimmedLeft (size + 6.0f), juce::Justification::centredLeft, false);
}

// PowerPill -------------------------------------------------------------------

void PowerPill::paint (juce::Graphics& g)
{
    const auto active = ! isOn();
    const auto bounds = getLocalBounds().toFloat();
    const auto ink = active ? colours::bgHeader : colours::textSecondary;

    paintPill (g, bounds, active ? colours::uiTeal : colours::grey5);

    const auto icon = juce::Rectangle<float> (10.0f, 10.0f)
                          .withCentre ({ bounds.getX() + 9.0f, bounds.getCentreY() });
    g.setColour (ink);
    g.strokePath (icons::power (icon), juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (font (Weight::medium, 11.5f));
    g.drawText ("ON", bounds.withTrimmedLeft (15.0f), juce::Justification::centredLeft, false);
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
    paintPill (g, getLocalBounds().toFloat(), colours::grey3);
    g.setFont (font (Weight::regular, buttonText == "+" ? 16.0f : 12.0f));
    g.setColour (colours::textPrimary);
    g.drawText (buttonText, getLocalBounds(), juce::Justification::centred, false);
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
