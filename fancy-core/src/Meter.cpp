#include <fancy/ui/Meter.h>

#include <fancy/Decibels.h>
#include <fancy/plugin/ParamFormat.h>

namespace fancy::ui
{

namespace
{
// Geometry from the Figma LevelController/InOut component, relative to the panel.
constexpr float wellTop = 49.0f;
constexpr float barWidth = 8.0f, barGap = 1.0f, wellWidth = 17.0f;
constexpr float inX = 46.5f, outX = 80.5f;
constexpr float scaleRight = 37.0f, gridLeft = 39.0f, gridRight = 96.0f;

juce::String readout (float db)
{
    return db <= -60.0f ? juce::String ("-inf") : juce::String (db, 1);
}
} // namespace

InOutMeter::InOutMeter (const Geometry& geometry)
    : layout (geometry)
{
}

float InOutMeter::yForDb (float db) const
{
    return juce::jlimit (wellTop, layout.wellBottom, layout.zeroDbY - db * layout.pixelsPerDb);
}

void InOutMeter::paintBar (juce::Graphics& g, float x, const MeterChannel& channel) const
{
    const juce::Rectangle<float> bar (x, wellTop, barWidth, layout.wellBottom - wellTop);
    const auto top = yForDb (channel.level);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffcf07), 0.0f, yForDb (-6.0f),
                                             juce::Colour (0xff00ff96), 0.0f, layout.wellBottom, false));
    g.fillRect (bar.withTop (top));

    if (channel.level > 0.0f)
    {
        g.setColour (colours::accentRed);
        g.fillRect (bar.withTop (top).withBottom (layout.zeroDbY));
    }

    if (channel.hold > -60.0f)
    {
        g.setColour (channel.hold > 0.0f ? colours::accentRed : colours::textPrimary);
        g.fillRect (bar.withTop (yForDb (channel.hold) - 0.5f).withHeight (1.0f));
    }
}

void MeterChannel::update (float peakGain, float seconds)
{
    constexpr float fallDbPerSecond = 26.0f;
    constexpr float holdSeconds = 1.5f;

    const auto db = (float) gainToDb ((double) peakGain);

    level = juce::jmax (db, level - fallDbPerSecond * seconds);

    if (db >= hold)
    {
        hold = db;
        holdAge = 0.0f;
    }
    else
    {
        holdAge += seconds;

        if (holdAge > holdSeconds)
            hold = juce::jmax (db, hold - fallDbPerSecond * seconds);
    }
}

void InOutMeter::update (dsp::MeterTap& input, dsp::MeterTap& output, float seconds)
{
    channels[0].update (input.take (0), seconds);
    channels[1].update (input.take (1), seconds);
    channels[2].update (output.take (0), seconds);
    channels[3].update (output.take (1), seconds);
    repaint();
}

void InOutMeter::show (float inLeft, float inRight, float outLeft, float outRight)
{
    channels[0].show (inLeft);
    channels[1].show (inRight);
    channels[2].show (outLeft);
    channels[3].show (outRight);
    repaint();
}

void InOutMeter::paint (juce::Graphics& g)
{
    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);
    g.drawText ("In", juce::Rectangle<float> (inX - 8.0f, 24.0f, wellWidth + 16.0f, 17.0f), juce::Justification::centred, false);
    g.drawText ("Out", juce::Rectangle<float> (outX - 8.0f, 24.0f, wellWidth + 16.0f, 17.0f), juce::Justification::centred, false);

    g.setFont (font (Weight::medium, 11.0f));

    for (int db = 0; db <= 54; db += 6)
    {
        const auto y = yForDb ((float) -db);

        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, scaleRight, 12.0f), juce::Justification::centredRight, false);
        g.fillRect (gridLeft, y - 0.5f, gridRight - gridLeft, 1.0f);
    }

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (inX, wellTop, wellWidth, layout.wellBottom - wellTop, 2.0f);
    g.fillRoundedRectangle (outX, wellTop, wellWidth, layout.wellBottom - wellTop, 2.0f);

    paintBar (g, inX, channels[0]);
    paintBar (g, inX + barWidth + barGap, channels[1]);
    paintBar (g, outX, channels[2]);
    paintBar (g, outX + barWidth + barGap, channels[3]);

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);

    const auto inPeak = juce::jmax (channels[0].hold, channels[1].hold);
    const auto outPeak = juce::jmax (channels[2].hold, channels[3].hold);

    g.drawText (readout (inPeak), juce::Rectangle<float> (35.0f, layout.readoutY, 40.0f, 23.0f), juce::Justification::centred, false);
    g.drawText (readout (outPeak), juce::Rectangle<float> (69.0f, layout.readoutY, 40.0f, 23.0f), juce::Justification::centred, false);
}

