#include <fancy/ui/LevelHistory.h>

#include <algorithm>
#include <cmath>

namespace fancy::ui
{

namespace
{
const juce::Colour inputColour { 0xff5a5a5a };
const juce::Colour reductionColour { 0xffffcf07 }; // the GR meter's yellow
const juce::Colour rightReductionColour { 0xffffe88a };
constexpr float waveMargin = 8.0f;
constexpr float reductionMargin = 4.0f;
} // namespace

LevelHistory::LevelHistory()
    : ring ((size_t) maxColumns)
{
    setOpaque (false);
}

double LevelHistory::secondsFor (int zoomIndex) noexcept
{
    return dsp::LevelHistoryRecorder::finestSeconds * (double) (1 << juce::jlimit (0, numZooms - 1, zoomIndex));
}

void LevelHistory::push (const dsp::LevelColumn* newColumns, int count)
{
    if (paused || frozen || count <= 0)
        return;

    for (int i = 0; i < count; ++i)
    {
        ring[(size_t) writeIndex] = newColumns[i];
        writeIndex = (writeIndex + 1) % maxColumns;
    }

    filled = std::min (maxColumns, filled + count);
    pushed += count;
    repaint();
}

const dsp::LevelColumn& LevelHistory::column (int age) const noexcept
{
    return ring[(size_t) ((writeIndex - 1 - age + 2 * maxColumns) % maxColumns)];
}

void LevelHistory::setZoom (int newZoom)
{
    newZoom = juce::jlimit (0, numZooms - 1, newZoom);

    if (newZoom != zoom)
    {
        zoom = newZoom;
        repaint();
    }
}

void LevelHistory::setGrid (bool shouldShow)
{
    if (grid != shouldShow)
    {
        grid = shouldShow;
        repaint();
    }
}

void LevelHistory::setPaused (bool shouldPause)
{
    if (paused == shouldPause)
        return;

    paused = shouldPause;
    repaint();

    if (onPausedChange != nullptr)
        onPausedChange();
}

float LevelHistory::getPeakReduction() const noexcept
{
    // Only the columns in view, and only those since the last clear.
    const auto sinceClear = (int) std::min<std::int64_t> (pushed - clearedAt, (std::int64_t) maxColumns);
    const auto count = std::min ({ filled, shownColumns(), frozen ? filled : sinceClear });
    auto peak = 0.0f;

    for (int age = 0; age < count; ++age)
    {
        const auto& c = column (age);
        peak = std::max ({ peak, c.reductionDb[0], c.reductionDb[1] });
    }

    return peak;
}

void LevelHistory::clearPeak() noexcept
{
    clearedAt = pushed;
    frozen = false;
}

void LevelHistory::show (const std::vector<dsp::LevelColumn>& fixedColumns)
{
    filled = 0;
    writeIndex = 0;

    for (const auto& c : fixedColumns)
    {
        ring[(size_t) writeIndex] = c;
        writeIndex = (writeIndex + 1) % maxColumns;
        filled = std::min (maxColumns, filled + 1);
    }

    pushed = (std::int64_t) fixedColumns.size();
    clearedAt = 0;
    frozen = true;
    repaint();
}

void LevelHistory::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu())
        setPaused (! paused);
}

