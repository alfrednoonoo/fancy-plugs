#include "CompMeters.h"

#include <fancy/plugin/ParamFormat.h>

using namespace fancy::ui;

namespace
{
constexpr float barWidth = 8.0f, barGap = 1.0f, wellWidth = 17.0f;

// Threshold and GR share a well height. Positions are relative to each component.
constexpr float wellTop = 25.0f, wellHeight = 270.0f;
constexpr float compressionHeight = 20.0f;


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