// OutMeter --------------------------------------------------------------------

OutMeter::OutMeter (const Geometry& geometry, const juce::String& label)
    : layout (geometry), labelText (label)
{
}

void OutMeter::update (dsp::MeterTap& output, float seconds)
{
    channels[0].update (output.take (0), seconds);
    channels[1].update (output.take (1), seconds);
    repaint();
}

void OutMeter::show (float left, float right)
{
    channels[0].show (left);
    channels[1].show (right);
    repaint();
}

void OutMeter::paint (juce::Graphics& g)
{
    constexpr float scaleRight = 23.0f, scaleGridLeft = 25.0f;

    const auto wellX = layout.wellX, top = layout.top, bottom = layout.bottom;
    const auto yFor = [this] (float db) { return juce::jlimit (layout.top, layout.bottom, layout.zeroDbY - db * layout.pixelsPerDb); };

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);
    g.drawText (labelText, juce::Rectangle<float> (wellX - 8.0f, 0.0f, wellWidth + 16.0f, 17.0f), juce::Justification::centred, false);

    if (layout.showScale)
    {
        g.setFont (font (Weight::medium, 11.0f));

        for (int db = 0; db <= 54; db += 6)
        {
            const auto y = yFor ((float) -db);

            g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, scaleRight, 12.0f), juce::Justification::centredRight, false);
            g.fillRect (scaleGridLeft, y - 0.5f, wellX - scaleGridLeft, 1.0f);
        }
    }

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (wellX, top, wellWidth, bottom - top, 2.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto x = wellX + (float) ch * (barWidth + barGap);
        const juce::Rectangle<float> bar (x, top, barWidth, bottom - top);
        const auto y = yFor (channels[ch].level);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffcf07), 0.0f, yFor (-6.0f), juce::Colour (0xff00ff96), 0.0f, bottom, false));
        g.fillRect (bar.withTop (y));

        if (channels[ch].level > 0.0f)
        {
            g.setColour (colours::accentRed);
            g.fillRect (bar.withTop (y).withBottom (layout.zeroDbY));
        }

        if (channels[ch].hold > -60.0f)
        {
            g.setColour (channels[ch].hold > 0.0f ? colours::accentRed : colours::textPrimary);
            g.fillRect (bar.withTop (yFor (channels[ch].hold) - 0.5f).withHeight (1.0f));
        }
    }

    if (! layout.showReadout)
        return;

    const auto peak = juce::jmax (channels[0].hold, channels[1].hold);

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);
    g.drawText (peak <= -60.0f ? juce::String ("-inf") : formatFixed (peak, 1),
                juce::Rectangle<float> (wellX + wellWidth * 0.5f - 20.0f, layout.readoutY, 40.0f, 23.0f), juce::Justification::centred, false);
}

// LoudnessBar -----------------------------------------------------------------

LoudnessBar::LoudnessBar (const Geometry& geometry, const juce::String& label)
    : layout (geometry), labelText (label)
{
}

float LoudnessBar::yFor (float db) const noexcept
{
    return juce::jlimit (layout.top, layout.bottom, layout.zeroDbY - db * layout.pixelsPerDb);
}

void LoudnessBar::setLevel (float lufs)
{
    if (std::abs (lufs - level) > 0.01f)
    {
        level = lufs;
        repaint();
    }
}

void LoudnessBar::setTarget (float lufs)
{
    if (lufs != target)
    {
        target = lufs;
        repaint();
    }
}

void LoudnessBar::paint (juce::Graphics& g)
{
    const auto x = layout.wellX;

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);
    g.drawText (labelText, juce::Rectangle<float> (x + barWidth * 0.5f - 24.0f, 0.0f, 48.0f, 17.0f), juce::Justification::centred, false);

    const juce::Rectangle<float> well (x, layout.top, barWidth, layout.bottom - layout.top);
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (well, 2.0f);

    if (level > -100.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffcf07), 0.0f, yFor (-6.0f), juce::Colour (0xff00ff96), 0.0f, layout.bottom, false));
        g.fillRect (well.withTop (yFor (level)));
    }

    // The target: a red line across the bar, a little wider than it.
    g.setColour (colours::accentRed);
    g.fillRect (juce::Rectangle<float> (x - 1.0f, yFor (target) - 1.0f, barWidth + 2.0f, 2.0f));

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);
    g.drawText (level <= -100.0f ? juce::String ("-inf") : formatFixed (level, 1),
                juce::Rectangle<float> (x + barWidth * 0.5f - 24.0f, layout.readoutY, 48.0f, 23.0f), juce::Justification::centred, false);
}

