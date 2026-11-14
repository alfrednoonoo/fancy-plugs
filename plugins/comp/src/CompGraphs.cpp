#include "CompGraphs.h"

#include <fancy/dsp/Compression.h>
#include <fancy/dsp/MatchedFilters.h>
#include <fancy/dsp/Saturation.h>
#include <fancy/dsp/Upward.h>

#include <array>
#include <cmath>
#include <numbers>

using namespace fancy::ui;

namespace
{
constexpr double minHz = 20.0, maxHz = 20000.0;
constexpr double curveDb = 18.0;
constexpr double previewRate = 48000.0;
constexpr float handleRadius = 5.0f;

const char* const frequencyIds[] { "scLowFreq", "scMidFreq", "scHighFreq" };
const char* const gainIds[] { "scLowGain", "scMidGain", "scHighGain" };

void drawDashed (juce::Graphics& g, juce::Line<float> line, float thickness)
{
    const float dashes[] { 4.0f, 4.0f };
    g.drawDashedLine (line, dashes, 2, thickness);
}

juce::Font smallFont (float height = 10.0f)
{
    return font (Weight::regular, height);
}
} // namespace

// DrawerTabs ------------------------------------------------------------------

void DrawerTabs::setSelected (int tab)
{
    tab = juce::jlimit (0, numTabs - 1, tab);

    if (tab != selected)
    {
        selected = tab;
        repaint();
    }
}

juce::Rectangle<float> DrawerTabs::tabBounds (int index) const
{
    const auto area = getLocalBounds().toFloat().reduced (3.0f);
    const auto width = area.getWidth() / (float) numTabs;
    return area.withX (area.getX() + (float) index * width).withWidth (width);
}

void DrawerTabs::paint (juce::Graphics& g)
{
    static const char* const names[] { "Detector", "Character", "Upward", "Options" };

    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    g.setFont (font (Weight::regular, 12.0f));

    for (int i = 0; i < numTabs; ++i)
    {
        const auto area = tabBounds (i);

        if (i == selected)
        {
            g.setColour (colours::grey5);
            g.fillRoundedRectangle (area, 3.0f);
        }

        g.setColour (i == selected ? colours::textPrimary : colours::textSecondary);
        g.drawText (names[i], area, juce::Justification::centred, false);
    }
}

void DrawerTabs::mouseDown (const juce::MouseEvent& event)
{
    for (int i = 0; i < numTabs; ++i)
    {
        if (tabBounds (i).contains (event.position) && i != selected)
        {
            setSelected (i);

            if (onChange != nullptr)
                onChange (i);
        }
    }
}

// CompParameters --------------------------------------------------------------

float CompParameters::get (const juce::String& id) const
{
    auto& parameter = find (id);
    return parameter.convertFrom0to1 (parameter.getValue());
}

