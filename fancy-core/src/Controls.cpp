#include <fancy/ui/Controls.h>

#include <fancy/plugin/ParamFormat.h>

#include <cmath>
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
constexpr int valueEditorWidth = 76;
constexpr int valueEditorMinHeight = 22;

void paintPill (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (area, 3.0f);
}

void paintDropdown (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text, float textHeight = bodyFontSize)
{
    paintPill (g, bounds, colours::bgBody);

    g.setFont (font (Weight::regular, textHeight));
    g.setColour (colours::textPrimary);
    g.drawText (text, bounds.withTrimmedLeft (8.0f).withTrimmedRight (18.0f), juce::Justification::centredLeft, true);

    const auto area = juce::Rectangle<float> (7.0f, 10.0f)
                          .withCentre ({ bounds.getRight() - 9.5f, bounds.getCentreY() });
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.strokePath (icons::chevrons (area), juce::PathStrokeType (1.0f));
}

// Clicking a plain component leaves keyboard focus where it was, so losing focus alone
// wouldn't close the box. This watches every click in the plugin while the box is open.
class ValueEditor final : public juce::TextEditor
{
public:
    ValueEditor() { juce::Desktop::getInstance().addGlobalMouseListener (&clickWatcher); }
    ~ValueEditor() override { juce::Desktop::getInstance().removeGlobalMouseListener (&clickWatcher); }

    std::function<void()> onClickAway;

private:
    struct ClickWatcher final : public juce::MouseListener
    {
        explicit ClickWatcher (ValueEditor& editorToWatch) : editor (editorToWatch) {}

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (event.eventComponent != &editor && ! editor.isParentOf (event.eventComponent) && editor.onClickAway != nullptr)
                editor.onClickAway();
        }

        ValueEditor& editor;
    };

    ClickWatcher clickWatcher { *this };
};

// The box goes on the component holding the plugin's layout, so it can be wider than a fader
// and still scales with the UI size menu.
juce::Component& valueEditorParent (juce::Component& control)
{
    auto* holder = &control;

    while (auto* parent = holder->getParentComponent())
    {
        if (dynamic_cast<juce::AudioProcessorEditor*> (parent) != nullptr)
            break;

        holder = parent;
    }

    return *holder;
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

    dragging = true;
    clickOpensEditor = event.mods.isCtrlDown() || event.mods.isCommandDown() || getValueArea().contains (event.getPosition());
    lastDragY = event.position.y;
    dragPosition = getNormalised();
    attachment.beginGesture();
}

void ParamControl::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;

    // Until the mouse has moved far enough to be a drag, this may still be a click to type.
    if (clickOpensEditor && ! event.mouseWasDraggedSinceMouseDown())
        return;

    const auto divisor = event.mods.isShiftDown() ? getDragPixels() * fineDragDivisor : getDragPixels();
    const auto delta = (lastDragY - event.position.y) / divisor;
    lastDragY = event.position.y;

    // Kept apart from the value, which snaps to the parameter's steps. Adding to that instead
    // would round every small move away on a control with coarse steps.
    dragPosition = juce::jlimit (0.0f, 1.0f, dragPosition + delta);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (dragPosition));
}

void ParamControl::mouseUp (const juce::MouseEvent& event)
{
    if (! dragging)
        return;

    attachment.endGesture();
    dragging = false;

    if (clickOpensEditor && ! event.mouseWasDraggedSinceMouseDown())
        showValueEditor();
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
    // A parameter the host doesn't know about has no automation menu.
    if (param.getParameterIndex() < 0)
        return;

    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* context = editor->getHostContext())
            if (const auto menu = context->getContextMenuForParameter (&param))
                menu->showNativeMenu (editor->getMouseXYRelative());
}

void ParamControl::showValueEditor()
{
    if (valueEditor != nullptr)
        return;

    auto& holder = valueEditorParent (*this);
    const auto valueArea = getValueArea();
    const auto area = holder.getLocalArea (this, valueArea.isEmpty() ? getLocalBounds() : valueArea);

    textBeforeEditing = getValueText();

    auto editor = std::make_unique<ValueEditor>();
    editor->setJustification (juce::Justification::centred);
    editor->setFont (font (Weight::regular, 12.0f));
    editor->setText (textBeforeEditing, false);
    editor->setPopupMenuEnabled (false);
    editor->setSelectAllWhenFocused (true);
    editor->setBounds (area.withSizeKeepingCentre (valueEditorWidth, juce::jmax (area.getHeight(), valueEditorMinHeight))
                           .constrainedWithin (holder.getLocalBounds()));

    editor->onReturnKey = [this] { closeValueEditor (true); };
    editor->onEscapeKey = [this] { closeValueEditor (false); };
    editor->onFocusLost = [this] { closeValueEditor (true); };
    editor->onClickAway = [this] { closeValueEditor (true); };

    holder.addAndMakeVisible (*editor);
    editor->selectAll();
    editor->grabKeyboardFocus();

    valueEditor = std::move (editor);
}