// ThresholdMeter --------------------------------------------------------------

ThresholdMeter::ThresholdMeter (juce::RangedAudioParameter& threshold)
    : ThresholdMeter (threshold, Style {})
{
}

ThresholdMeter::ThresholdMeter (juce::RangedAudioParameter& threshold, const Style& style)
    : ParamControl (threshold), meterStyle (style)
{
}

float ThresholdMeter::wellLeft() const noexcept
{
    return meterStyle.wellX < 0.0f ? (float) getWidth() * 0.5f - wellWidth * 0.5f : meterStyle.wellX;
}

juce::Rectangle<int> ThresholdMeter::getValueArea() const
{
    if (meterStyle.gateReadouts)
        return { 0, (int) (meterStyle.wellTop + meterStyle.wellHeight) + 1, getWidth(), 14 };

    return getLocalBounds().removeFromBottom (23);
}

void ThresholdMeter::update (dsp::MeterTap& input, bool compressing, float seconds)
{
    channels[0].update (input.take (0), seconds);
    channels[1].update (input.take (1), seconds);
    isCompressing = compressing;
    repaint();
}

void ThresholdMeter::show (float left, float right, bool compressing)
{
    channels[0].show (left);
    channels[1].show (right);
    isCompressing = compressing;
    repaint();
}

void ThresholdMeter::setCloseDb (float db)
{
    if (db != closeDb)
    {
        closeDb = db;
        repaint();
    }
}

void ThresholdMeter::paint (juce::Graphics& g)
{
    constexpr float compressionHeight = 20.0f;

    const auto& style = meterStyle;
    const auto bounds = getLocalBounds().toFloat();
    const auto wellX = wellLeft();
    const auto wellTop = style.wellTop, wellHeight = style.wellHeight;
    const juce::Rectangle<float> well (wellX, wellTop, wellWidth, wellHeight);

    const auto yFor = [&] (float db) { return wellTop + juce::jlimit (0.0f, 1.0f, db / style.lowestDb) * wellHeight; };

    g.setFont (font (Weight::regular, style.labelHeight));
    g.setColour (style.secondaryLabel ? colours::textSecondary : colours::textPrimary);
    g.drawText ("Threshold", bounds.withHeight (17.0f), juce::Justification::centred, false);

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (well, 2.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto x = wellX + (float) ch * (barWidth + barGap);
        const juce::Rectangle<float> bar (x, wellTop, barWidth, wellHeight);

        juce::ColourGradient gradient (juce::Colour (0xffff5107), 0.0f, wellTop, juce::Colour (0xff00ff96), 0.0f, wellTop + wellHeight, false);
        gradient.addColour (0.158, juce::Colour (0xffffcf07));

        g.setGradientFill (gradient);
        g.fillRect (bar.withTop (yFor (channels[ch].level)));

        if (isCompressing)
        {
            g.setColour (colours::accentRed);
            g.fillRect (bar.withHeight (compressionHeight));
        }
    }

    if (style.scale)
    {
        const auto textHeight = style.scaleTextHeight;
        g.setFont (font (Weight::medium, textHeight));
        g.setColour (colours::textSecondary);

        // The end labels stay inside the well's height, clear of the title and the readouts.
        for (auto db = 0.0f; db >= style.lowestDb - 0.01f; db -= style.scaleStepDb)
        {
            const auto top = juce::jlimit (wellTop, wellTop + wellHeight - textHeight, yFor (db) - textHeight * 0.5f);
            g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (wellX + wellWidth + 2.0f, top, bounds.getWidth() - wellX - wellWidth - 2.0f, textHeight),
                        juce::Justification::centredRight, false);
        }
    }

    if (style.gateReadouts && closeDb > style.lowestDb - 1.0f)
    {
        // The close threshold, dashed across the well.
        const auto y = yFor (closeDb);

        g.setColour (style.accent);

        for (auto x = wellX - 3.0f; x < wellX + wellWidth + 3.0f; x += 4.0f)
            g.fillRect (x, y - 1.0f, 2.0f, 2.0f);
    }

    const auto handleY = wellTop + (1.0f - getNormalised()) * wellHeight;
    const auto handle = juce::Rectangle<float> (style.gateReadouts ? 24.0f : 34.0f, style.gateReadouts ? 10.0f : 12.0f)
                            .withCentre ({ wellX + wellWidth * 0.5f, handleY });

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (handle, 2.0f);
    g.setColour (style.accent);
    g.drawRoundedRectangle (handle.reduced (0.5f), 2.0f, 1.0f);
    g.fillRoundedRectangle (handle.withSizeKeepingCentre (style.gateReadouts ? 16.0f : 20.0f, 2.0f), 1.0f);

    if (style.gateReadouts)
    {
        const auto readoutTop = wellTop + wellHeight + 1.0f;

        g.setFont (font (Weight::regular, style.readoutTextHeight));
        g.setColour (style.accent);
        g.drawText (getValueText(), juce::Rectangle<float> (0.0f, readoutTop, bounds.getWidth(), 14.0f), juce::Justification::centred, false);

        // A note, such as Fancy DS's "relative", takes the second line when there's no close threshold.
        if (readoutNote.isNotEmpty() && closeDb <= -199.0f)
        {
            g.setColour (colours::textSecondary);
            g.drawText (readoutNote, juce::Rectangle<float> (0.0f, readoutTop + 14.0f, bounds.getWidth(), 14.0f), juce::Justification::centred, false);
        }

        // Fancy Limit has no close threshold, so it never sets one.
        if (closeDb > -199.0f)
        {
            g.setColour (colours::textSecondary);
            g.drawText ("close " + formatFixed (closeDb, 1), juce::Rectangle<float> (0.0f, readoutTop + 14.0f, bounds.getWidth(), 14.0f),
                    juce::Justification::centred, false);
        }

        return;
    }

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textPrimary);
    g.drawText (getValueText(), bounds.withTop (bounds.getBottom() - 23.0f), juce::Justification::centred, false);
}