void CompParameters::set (const juce::String& id, float value) const
{
    auto& parameter = find (id);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

void CompParameters::begin (const juce::String& id) const
{
    find (id).beginChangeGesture();
}

void CompParameters::end (const juce::String& id) const
{
    find (id).endChangeGesture();
}

// DetectorCurve ---------------------------------------------------------------

DetectorCurve::DetectorCurve (CompParameters parameters) : params (std::move (parameters))
{
}

float DetectorCurve::xFor (double hz) const
{
    const auto t = std::log (hz / minHz) / std::log (maxHz / minHz);
    return 10.0f + (float) t * ((float) getWidth() - 20.0f);
}

double DetectorCurve::hzFor (float x) const
{
    const auto t = (double) (x - 10.0f) / std::max (1.0, (double) getWidth() - 20.0);
    return minHz * std::pow (maxHz / minHz, juce::jlimit (0.0, 1.0, t));
}

float DetectorCurve::yFor (double db) const
{
    const auto half = (float) getHeight() * 0.5f;
    return half - (float) (db / curveDb) * (half - 8.0f);
}

double DetectorCurve::dbFor (float y) const
{
    const auto half = (float) getHeight() * 0.5f;
    return (double) ((half - y) / (half - 8.0f)) * curveDb;
}

juce::Point<float> DetectorCurve::handleFor (int band) const
{
    return { xFor ((double) params.get (frequencyIds[band])), yFor ((double) params.get (gainIds[band])) };
}

int DetectorCurve::bandAt (juce::Point<float> position) const
{
    auto best = -1;
    auto bestDistance = 12.0f;

    for (int b = 0; b < 3; ++b)
    {
        const auto distance = handleFor (b).getDistanceFrom (position);

        if (distance < bestDistance)
        {
            best = b;
            bestDistance = distance;
        }
    }

    return best;
}

void DetectorCurve::paint (juce::Graphics& g)
{
    namespace matched = fancy::dsp::matched;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (colours::bgSection);

    for (const auto hz : { 100.0, 1000.0, 10000.0 })
        g.drawVerticalLine (juce::roundToInt (xFor (hz)), 6.0f, bounds.getBottom() - 6.0f);

    g.setFont (smallFont (9.0f));

    for (const auto db : { -12.0, 0.0, 12.0 })
    {
        const auto y = yFor (db);
        g.setColour (db == 0.0 ? colours::bgBody : colours::bgSection);
        g.drawHorizontalLine (juce::roundToInt (y), 10.0f, bounds.getRight() - 10.0f);
        g.setColour (colours::textDisabled);
        g.drawText ((db > 0.0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (bounds.getRight() - 40.0f, y - 12.0f, 28.0f, 11.0f),
                    juce::Justification::centredRight, false);
    }

    const auto highpass = (double) params.get ("highpass");
    const auto lowpass = (double) params.get ("lowpass");

    std::array<matched::BandFilter, 3> filters;

    for (int b = 0; b < 3; ++b)
    {
        matched::BandSpec spec;
        spec.type = b == 0 ? matched::BandType::lowShelf : b == 2 ? matched::BandType::highShelf : matched::BandType::band;
        spec.frequencyHz = (double) params.get (frequencyIds[b]);
        spec.gainDb = (double) params.get (gainIds[b]);
        spec.q = b == 1 ? (double) params.get ("scMidQ") : matched::butterworthQ;
        spec.order = 2;
        spec.on = spec.gainDb != 0.0;
        filters[(size_t) b] = matched::design (spec, previewRate);
    }

    juce::Path filtersOnly, total;
    const auto steps = std::max (2, getWidth() / 2);

    for (int i = 0; i <= steps; ++i)
    {
        const auto hz = minHz * std::pow (maxHz / minHz, (double) i / (double) steps);
        auto db = 0.0;

        if (highpass > 20.5)
        {
            const auto r = std::pow (hz / highpass, 4.0);
            db += 10.0 * std::log10 (r / (1.0 + r));
        }

        if (lowpass < 23900.0)
            db += 10.0 * std::log10 (1.0 / (1.0 + std::pow (hz / lowpass, 4.0)));

        auto withBands = db;
        const auto w = 2.0 * std::numbers::pi * hz / previewRate;

        for (const auto& filter : filters)
            if (filter.numSections > 0)
                withBands += 20.0 * std::log10 (std::max (1.0e-9, std::abs (filter.responseAt (w))));

        const auto x = xFor (hz);
        const auto clampDb = [] (double v) { return juce::jlimit (-curveDb - 2.0, curveDb + 2.0, v); };

        if (i == 0)
        {
            filtersOnly.startNewSubPath (x, yFor (clampDb (db)));
            total.startNewSubPath (x, yFor (clampDb (withBands)));
        }
        else
        {
            filtersOnly.lineTo (x, yFor (clampDb (db)));
            total.lineTo (x, yFor (clampDb (withBands)));
        }
    }

    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (2));

    g.setColour (colours::uiBlue.withAlpha (0.35f));
    g.strokePath (filtersOnly, juce::PathStrokeType (1.5f));
    g.setColour (colours::uiBlue);
    g.strokePath (total, juce::PathStrokeType (2.0f));

    for (int b = 0; b < 3; ++b)
    {
        const auto centre = handleFor (b);
        const auto active = params.get (gainIds[b]) != 0.0f;
        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (centre);

        g.setColour (colours::bgWell);
        g.fillEllipse (circle);
        g.setColour (colours::uiBlue.withAlpha (active ? 1.0f : 0.35f));
        g.drawEllipse (circle, 2.0f);
    }

    g.restoreState();

    if (highpass > 20.5)
    {
        g.setFont (smallFont (9.0f));
        g.setColour (colours::uiBlue.withAlpha (0.5f));
        g.drawText ("HP " + juce::String (juce::roundToInt (highpass)) + " Hz",
                    juce::Rectangle<float> (xFor (highpass) + 4.0f, bounds.getBottom() - 16.0f, 80.0f, 11.0f), juce::Justification::centredLeft, false);
    }
}

void DetectorCurve::mouseDown (const juce::MouseEvent& event)
{
    dragging = bandAt (event.position);

    if (dragging >= 0)
    {
        params.begin (frequencyIds[dragging]);
        params.begin (gainIds[dragging]);
    }
}

void DetectorCurve::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging < 0)
        return;

    params.set (frequencyIds[dragging], (float) hzFor (event.position.x));
    params.set (gainIds[dragging], (float) juce::jlimit (-curveDb, curveDb, dbFor (event.position.y)));
    repaint();
}

