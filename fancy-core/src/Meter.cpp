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

OutMeter::OutMeter (const Geometry& geometry)
    : layout (geometry)
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
    g.drawText ("Out", juce::Rectangle<float> (wellX - 8.0f, 0.0f, wellWidth + 16.0f, 17.0f), juce::Justification::centred, false);

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

    const auto peak = juce::jmax (channels[0].hold, channels[1].hold);

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textSecondary);
    g.drawText (peak <= -60.0f ? juce::String ("-inf") : formatFixed (peak, 1),
                juce::Rectangle<float> (wellX + wellWidth * 0.5f - 20.0f, layout.readoutY, 40.0f, 23.0f), juce::Justification::centred, false);
}

} // namespace fancy::ui