void ParamControl::closeValueEditor (bool applyText)
{
    if (valueEditor == nullptr || valueEditorClosing)
        return;

    valueEditorClosing = true;

    const auto text = valueEditor->getText();
    valueEditor->setVisible (false);

    // Deferred, because this runs inside the text editor's own callbacks.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ParamControl> (this)]
    {
        if (safeThis != nullptr)
        {
            safeThis->valueEditor.reset();
            safeThis->valueEditorClosing = false;
        }
    });

    if (applyText)
        applyTypedText (text);
}

void ParamControl::applyTypedText (const juce::String& text)
{
    // The box shows a rounded value, so applying it unchanged would move the control.
    if (text.trim() == textBeforeEditing || ! isValueText (text))
        return;

    const auto normalised = param.getValueForText (text.trim());

    if (std::isfinite (normalised))
        setDenormalised (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised)));
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

juce::Rectangle<int> Knob::getValueArea() const
{
    if (! knobStyle.showText)
        return {};

    const auto top = knobStyle.labelHeight + knobStyle.labelGap + knobStyle.outerDiameter + knobStyle.valueGap;
    return getLocalBounds().withTop (juce::roundToInt (top)).withHeight (juce::roundToInt (knobStyle.valueHeight));
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

void Knob::setValueSuffix (const juce::String& suffix, juce::Colour colour)
{
    if (suffix != valueSuffix || colour != suffixColour)
    {
        valueSuffix = suffix;
        suffixColour = colour;
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

            g.setColour (suffixColour);
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

juce::Rectangle<int> Fader::getValueArea() const
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

// ParamSegments ---------------------------------------------------------------

ParamSegments::ParamSegments (juce::RangedAudioParameter& parameter, juce::Colour selectedFill, juce::Colour selectedText)
    : ParamClickControl (parameter),
      numSegments (juce::jmax (1, parameter.getAllValueStrings().size())),
      fillColour (selectedFill),
      textColour (selectedText)
{
}

void ParamSegments::setSelectedColours (juce::Colour selectedFill, juce::Colour selectedText)
{
    fillColour = selectedFill;
    textColour = selectedText;
    repaint();
}

void ParamSegments::setTextHeight (float height)
{
    textHeight = height;
    repaint();
}

void ParamSegments::setFitToText (bool shouldFit)
{
    fitToText = shouldFit;
    repaint();
}

int ParamSegments::getSelected() const
{
    return juce::jlimit (0, numSegments - 1, juce::roundToInt (getNormalised() * (float) (numSegments - 1)));
}

juce::Rectangle<float> ParamSegments::segmentBounds (int index) const
{
    const auto area = getLocalBounds().toFloat().reduced (3.0f);

    if (! fitToText)
    {
        const auto width = area.getWidth() / (float) numSegments;
        return area.withX (area.getX() + (float) index * width).withWidth (width);
    }

    // Each segment gets its text plus an equal share of what's left.
    const auto choices = param.getAllValueStrings();
    const auto textFont = font (Weight::regular, textHeight);

    const auto widthOf = [&] (const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (textFont, text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth();
    };

    auto total = 0.0f, before = 0.0f;

    for (int i = 0; i < numSegments; ++i)
    {
        const auto width = widthOf (choices[i]);
        total += width;

        if (i < index)
            before += width;
    }

    const auto padding = juce::jmax (0.0f, (area.getWidth() - total) / (float) numSegments);

    return area.withX (area.getX() + before + padding * (float) index).withWidth (widthOf (choices[index]) + padding);
}

void ParamSegments::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        ParamClickControl::mouseDown (event);
        return;
    }

    for (int i = 0; i < numSegments; ++i)
        if (segmentBounds (i).contains (event.position))
            setDenormalised (param.convertFrom0to1 ((float) i / (float) juce::jmax (1, numSegments - 1)));
}

void ParamSegments::paint (juce::Graphics& g)
{
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    const auto selected = getSelected();

    for (int i = 0; i < numSegments; ++i)
    {
        const auto area = segmentBounds (i);

        if (i == selected)
        {
            g.setColour (fillColour);
            g.fillRoundedRectangle (area, 3.0f);
        }

        paintSegment (g, i, area, i == selected ? textColour : colours::textSecondary);
    }
}

void ParamSegments::paintSegment (juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour ink)
{
    g.setFont (font (Weight::regular, textHeight));
    g.setColour (ink);
    g.drawText (param.getAllValueStrings()[index], area, juce::Justification::centred, false);
}

// ParamTextToggle -------------------------------------------------------------

ParamTextToggle::ParamTextToggle (juce::RangedAudioParameter& parameter, const juce::String& text, juce::Colour accent)
    : ParamClickControl (parameter), buttonText (text), colour (accent)
{
}

void ParamTextToggle::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (isOn() ? colour : colours::bgSection);
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (isOn() ? colours::bgWell : colours::textPrimary);
    g.setFont (font (Weight::regular, bodyFontSize));
    g.drawText (buttonText, bounds, juce::Justification::centred, false);
}

// ParamPowerIcon --------------------------------------------------------------

ParamPowerIcon::ParamPowerIcon (juce::RangedAudioParameter& parameter, juce::Colour accent)
    : ParamClickControl (parameter), colour (accent)
{
}

void ParamPowerIcon::paint (juce::Graphics& g)
{
    g.setColour (isOn() ? colour : colours::textSecondary);
    g.strokePath (icons::power (getLocalBounds().toFloat()), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// IconButton ------------------------------------------------------------------

IconButton::IconButton (Glyph glyph)
    : iconGlyph (glyph)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plus = iconGlyph == Glyph::plus;

    juce::Path path;

    if (plus)
    {
        path.startNewSubPath (bounds.getCentreX(), bounds.getY());
        path.lineTo (bounds.getCentreX(), bounds.getBottom());
        path.startNewSubPath (bounds.getX(), bounds.getCentreY());
        path.lineTo (bounds.getRight(), bounds.getCentreY());
    }
    else
    {
        path.startNewSubPath (bounds.getTopLeft());
        path.lineTo (bounds.getBottomRight());
        path.startNewSubPath (bounds.getTopRight());
        path.lineTo (bounds.getBottomLeft());
    }

    g.setColour (plus ? colours::textPrimary : colours::textSecondary);
    g.strokePath (path, juce::PathStrokeType (2.0f));
}

void IconButton::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// ColourTabs ------------------------------------------------------------------

namespace
{
constexpr int colourTabWidth = 22, colourTabGap = 6;
}

void ColourTabs::setTabs (std::vector<Tab> newTabs, int selectedId)
{
    tabs = std::move (newTabs);
    selected = selectedId;
    repaint();
}

juce::Rectangle<int> ColourTabs::tabBounds (int index) const
{
    return { index * (colourTabWidth + colourTabGap), 0, colourTabWidth, getHeight() };
}

int ColourTabs::getEndX() const
{
    return getX() + tabBounds ((int) tabs.size() + (showAddButton ? 1 : 0)).getX() - colourTabGap;
}

void ColourTabs::paint (juce::Graphics& g)
{
    g.setFont (font (Weight::regular, bodyFontSize));

    for (int i = 0; i < (int) tabs.size(); ++i)
    {
        const auto& tab = tabs[(size_t) i];
        const auto area = tabBounds (i).toFloat();
        const auto isSelected = tab.id == selected;

        if (isSelected)
        {
            g.setColour (tab.colour);
            g.fillRoundedRectangle (area, 3.0f);
        }
        else
        {
            g.setColour (tab.colour);
            g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
        }

        g.setColour (isSelected && tab.colour.getPerceivedBrightness() > 0.7f ? juce::Colours::black : colours::textPrimary);
        g.drawText (juce::String (tab.id + 1), area, juce::Justification::centred, false);
    }

    if (showAddButton)
    {
        const auto area = tabBounds ((int) tabs.size()).toFloat();

        g.setColour (colours::bgSection);
        g.fillRoundedRectangle (area, 3.0f);

        g.setColour (colours::textPrimary);
        g.strokePath (icons::plus (juce::Rectangle<float> (9.0f, 9.0f).withCentre (area.getCentre())), juce::PathStrokeType (1.5f));
    }
}

void ColourTabs::mouseDown (const juce::MouseEvent& event)
{
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabBounds (i).contains (event.getPosition()) && onSelect != nullptr)
            onSelect (tabs[(size_t) i].id);

    if (showAddButton && tabBounds ((int) tabs.size()).contains (event.getPosition()) && onAdd != nullptr)
        onAdd();
}

// ParamCheckbox ---------------------------------------------------------------

ParamCheckbox::ParamCheckbox (juce::RangedAudioParameter& parameter, const juce::String& label, juce::Colour accent)
    : ParamClickControl (parameter), labelText (label), accentColour (accent)
{
}

void ParamCheckbox::setLabelHeight (float height)
{
    labelHeight = height;
    repaint();
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

    g.setFont (font (Weight::regular, labelHeight));
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

void ParamDropdown::setTextHeight (float height)
{
    textHeight = height;
    repaint();
}

void ParamDropdown::paint (juce::Graphics& g)
{
    paintDropdown (g, getLocalBounds().toFloat(), getValueText(), textHeight);
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
