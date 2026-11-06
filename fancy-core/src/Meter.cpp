#include <fancy/ui/Meter.h>

#include <fancy/Decibels.h>

namespace fancy::ui
{

namespace
{
constexpr float wellTop = 33.0f, wellBottom = 246.0f;
constexpr float zeroDbY = 51.0f;
constexpr float pixelsPerDb = 3.27f;
constexpr float barWidth = 5.5f, barGap = 0.5f;
constexpr float inX = 32.0f, outX = 54.0f;
constexpr float scaleRight = 23.0f;

float yForDb (float db)
{
    return juce::jlimit (wellTop, wellBottom, zeroDbY - db * pixelsPerDb);
}

void paintBar (juce::Graphics& g, float x, const MeterChannel& channel)
{
    const juce::Rectangle<float> well (x, wellTop, barWidth, wellBottom - wellTop);

    g.setColour (colours::bgWell);
    g.fillRect (well);

    juce::ColourGradient gradient (juce::Colour (0xfff7d10a), 0.0f, yForDb (-6.0f),
                                   juce::Colour (0xff0dfc90), 0.0f, wellBottom, false);
    gradient.addColour (0.25, juce::Colour (0xffc7d924));
    gradient.addColour (0.50, juce::Colour (0xff82e64d));
    gradient.addColour (0.75, juce::Colour (0xff3cf473));

    const auto top = yForDb (channel.level);
    g.setGradientFill (gradient);
    g.fillRect (well.withTop (top));

    if (channel.level > 0.0f)
    {
        g.setColour (colours::accentRed);
        g.fillRect (well.withTop (top).withBottom (zeroDbY));
    }

    if (channel.hold > -60.0f)
    {
        g.setColour (channel.hold > 0.0f ? colours::accentRed : colours::textPrimary);
        g.fillRect (well.withTop (yForDb (channel.hold) - 0.5f).withHeight (1.0f));
    }
}

juce::String readout (float db)
{
    return db <= -60.0f ? juce::String ("-inf") : juce::String (db, 1);
}
} // namespace

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
    const auto pairWidth = barWidth * 2.0f + barGap;

    g.setFont (font (Weight::regular, 12.0f));
    g.setColour (colours::textSecondary);
    g.drawText ("In", juce::Rectangle<float> (inX - 8.0f, 14.0f, pairWidth + 16.0f, 16.0f), juce::Justification::centred, false);
    g.drawText ("Out", juce::Rectangle<float> (outX - 8.0f, 14.0f, pairWidth + 16.0f, 16.0f), juce::Justification::centred, false);

    g.setFont (font (Weight::regular, 10.0f));

    for (int db = 0; db <= 54; db += 6)
    {
        const auto y = yForDb ((float) -db);

        g.setColour (colours::textSecondary);
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, scaleRight, 12.0f), juce::Justification::centredRight, false);

        g.setColour (colours::grey5);
        g.fillRect (scaleRight + 3.0f, y - 0.5f, inX - scaleRight - 3.0f, 1.0f);
        g.fillRect (inX + pairWidth, y - 0.5f, outX - inX - pairWidth, 1.0f);
    }

    paintBar (g, inX, channels[0]);
    paintBar (g, inX + barWidth + barGap, channels[1]);
    paintBar (g, outX, channels[2]);
    paintBar (g, outX + barWidth + barGap, channels[3]);

    g.setFont (font (Weight::regular, 12.0f));
    g.setColour (colours::textSecondary);

    const auto inPeak = juce::jmax (channels[0].hold, channels[1].hold);
    const auto outPeak = juce::jmax (channels[2].hold, channels[3].hold);

    g.drawText (readout (inPeak), juce::Rectangle<float> (inX - 10.0f, 251.0f, pairWidth + 20.0f, 16.0f), juce::Justification::centred, false);
    g.drawText (readout (outPeak), juce::Rectangle<float> (outX - 10.0f, 251.0f, pairWidth + 20.0f, 16.0f), juce::Justification::centred, false);
}

} // namespace fancy::ui