void ThresholdMeter::setReadoutNote (const juce::String& note)
{
    if (note != readoutNote)
    {
        readoutNote = note;
        repaint();
    }
}

// GainReductionMeter ----------------------------------------------------------

GainReductionMeter::GainReductionMeter (const Geometry& geometry)
    : layout (geometry)
{
}

void GainReductionMeter::update (float leftDb, float rightDb, float seconds)
{
    constexpr float fallDbPerSecond = 40.0f;

    const float incoming[] { leftDb, rightDb };

    for (int ch = 0; ch < 2; ++ch)
        reduction[ch] = juce::jmax (incoming[ch], reduction[ch] - fallDbPerSecond * seconds);

    repaint();
}

void GainReductionMeter::show (float leftDb, float rightDb)
{
    reduction[0] = leftDb;
    reduction[1] = rightDb;
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    constexpr float maxReductionDb = 30.0f;

    const auto bounds = getLocalBounds().toFloat();
    const auto wellX = layout.wellX, wellTop = layout.wellTop, wellHeight = layout.wellHeight;
    const auto width = layout.singleBar ? barWidth : wellWidth;
    const juce::Rectangle<float> well (wellX, wellTop, width, wellHeight);

    g.setFont (font (Weight::regular, layout.singleBar ? 12.0f : 14.0f));
    g.setColour (layout.singleBar ? colours::textSecondary : colours::textPrimary);
    g.drawText (layout.label, bounds.withHeight (17.0f), juce::Justification::centred, false);

    if (! layout.singleBar)
    {
        g.setFont (font (Weight::medium, 11.0f));
        g.setColour (colours::textSecondary);

        for (int db = 0; db <= 30; db += 6)
        {
            const auto y = wellTop + (float) db / maxReductionDb * wellHeight;

            g.drawText (db == 0 ? juce::String (0) : "-" + juce::String (db),
                        juce::Rectangle<float> (0.0f, y - 6.0f, wellX - 8.0f, 12.0f), juce::Justification::centredRight, false);
            g.fillRect (wellX - 6.0f, y - 0.5f, 5.0f, 1.0f);
        }
    }

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (well, 2.0f);

    g.setColour (barColour);

    for (int ch = 0; ch < (layout.singleBar ? 1 : 2); ++ch)
    {
        const auto height = juce::jlimit (0.0f, 1.0f, reduction[ch] / maxReductionDb) * wellHeight;
        g.fillRect (wellX + (float) ch * (barWidth + barGap), wellTop, barWidth, height);
    }

    g.setFont (font (Weight::regular, layout.singleBar ? 12.0f : 11.0f));
    g.setColour (colours::textPrimary);

    const auto text = layout.singleBar ? "-" + formatFixed (reduction[0], 1)
                                       : "-" + formatFixed (reduction[0], 1) + " dB  -" + formatFixed (reduction[1], 1) + " dB";

    g.drawText (text, bounds.withTop (bounds.getBottom() - 23.0f), juce::Justification::centred, false);
}

} // namespace fancy::ui
