#include "IrDisplay.h"

#include <fancy/ui/Style.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
// Figma's empty display: 0.2 s every 96.2 px on a 734 px axis.
constexpr double emptyWindowSeconds = 1.526;
constexpr float grabDistance = 6.0f;
constexpr float waveCentreY = 96.0f, waveHalfHeight = 67.0f;
constexpr float axisTextHeight = 14.0f, axisTextBottom = 4.0f;
constexpr float minTrimSeconds = 0.001f;

double tickStep (double window)
{
    for (const auto step : { 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0 })
        if (window / step < 8.0)
            return step;

    return 20.0;
}
} // namespace

void IrDisplay::setPreview (verb::IrPreview newPreview, double trimStartSeconds, double trimEndSeconds)
{
    preview = std::move (newPreview);
    trimStart = trimStartSeconds;
    trimEnd = trimEndSeconds;
    repaint();
}

double IrDisplay::windowSeconds() const noexcept
{
    return preview.fullSeconds > 0.0 ? preview.fullSeconds : emptyWindowSeconds;
}

float IrDisplay::xOf (double seconds) const noexcept
{
    return (float) (seconds / windowSeconds()) * (float) getWidth();
}

double IrDisplay::secondsAt (float x) const noexcept
{
    return juce::jlimit (0.0, windowSeconds(), (double) x / (double) getWidth() * windowSeconds());
}

double IrDisplay::endSeconds() const noexcept
{
    return trimEnd > 0.0 ? juce::jmin (trimEnd, preview.fullSeconds) : preview.fullSeconds;
}

IrDisplay::Handle IrDisplay::handleAt (float x) const noexcept
{
    if (preview.columns.empty())
        return Handle::none;

    if (std::abs (x - xOf (trimStart)) <= grabDistance)
        return Handle::start;

    if (std::abs (x - xOf (endSeconds())) <= grabDistance)
        return Handle::end;

    return Handle::none;
}

void IrDisplay::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (handleAt (event.position.x) == Handle::none ? juce::MouseCursor::NormalCursor
                                                                : juce::MouseCursor::LeftRightResizeCursor);
}

void IrDisplay::mouseDown (const juce::MouseEvent& event)
{
    dragged = handleAt (event.position.x);
}

void IrDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (dragged == Handle::none)
        return;

    const auto seconds = secondsAt (event.position.x);

    if (dragged == Handle::start)
        trimStart = juce::jlimit (0.0, juce::jmax (0.0, endSeconds() - (double) minTrimSeconds), seconds);
    else
        trimEnd = juce::jlimit (trimStart + (double) minTrimSeconds, preview.fullSeconds, seconds);

    repaint();

    if (onTrimChanged != nullptr)
        onTrimChanged (trimStart, trimEnd);
}

void IrDisplay::mouseUp (const juce::MouseEvent&)
{
    dragged = Handle::none;
}

void IrDisplay::paint (juce::Graphics& g)
{
    const auto width = (float) getWidth();
    const auto height = (float) getHeight();

    g.fillAll (colours::bgHeader);

    const auto window = windowSeconds();
    const auto step = tickStep (window);

    g.setFont (font (Weight::medium, 12.0f));

    for (auto tick = 1; tick * step < window; ++tick)
    {
        const auto x = xOf (tick * step);

        g.setColour (colours::grey5);
        g.fillRect (x - 0.25f, 0.0f, 0.5f, height);

        g.setColour (colours::textSecondary);
        g.drawText (juce::String (tick * step, step < 0.1 ? 2 : 1),
                    juce::Rectangle<float> (x - 44.0f, height - axisTextHeight - axisTextBottom, 40.0f, axisTextHeight),
                    juce::Justification::centredRight, false);
    }

    if (preview.columns.empty())
        return;

    auto peak = 0.0f;

    for (const auto& column : preview.columns)
        peak = juce::jmax (peak, std::abs (column.low), std::abs (column.high));

    const auto scale = peak > 0.0f ? waveHalfHeight / peak : 0.0f;
    const auto columns = (int) preview.columns.size();
    const auto toX = [&] (int column) { return width * (float) column / (float) (columns - 1); };

    juce::Path outer, inner;

    outer.startNewSubPath (0.0f, waveCentreY);
    inner.startNewSubPath (0.0f, waveCentreY);

    for (int i = 0; i < columns; ++i)
    {
        const auto& column = preview.columns[(size_t) i];
        outer.lineTo (toX (i), waveCentreY - column.high * scale);
        inner.lineTo (toX (i), waveCentreY - column.rms * scale);
    }

    for (int i = columns; --i >= 0;)
    {
        const auto& column = preview.columns[(size_t) i];
        outer.lineTo (toX (i), waveCentreY - column.low * scale);
        inner.lineTo (toX (i), waveCentreY + column.rms * scale);
    }

    outer.closeSubPath();
    inner.closeSubPath();

    g.setColour (colours::uiPurple.withMultipliedBrightness (0.72f));
    g.fillPath (outer);
    g.setColour (colours::uiPurple);
    g.fillPath (inner);

    g.setColour (colours::accentGreen.withAlpha (0.8f));

    for (const auto seconds : preview.echoSeconds)
        g.fillRect (xOf (seconds), 0.0f, 1.0f, height);

    const auto startX = xOf (trimStart);
    const auto endX = xOf (endSeconds());

    g.setColour (colours::bgHeader.withAlpha (0.7f));
    g.fillRect (0.0f, 0.0f, startX, height);
    g.fillRect (endX, 0.0f, width - endX, height);

    g.setColour (colours::textSecondary);

    for (const auto [x, direction] : { std::pair { startX, 1.0f }, std::pair { juce::jmin (endX, width - 1.0f), -1.0f } })
    {
        g.fillRect (x, 0.0f, 1.0f, height);

        juce::Path grip;
        grip.addTriangle (x, 0.0f, x + direction * 8.0f, 0.0f, x, 10.0f);
        g.fillPath (grip);
    }

    const auto rateKhz = preview.sampleRate / 1000.0;
    const auto rateText = juce::approximatelyEqual (rateKhz, std::round (rateKhz)) ? juce::String (juce::roundToInt (rateKhz))
                                                                                  : juce::String (rateKhz, 1);

    const auto info = juce::String (preview.trimmedSeconds, 1) + "s, " + rateText + "kHz, "
                    + juce::String (preview.numChannels) + "ch, max: " + juce::String (preview.peakDb, 1) + "dB";

    g.setFont (font (Weight::medium, 16.0f));
    g.setColour (colours::textSecondary);
    g.drawText (info, getLocalBounds().toFloat().withTrimmedRight (16.0f).withTrimmedTop (8.0f).withHeight (19.0f),
                juce::Justification::centredRight, false);
}
