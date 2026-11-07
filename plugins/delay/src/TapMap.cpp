#include "TapMap.h"

#include <fancy/ui/Style.h>

#include <cmath>

using namespace fancy::ui;

namespace
{
constexpr float leftInset = 30.0f, rightInset = 14.0f, topInset = 10.0f, baselineInset = 26.0f;
constexpr float barWidth = 8.0f, handleRadius = 8.5f, grabRadius = 13.0f;
constexpr float repeatAlpha = 0.45f;
constexpr double smallestRepeatGain = 0.03;
constexpr int maxRepeats = 24;
} // namespace

juce::Colour tapColour (int tap)
{
    const juce::Colour palette[] { colours::accentYellow, colours::accentOrange, colours::accentPurple,
                                   colours::uiEqBlue, colours::uiEqGreen, colours::accentRed };

    return palette[tap % (int) std::size (palette)];
}

TapMap::TapMap (DelayProcessor& processor)
    : delayProcessor (processor)
{
    update();
}

void TapMap::setSelectedTap (int tap)
{
    selected = tap;
    repaint();
}

void TapMap::update()
{
    targets = delayProcessor.readTargets();
    repaint();
}

int TapMap::beatsForLongest (double longestMs, double bpm)
{
    return juce::jmax (4, (int) std::ceil (longestMs / delay::beatMs (bpm)));
}

double TapMap::axisMs() const
{
    // Held during a drag, or the axis would stretch away under the mouse.
    if (dragged >= 0)
        return fixedAxisMs;

    auto longest = 0.0;

    for (const auto& spec : targets.taps)
        if (spec.used)
            longest = juce::jmax (longest, DelayDsp::tapMs (spec, targets.bpm));

    return beatsForLongest (longest, targets.bpm) * delay::beatMs (targets.bpm);
}

float TapMap::xForMs (double ms) const
{
    return leftInset + (float) (ms / axisMs()) * ((float) getWidth() - leftInset - rightInset);
}

double TapMap::msForX (float x) const
{
    return (double) ((x - leftInset) / ((float) getWidth() - leftInset - rightInset)) * axisMs();
}

float TapMap::yForVolume (float normalised) const
{
    const auto baseline = (float) getHeight() - baselineInset;
    return baseline - normalised * (baseline - topInset - handleRadius * 2.0f);
}

float TapMap::volumeForY (float y) const
{
    const auto baseline = (float) getHeight() - baselineInset;
    return juce::jlimit (0.0f, 1.0f, (baseline - y) / (baseline - topInset - handleRadius * 2.0f));
}

float TapMap::volumeOf (int tap) const
{
    return delayProcessor.getApvts().getParameter (DelayProcessor::id (tap, "vol"))->getValue();
}

juce::Point<float> TapMap::handleOf (int tap) const
{
    const auto& spec = targets.taps[(size_t) tap];

    return { xForMs (DelayDsp::tapMs (spec, targets.bpm)), yForVolume (volumeOf (tap)) - handleRadius };
}

int TapMap::tapAt (juce::Point<float> position) const
{
    auto best = -1;
    auto bestDistance = grabRadius;

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        if (! targets.taps[(size_t) tap].used)
            continue;

        const auto distance = handleOf (tap).getDistanceFrom (position);

        if (distance <= bestDistance)
        {
            best = tap;
            bestDistance = distance;
        }
    }

    return best;
}

void TapMap::paintRepeats (juce::Graphics& g, int tap, const DelayDsp::TapSpec& spec) const
{
    const auto ms = DelayDsp::tapMs (spec, targets.bpm);
    const auto feedback = juce::jmin (spec.feedback, spec.saturate ? 1.1 : 1.0);

    if (feedback <= 0.0 || ms <= 0.0)
        return;

    const auto baseline = (float) getHeight() - baselineInset;
    const auto height = baseline - yForVolume (volumeOf (tap));

    g.setColour (tapColour (tap).withAlpha (repeatAlpha));

    auto level = feedback;

    for (int repeat = 2; repeat < maxRepeats && level > smallestRepeatGain; ++repeat)
    {
        const auto position = ms * (double) repeat;

        if (position > axisMs())
            break;

        const auto barHeight = height * (float) juce::jmin (1.0, level);

        g.fillRoundedRectangle (xForMs (position) - barWidth * 0.5f, baseline - barHeight, barWidth, barHeight, 2.0f);
        level *= feedback;
    }
}

