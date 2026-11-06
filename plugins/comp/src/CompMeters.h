#pragma once

#include <fancy/ui/Controls.h>
#include <fancy/ui/Meter.h>

// The Threshold fader, drawn over the stereo input meter.
class ThresholdMeter final : public fancy::ui::ParamControl
{
public:
    explicit ThresholdMeter (juce::RangedAudioParameter& threshold);

    void update (fancy::dsp::MeterTap& input, bool compressing, float seconds);
    void show (float left, float right, bool compressing);

    void paint (juce::Graphics&) override;

private:
    float getDragPixels() const override { return 270.0f; }
    juce::Rectangle<int> getValueEditorBounds() const override;

    fancy::ui::MeterChannel channels[2];
    bool isCompressing = false;
};

// Gain reduction bars falling from the top, with one readout per channel.
class GainReductionMeter final : public juce::Component
{
public:
    void update (float leftDb, float rightDb, float seconds);
    void show (float leftDb, float rightDb);

    void paint (juce::Graphics&) override;

private:
    float reduction[2] { 0.0f, 0.0f };
};

// The output meter with its dB scale and one readout.
class OutMeter final : public juce::Component
{
public:
    void update (fancy::dsp::MeterTap& output, float seconds);
    void show (float left, float right);

    void paint (juce::Graphics&) override;

private:
    fancy::ui::MeterChannel channels[2];
};
