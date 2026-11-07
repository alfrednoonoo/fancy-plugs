#include "CompMeters.h"

#include <fancy/plugin/ParamFormat.h>

using namespace fancy::ui;

namespace
{
constexpr float barWidth = 8.0f, barGap = 1.0f, wellWidth = 17.0f;

// Threshold and GR share a well height. Positions are relative to each component.
constexpr float wellTop = 25.0f, wellHeight = 270.0f;
constexpr float compressionHeight = 20.0f;
constexpr float compressionIndicatorDb = 0.5f;

constexpr float meterFallDbPerSecond = 40.0f;

juce::String readout (float db)
{
    return db <= -60.0f ? juce::String ("-inf") : fancy::formatFixed (db, 1);
}

juce::Colour gradientTop()
{
    return juce::Colour (0xffffcf07);
}

juce::Colour gradientBottom()
{
    return juce::Colour (0xff00ff96);
}
} // namespace

// ThresholdMeter --------------------------------------------------------------

ThresholdMeter::ThresholdMeter (juce::RangedAudioParameter& threshold)
    : ParamControl (threshold)
{
}

juce::Rectangle<int> ThresholdMeter::getValueArea() const
{
    return getLocalBounds().removeFromBottom (23);
}

void ThresholdMeter::update (fancy::dsp::MeterTap& input, bool compressing, float seconds)
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

void ThresholdMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto wellX = bounds.getCentreX() - wellWidth * 0.5f;
    const juce::Rectangle<float> well (wellX, wellTop, wellWidth, wellHeight);

    const auto yFor = [] (float db) { return wellTop + juce::jlimit (0.0f, 1.0f, db / -60.0f) * wellHeight; };

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textPrimary);
    g.drawText ("Threshold", bounds.withHeight (17.0f), juce::Justification::centred, false);

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (well, 2.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto x = wellX + (float) ch * (barWidth + barGap);
        const juce::Rectangle<float> bar (x, wellTop, barWidth, wellHeight);

        juce::ColourGradient gradient (juce::Colour (0xffff5107), 0.0f, wellTop, gradientBottom(), 0.0f, wellTop + wellHeight, false);
        gradient.addColour (0.158, gradientTop());

        g.setGradientFill (gradient);
        g.fillRect (bar.withTop (yFor (channels[ch].level)));

        if (isCompressing)
        {
            g.setColour (colours::accentRed);
            g.fillRect (bar.withHeight (compressionHeight));
        }
    }

    const auto handleY = wellTop + (1.0f - getNormalised()) * wellHeight;
    const auto handle = juce::Rectangle<float> (34.0f, 12.0f).withCentre ({ bounds.getCentreX(), handleY });

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (handle, 2.0f);
    g.setColour (colours::uiTeal);
    g.drawRoundedRectangle (handle.reduced (0.5f), 2.0f, 1.0f);
    g.fillRoundedRectangle (handle.withSizeKeepingCentre (20.0f, 2.0f), 1.0f);

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textPrimary);
    g.drawText (getValueText(), bounds.withTop (bounds.getBottom() - 23.0f), juce::Justification::centred, false);
}

// GainReductionMeter ----------------------------------------------------------

void GainReductionMeter::update (float leftDb, float rightDb, float seconds)
{
    const float incoming[] { leftDb, rightDb };

    for (int ch = 0; ch < 2; ++ch)
        reduction[ch] = juce::jmax (incoming[ch], reduction[ch] - meterFallDbPerSecond * seconds);

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
    constexpr float wellX = 44.0f;

    const auto bounds = getLocalBounds().toFloat();
    const juce::Rectangle<float> well (wellX, wellTop, wellWidth, wellHeight);

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textPrimary);
    g.drawText ("GR", bounds.withHeight (17.0f), juce::Justification::centred, false);

    g.setFont (font (Weight::medium, 11.0f));
    g.setColour (colours::textSecondary);

    for (int db = 0; db <= 30; db += 6)
    {
        const auto y = wellTop + (float) db / maxReductionDb * wellHeight;

        g.drawText (db == 0 ? juce::String (0) : "-" + juce::String (db),
                    juce::Rectangle<float> (0.0f, y - 6.0f, wellX - 8.0f, 12.0f), juce::Justification::centredRight, false);
        g.fillRect (wellX - 6.0f, y - 0.5f, 5.0f, 1.0f);
    }

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (well, 2.0f);

    g.setColour (juce::Colour (0xffffcf07));

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto height = juce::jlimit (0.0f, 1.0f, reduction[ch] / maxReductionDb) * wellHeight;
        g.fillRect (wellX + (float) ch * (barWidth + barGap), wellTop, barWidth, height);
    }

    g.setFont (font (Weight::regular, 11.0f));
    g.setColour (colours::textPrimary);

    const auto text = "-" + fancy::formatFixed (reduction[0], 1) + " dB  -" + fancy::formatFixed (reduction[1], 1) + " dB";
    g.drawText (text, bounds.withTop (bounds.getBottom() - 23.0f), juce::Justification::centred, false);
}