void TapMap::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto baseline = bounds.getHeight() - baselineInset;
    const auto right = bounds.getWidth() - rightInset;
    const auto beat = delay::beatMs (targets.bpm);
    const auto beats = juce::roundToInt (axisMs() / beat);
    const auto beatStep = juce::jmax (1, beats / 4);

    g.setColour (colours::bgBody);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setFont (font (Weight::regular, 11.0f));

    for (int tick = 0; tick <= beats * 4; ++tick)
    {
        const auto x = xForMs (beat * tick * 0.25);
        const auto isBeat = tick % 4 == 0;
        const auto isMajor = isBeat && (tick / 4) % beatStep == 0;

        g.setColour (colours::grey6);

        if (isMajor && tick > 0 && tick < beats * 4)
            g.fillRect (x - 0.5f, topInset, 1.0f, baseline - topInset);
        else if (beats <= 8 || isBeat)
            g.fillRect (x - 0.5f, baseline - 5.0f, 1.0f, 5.0f);

        if (isMajor)
        {
            g.setColour (colours::textSecondary);

            // The ends line up with the axis, so they can't run off the card.
            const auto isFirst = tick == 0, isLast = tick == beats * 4;
            const auto area = isFirst ? juce::Rectangle<float> (x - 4.0f, baseline + 5.0f, 72.0f, 16.0f)
                                      : (isLast ? juce::Rectangle<float> (x - 72.0f, baseline + 5.0f, 72.0f, 16.0f)
                                                : juce::Rectangle<float> (x - 36.0f, baseline + 5.0f, 72.0f, 16.0f));

            g.drawText (juce::String (juce::roundToInt (beat * tick * 0.25)) + " ms", area,
                        isFirst ? juce::Justification::centredLeft : (isLast ? juce::Justification::centredRight : juce::Justification::centred), false);
        }
    }

    g.setColour (colours::grey6);
    g.fillRect (leftInset, baseline - 0.5f, right - leftInset, 1.0f);

    // Dry, always at time zero.
    const auto dryParameter = delayProcessor.getApvts().getParameter ("dry");
    const auto dryTop = yForVolume (dryParameter->getValue());

    g.setColour (colours::textPrimary.withAlpha (0.85f));
    g.fillRoundedRectangle (leftInset - barWidth * 0.5f, dryTop, barWidth, baseline - dryTop, 2.0f);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 12.0f));
    g.drawText ("dry", juce::Rectangle<float> (leftInset + 7.0f, topInset - 2.0f, 40.0f, 16.0f), juce::Justification::centredLeft, false);
    g.drawText ("faded bars = feedback repeats", juce::Rectangle<float> (right - 190.0f, topInset - 2.0f, 190.0f, 16.0f),
                juce::Justification::centredRight, false);

    for (int tap = 0; tap < delay::maxTaps; ++tap)
        if (targets.taps[(size_t) tap].used)
            paintRepeats (g, tap, targets.taps[(size_t) tap]);

    for (int tap = 0; tap < delay::maxTaps; ++tap)
    {
        const auto& spec = targets.taps[(size_t) tap];

        if (! spec.used)
            continue;

        const auto colour = tapColour (tap).withMultipliedAlpha (spec.on ? 1.0f : 0.4f);
        const auto handle = handleOf (tap);
        const auto top = handle.y + handleRadius;

        g.setColour (colour);
        g.fillRoundedRectangle (handle.x - barWidth * 0.5f, top, barWidth, baseline - top, 2.0f);

        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (handle);

        if (tap == selected)
        {
            g.setColour (colour);
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (colours::textPrimary);
            g.fillEllipse (circle);
            g.setColour (colour);
            g.drawEllipse (circle.reduced (1.0f), 2.0f);
        }

        g.setColour (colours::bgWell);
        g.setFont (font (Weight::semiBold, 11.0f));
        g.drawText (juce::String (tap + 1), circle, juce::Justification::centred, false);
    }
}

void TapMap::mouseDown (const juce::MouseEvent& event)
{
    const auto tap = tapAt (event.position);

    if (tap < 0)
        return;

    fixedAxisMs = axisMs();
    dragged = tap;

    if (onTapSelected != nullptr)
        onTapSelected (tap);
}

void TapMap::mouseDrag (const juce::MouseEvent& event)
{
    if (dragged < 0)
        return;

    const auto ms = juce::jlimit (0.0, delay::maxDelayMs, msForX (event.position.x));
    const auto spec = delayProcessor.readTap (dragged);
    auto& processor = delayProcessor;

    if (spec.noteMode && ! event.mods.isShiftDown())
    {
        const auto length = delay::nearestNote (ms, targets.bpm);

        processor.setParameter (DelayProcessor::id (dragged, "note"), (float) length.note);
        processor.setParameter (DelayProcessor::id (dragged, "feel"), (float) length.feel);
        processor.setParameter (DelayProcessor::id (dragged, "mult"), (float) length.multiplier - 1.0f);
    }
    else
    {
        // Shift leaves the note grid, and a time off the grid is an ms time.
        processor.setParameter (DelayProcessor::id (dragged, "ms"), (float) ms);
        processor.setParameter (DelayProcessor::id (dragged, "mode"), 0.0f);
    }

    auto& volume = *delayProcessor.getApvts().getParameter (DelayProcessor::id (dragged, "vol"));

    volume.beginChangeGesture();
    volume.setValueNotifyingHost (volumeForY (event.position.y + handleRadius));
    volume.endChangeGesture();

    update();
}

void TapMap::mouseUp (const juce::MouseEvent&)
{
    dragged = -1;
    update();
}
