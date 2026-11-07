#pragma once

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
        bool showScale;
    };

    // Geometry from the Figma LevelController/Out component. The scale area is wider than
    // Figma's, because Inter's digits don't fit its 13 px.
    static constexpr Geometry withScale { 32.5f, 25.0f, 395.0f, 60.07f, 5.545f, 403.0f, true };

    explicit OutMeter (const Geometry& geometry = withScale);

    void update (dsp::MeterTap& output, float seconds);

    // Sets levels directly, for rendered screenshots.
    void show (float left, float right);

    void paint (juce::Graphics&) override;

private:
    Geometry layout;
    MeterChannel channels[2];
};

} // namespace fancy::ui