void DetectorCurve::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        params.end (frequencyIds[dragging]);
        params.end (gainIds[dragging]);
    }

    dragging = -1;
}

void DetectorCurve::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto q = params.get ("scMidQ") * std::exp (wheel.deltaY * 0.6f);

    params.begin ("scMidQ");
    params.set ("scMidQ", juce::jlimit (0.3f, 8.0f, q));
    params.end ("scMidQ");
    repaint();
}

// CharacterCurve --------------------------------------------------------------

CharacterCurve::CharacterCurve (CompParameters parameters) : params (std::move (parameters))
{
}

void CharacterCurve::paint (juce::Graphics& g)
{
    namespace sat = fancy::dsp::saturation;

    constexpr float margin = 14.0f;
    constexpr double low = -24.0, high = 6.0;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto size = bounds.getWidth() - 2.0f * margin;
    const auto xFor = [&] (double db) { return margin + (float) ((db - low) / (high - low)) * size; };
    const auto yFor = [&] (double db) { return bounds.getHeight() - margin - (float) ((db - low) / (high - low)) * (bounds.getHeight() - 2.0f * margin); };

    g.setColour (colours::bgSection);

    for (const auto db : { -18.0, -12.0, -6.0, 0.0 })
    {
        g.drawVerticalLine (juce::roundToInt (xFor (db)), margin, bounds.getHeight() - margin);
        g.drawHorizontalLine (juce::roundToInt (yFor (db)), margin, bounds.getWidth() - margin);
    }

    g.setColour (colours::grey6);
    drawDashed (g, { xFor (low), yFor (low), xFor (high), yFor (high) }, 1.0f);

    const auto type = juce::roundToInt (params.get ("charType"));
    const auto driveDb = (double) params.get ("charDrive");

    g.setFont (smallFont());
    g.setColour (colours::textSecondary);

    if (type == 0)
    {
        g.drawText ("Off", juce::Rectangle<float> (margin + 4.0f, margin - 4.0f, size, 14.0f), juce::Justification::centredLeft, false);
        return;
    }

    g.drawText ((type == 1 ? "Warm" : "Bright") + juce::String (" - Drive ") + juce::String (driveDb, 1) + " dB",
                juce::Rectangle<float> (margin + 4.0f, margin - 4.0f, size, 14.0f), juce::Justification::centredLeft, false);

    const auto drive = std::pow (10.0, driveDb / 20.0);
    juce::Path curve;

    for (int i = 0; i <= 120; ++i)
    {
        const auto inDb = low + (high - low) * (double) i / 120.0;
        const auto u = drive * std::pow (10.0, inDb / 20.0);
        const auto shaped = type == 1 ? sat::soft (u, 1.0) : sat::knee (u, sat::transistorKnee);
        const auto outDb = 20.0 * std::log10 (std::max (1.0e-9, shaped / drive));

        if (i == 0)
            curve.startNewSubPath (xFor (inDb), yFor (outDb));
        else
            curve.lineTo (xFor (inDb), yFor (outDb));
    }

    g.setColour (colours::uiTeal);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

// UpwardCurve -----------------------------------------------------------------

UpwardCurve::UpwardCurve (CompParameters parameters) : params (std::move (parameters))
{
}

void UpwardCurve::setCurrent (float levelDb, float liftDb, float autoOffsetDb)
{
    if (std::abs (levelDb - level) > 0.1f || std::abs (liftDb - lift) > 0.05f || std::abs (autoOffsetDb - offset) > 0.05f)
    {
        level = levelDb;
        lift = liftDb;
        offset = autoOffsetDb;
        repaint();
    }
}

void UpwardCurve::paint (juce::Graphics& g)
{
    constexpr float margin = 18.0f;
    constexpr double low = -80.0, high = 0.0;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgWell);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto xFor = [&] (double db) { return margin + (float) ((db - low) / (high - low)) * (bounds.getWidth() - 2.0f * margin); };
    const auto yFor = [&] (double db) { return bounds.getHeight() - margin - (float) ((db - low) / (high - low)) * (bounds.getHeight() - 2.0f * margin); };

    g.setColour (colours::bgSection);

    for (auto db = -70.0; db < 0.0; db += 10.0)
    {
        g.drawVerticalLine (juce::roundToInt (xFor (db)), margin, bounds.getHeight() - margin);
        g.drawHorizontalLine (juce::roundToInt (yFor (db)), margin, bounds.getWidth() - margin);
    }

    g.setColour (colours::grey6);
    drawDashed (g, { xFor (low), yFor (low), xFor (high), yFor (high) }, 1.0f);

    const auto autoOn = params.get ("autoThr") >= 0.5f;
    const auto shift = autoOn ? (double) offset : 0.0;
    const auto threshold = (double) params.get ("threshold") + shift;
    const auto ratio = (double) params.get ("ratio");
    const auto slope = ratio >= 100.0 ? 1.0 : 1.0 - 1.0 / std::max (1.0, ratio);
    const auto knee = (double) params.get ("knee");
    const auto weird = params.get ("weird") >= 0.5f;
    const auto upward = params.get ("up") >= 0.5f;

    const fancy::dsp::upward::Settings settings { (double) params.get ("upThr") + shift, (double) params.get ("upRatio"),
                                                  (double) params.get ("upLift"), (double) params.get ("upFloor") };

    const auto out = [&] (double in)
    {
        auto result = in - fancy::dsp::computeReduction (in, threshold, slope, knee, weird);

        if (upward)
            result += fancy::dsp::upward::liftFor (in, threshold, settings);

        return result;
    };

    juce::Path curve, liftArea, reductionArea;
    constexpr int steps = 200;

    for (int i = 0; i <= steps; ++i)
    {
        const auto in = low + (high - low) * (double) i / (double) steps;
        const auto y = juce::jlimit (low - 30.0, high + 5.0, out (in));
        const auto x = xFor (in);

        if (i == 0)
        {
            curve.startNewSubPath (x, yFor (y));
            liftArea.startNewSubPath (x, yFor (in));
            reductionArea.startNewSubPath (x, yFor (in));
        }
        else
        {
            curve.lineTo (x, yFor (y));
        }

        liftArea.lineTo (x, yFor (std::max (y, in)));
        reductionArea.lineTo (x, yFor (std::min (y, in)));
    }

    liftArea.lineTo (xFor (high), yFor (high));
    liftArea.closeSubPath();
    reductionArea.lineTo (xFor (high), yFor (high));
    reductionArea.closeSubPath();

    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (2));

    g.setColour (colours::accentGreen.withAlpha (0.18f));
    g.fillPath (liftArea);
    g.setColour (colours::uiTeal.withAlpha (0.18f));
    g.fillPath (reductionArea);

    const auto marker = [&] (double db, juce::Colour colour, const juce::String& text, float textY)
    {
        const auto x = xFor (db);
        const float dashes[] { 2.0f, 3.0f };
        g.setColour (colour.withAlpha (0.5f));
        g.drawDashedLine ({ x, margin, x, bounds.getHeight() - margin }, dashes, 2, 1.0f);
        g.setColour (colour);
        g.setFont (smallFont (9.5f));
        g.drawText (text, juce::Rectangle<float> (x + 4.0f, textY, 80.0f, 11.0f), juce::Justification::centredLeft, false);
    };

    marker (threshold, colours::uiTeal, "Thresh " + juce::String (juce::roundToInt (threshold)), margin + 2.0f);

    if (upward)
    {
        marker (std::min (settings.upThresholdDb, threshold), colours::accentGreen, "Up " + juce::String (juce::roundToInt (std::min (settings.upThresholdDb, threshold))), margin + 14.0f);
        marker (settings.floorDb, colours::textSecondary, "Floor", margin + 26.0f);
    }

    g.setColour (colours::textPrimary);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    if (level > (float) low && level < (float) high)
    {
        const auto x = xFor ((double) level);
        const auto y = yFor (out ((double) level));

        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));

        if (upward && lift > 0.05f)
        {
            g.setColour (colours::accentGreen);
            g.setFont (smallFont());
            g.drawText ("+" + juce::String (lift, 1) + " dB", juce::Rectangle<float> (x - 64.0f, y - 20.0f, 56.0f, 12.0f), juce::Justification::centredRight, false);
        }
    }

    g.restoreState();
}
