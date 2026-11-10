#pragma once

#include "Controls.h"
#include "Style.h"

#include <fancy/dsp/MeterTap.h>

namespace fancy::ui
{

// Display ballistics for one channel: instant rise, steady fall, timed peak hold.
struct MeterChannel
{
    void update (float peakGain, float seconds);
    void show (float db) { level = hold = db; }

    float level = -120.0f;
    float hold = -120.0f;

private:
    float holdAge = 0.0f;
};

// The In and Out meter column used by Kicker and Exciter. Each side is a stereo pair of
// bars with one peak readout.
class InOutMeter final : public juce::Component
{
public:
    // Vertical layout, from each plugin's Figma frame. The width is the same everywhere.
    struct Geometry
    {
        float wellBottom, zeroDbY, pixelsPerDb, readoutY;
    };

    explicit InOutMeter (const Geometry& geometry);

    void update (dsp::MeterTap& input, dsp::MeterTap& output, float seconds);

    // Sets levels directly, for rendered screenshots.
    void show (float inLeft, float inRight, float outLeft, float outRight);

    void paint (juce::Graphics&) override;

private:
    float yForDb (float db) const;
    void paintBar (juce::Graphics&, float x, const MeterChannel&) const;

    Geometry layout;
    MeterChannel channels[4];
};

// A stereo output meter with one readout, and optionally a dB scale.
class OutMeter final : public juce::Component
{
public:
    // Positions are relative to the component.
    struct Geometry
    {
        float wellX, top, bottom, zeroDbY, pixelsPerDb, readoutY;
        bool showScale, showReadout;
    };

    // Geometry from the Figma LevelController/Out component. The scale area is wider than
    // Figma's, because Inter's digits don't fit its 13 px.
    static constexpr Geometry withScale { 32.5f, 25.0f, 395.0f, 60.07f, 5.545f, 403.0f, true, true };

    explicit OutMeter (const Geometry& geometry = withScale, const juce::String& label = "Out");

    void update (dsp::MeterTap& output, float seconds);

    // Sets levels directly, for rendered screenshots.
    void show (float left, float right);

    void paint (juce::Graphics&) override;

private:
    Geometry layout;
    juce::String labelText;
    MeterChannel channels[2];
};

// One bar of loudness in LUFS, with a red marker at a target and a read-out (Phase 3, Fancy
// Limit). Drawn on the same scale as an OutMeter beside it, without a scale of its own.
class LoudnessBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    // Positions are relative to the component, as OutMeter's are.
    struct Geometry
    {
        float wellX, top, bottom, zeroDbY, pixelsPerDb, readoutY;
    };

    explicit LoudnessBar (const Geometry& geometry, const juce::String& label = "LUFS");

    // Values at or below -100 show as empty, with "-inf".
    void setLevel (float lufs);
    void setTarget (float lufs);

    float getLevel() const noexcept { return level; }
    float getTarget() const noexcept { return target; }

    void paint (juce::Graphics&) override;

    static constexpr float barWidth = 12.0f;

private:
    float yFor (float db) const noexcept;

    Geometry layout;
    juce::String labelText;
    float level = -200.0f, target = -14.0f;
};

// The Threshold fader drawn over a stereo input meter. Fancy Comp's look by default. Fancy
// Gate's adds a dB scale, a dashed close threshold and two read-outs.
class ThresholdMeter final : public ParamControl
{
public:
    // Positions are relative to the component. A negative wellX centres the well.
    struct Style
    {
        float wellX = -1.0f, wellTop = 25.0f, wellHeight = 270.0f;
        float lowestDb = -60.0f;
        juce::Colour accent = colours::uiTeal;
        float labelHeight = 14.0f;
        bool secondaryLabel = false, scale = false, gateReadouts = false;
        float scaleStepDb = 12.0f;
        float scaleTextHeight = 11.0f, readoutTextHeight = 12.0f;
    };

    explicit ThresholdMeter (juce::RangedAudioParameter& threshold);
    ThresholdMeter (juce::RangedAudioParameter& threshold, const Style& style);

    void update (dsp::MeterTap& input, bool compressing, float seconds);

    // Sets levels directly, for rendered screenshots.
    void show (float left, float right, bool compressing);

    // The close threshold drawn dashed, and read out below. Only in the gate style.
    void setCloseDb (float db);

    // Text after the read-out's value in the gate style, such as " rel" (Fancy DS).
    void setReadoutSuffix (const juce::String& suffix);

    void paint (juce::Graphics&) override;
    juce::Colour getAccentColour() const override { return meterStyle.accent; }

private:
    float getDragPixels() const override { return meterStyle.wellHeight; }
    juce::Rectangle<int> getValueArea() const override;
    float wellLeft() const noexcept;

    Style meterStyle;
    MeterChannel channels[2];
    bool isCompressing = false;
    float closeDb = -200.0f;
    juce::String readoutSuffix;
};

// Gain reduction falling from the top: a stereo pair with a scale and one readout per channel,
// or a single bar with one readout.
class GainReductionMeter final : public juce::Component
{
public:
    // Positions are relative to the component.
    struct Geometry
    {
        float wellX, wellTop, wellHeight;
        bool singleBar;
        const char* label;
    };

    // From Fancy Comp's Figma frame.
    static constexpr Geometry stereoWithScale { 44.0f, 25.0f, 270.0f, false, "GR" };

    explicit GainReductionMeter (const Geometry& geometry = stereoWithScale);

    // Yellow by default. Fancy Limit's is its red accent.
    void setBarColour (juce::Colour colour) { barColour = colour; repaint(); }

    void update (float leftDb, float rightDb, float seconds);

    // Sets levels directly, for rendered screenshots.
    void show (float leftDb, float rightDb);

    void paint (juce::Graphics&) override;

private:
    Geometry layout;
    float reduction[2] { 0.0f, 0.0f };
    juce::Colour barColour { 0xffffcf07 };
};

} // namespace fancy::ui