void LevelHistory::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto width = bounds.getWidth();
    const auto height = bounds.getHeight();

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto pixels = juce::jmax (1, (int) width);
    const auto span = shownColumns();
    const auto perPixel = (double) span / (double) pixels;

    const auto mid = height * 0.5f;
    const auto amplitude = juce::jmax (1.0f, mid - waveMargin);
    const auto reductionY = [height] (float db)
    { return reductionMargin + (height - 2.0f * reductionMargin) * juce::jlimit (0.0f, 1.0f, db / reductionRangeDb); };

    // Per pixel, oldest on the left: the columns it covers, merged.
    struct Pixel
    {
        float input = 0.0f, output = 0.0f, left = 0.0f, right = 0.0f;
        std::uint8_t mark = 0;
        bool valid = false;
    };

    std::vector<Pixel> merged ((size_t) pixels);
    auto threshold = -200.0f;
    auto unlinked = false;

    for (int x = 0; x < pixels; ++x)
    {
        // Ages run from the oldest shown (span - 1) to the newest (0).
        const auto first = (int) std::floor ((double) x * perPixel);
        const auto last = juce::jmax (first + 1, (int) std::floor ((double) (x + 1) * perPixel));
        auto& p = merged[(size_t) x];

        for (int index = first; index < last; ++index)
        {
            const auto age = span - 1 - index;

            if (age < 0 || age >= filled)
                continue;

            const auto& c = column (age);
            p.input = std::max (p.input, c.inputPeak);
            p.output = std::max (p.output, c.outputPeak);
            p.left = std::max (p.left, c.reductionDb[0]);
            p.right = std::max (p.right, c.reductionDb[1]);
            p.mark = std::max (p.mark, c.mark);
            p.valid = true;

            if (age == 0)
                threshold = c.thresholdDb;
        }

        unlinked = unlinked || (p.valid && std::abs (p.left - p.right) > 0.05f);
    }

    if (filled > 0 && threshold <= -200.0f)
        threshold = column (0).thresholdDb;

    g.saveState();
    g.reduceClipRegion (bounds.toNearestInt());

    // The waveforms, mirrored about the centre line: input, then output over it.
    for (int pass = 0; pass < 2; ++pass)
    {
        g.setColour (pass == 0 ? inputColour.withAlpha (0.9f) : colours::uiTeal.withAlpha (0.85f));

        for (int x = 0; x < pixels; ++x)
        {
            const auto& p = merged[(size_t) x];

            if (! p.valid)
                continue;

            const auto peak = juce::jmin (1.0f, pass == 0 ? p.input : p.output) * amplitude;

            if (peak > 0.25f)
                g.fillRect ((float) x, mid - peak, 1.0f, 2.0f * peak);
        }
    }

    // Beat and bar lines, over the waveforms and faint.
    if (grid)
    {
        for (int x = 0; x < pixels; ++x)
        {
            const auto mark = merged[(size_t) x].mark;

            if (mark != dsp::LevelColumn::none)
            {
                g.setColour (juce::Colours::white.withAlpha (mark == dsp::LevelColumn::bar ? 0.35f : 0.14f));
                g.fillRect ((float) x, 0.0f, 1.0f, height);
            }
        }
    }

    // The threshold in use, a dashed pair at its level.
    if (threshold > -100.0f)
    {
        const auto level = juce::jmin (1.0f, juce::Decibels::decibelsToGain (threshold)) * amplitude;
        const float dashes[] { 4.0f, 4.0f };
        g.setColour (juce::Colours::white.withAlpha (0.45f));

        for (const auto y : { mid - level, mid + level })
            g.drawDashedLine ({ 0.0f, y, width, y }, dashes, 2, 1.0f);
    }

    // The gain reduction from the top: left, then right as a lighter line when they differ.
    for (int channel = unlinked ? 1 : 0; channel >= 0; --channel)
    {
        juce::Path line;
        auto started = false;

        for (int x = 0; x < pixels; ++x)
        {
            const auto& p = merged[(size_t) x];

            if (! p.valid)
            {
                started = false;
                continue;
            }

            const auto point = juce::Point<float> ((float) x + 0.5f, reductionY (channel == 0 ? p.left : p.right));

            if (started)
                line.lineTo (point);
            else
                line.startNewSubPath (point);

            started = true;
        }

        g.setColour (channel == 0 ? reductionColour : rightReductionColour.withAlpha (0.7f));
        g.strokePath (line, juce::PathStrokeType (channel == 0 ? 2.0f : 1.0f));
    }

    // The reduction scale on the right, and a legend at the bottom left.
    g.setFont (font (Weight::regular, 9.0f));
    g.setColour (reductionColour.withAlpha (0.55f));

    for (const auto db : { 6, 12, 18 })
        g.drawText (juce::String (db), juce::Rectangle<float> (width - 30.0f, reductionY ((float) db) - 13.0f, 26.0f, 11.0f),
                    juce::Justification::centredRight, false);

    const auto legendY = height - 14.0f;
    g.setColour (inputColour);
    g.fillRect (6.0f, legendY + 2.0f, 8.0f, 8.0f);
    g.drawText ("in", juce::Rectangle<float> (17.0f, legendY, 20.0f, 12.0f), juce::Justification::centredLeft, false);
    g.setColour (colours::uiTeal);
    g.fillRect (34.0f, legendY + 2.0f, 8.0f, 8.0f);
    g.drawText ("out", juce::Rectangle<float> (45.0f, legendY, 24.0f, 12.0f), juce::Justification::centredLeft, false);
    g.setColour (reductionColour);
    g.fillRect (68.0f, legendY + 5.0f, 8.0f, 2.0f);
    g.drawText ("GR", juce::Rectangle<float> (79.0f, legendY, 24.0f, 12.0f), juce::Justification::centredLeft, false);

    g.restoreState();

    if (paused)
    {
        g.setColour (colours::grey6);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    }
}

} // namespace fancy::ui
