#include "CompGraphs.h"
#include "CompProcessor.h"

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

// Phase 4 Wave 2: the six bands' parameters, and the band tabs' size.
const char* frequencyId (int band) { return CompProcessor::bandIds (band).frequency; }
const char* gainId (int band) { return CompProcessor::bandIds (band).gain; }
constexpr float tabWidth = 20.0f, tabHeight = 16.0f, tabGap = 2.0f;

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

DrawerTabs::DrawerTabs()
    : DrawerTabs (juce::StringArray { "Detector", "Character", "Upward", "Options", "History" })
{
}

DrawerTabs::DrawerTabs (juce::StringArray tabNames)
    : ViewTabs (std::move (tabNames))
{
}

// HistoryCaption --------------------------------------------------------------

void HistoryCaption::setText (const juce::String& newText, bool isPaused)
{
    if (newText != text || isPaused != paused)
    {
        text = newText;
        paused = isPaused;
        repaint();
    }
}

void HistoryCaption::paint (juce::Graphics& g)
{
    g.setFont (font (Weight::regular, 12.0f));
    g.setColour (paused ? colours::textSecondary : juce::Colour (0xffffcf07));
    g.drawText (text, getLocalBounds().toFloat(), juce::Justification::centredRight, false);
}

void HistoryCaption::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu() && onClick != nullptr)
        onClick();
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
    return { xFor ((double) params.get (frequencyId (band))), yFor ((double) params.get (gainId (band))) };
}

int DetectorCurve::bandType (int band) const
{
    return juce::roundToInt (params.get (CompProcessor::bandIds (band).type));
}

juce::Rectangle<float> DetectorCurve::tabBounds (int band) const
{
    return { 8.0f + (float) band * (tabWidth + tabGap), 7.0f, tabWidth, tabHeight };
}

void DetectorCurve::setSelected (int band)
{
    band = juce::jlimit (0, CompDsp::numDetectorBands - 1, band);

    if (band != selected)
    {
        selected = band;
        repaint();
    }
}

int DetectorCurve::bandAt (juce::Point<float> position) const
{
    auto best = -1;
    auto bestDistance = 12.0f;

    // The selected band first, so it wins over a handle under it.
    for (int i = 0; i < CompDsp::numDetectorBands; ++i)
    {
        const auto b = (selected + i) % CompDsp::numDetectorBands;

        if (bandType (b) == CompDsp::bandOff)
            continue;

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

    // Phase 4 Wave 2: 12, 24 or 48 dB/oct, a Butterworth response of order 2, 4 or 8.
    const auto highOrder = 2 << juce::jlimit (0, 2, juce::roundToInt (params.get ("hpSlope")));
    const auto lowOrder = 2 << juce::jlimit (0, 2, juce::roundToInt (params.get ("lpSlope")));

    std::array<matched::BandFilter, CompDsp::numDetectorBands> filters;

    for (int b = 0; b < CompDsp::numDetectorBands; ++b)
    {
        const auto type = bandType (b);

        matched::BandSpec spec;
        spec.type = type == CompDsp::bandLowShelf ? matched::BandType::lowShelf
                  : type == CompDsp::bandHighShelf ? matched::BandType::highShelf
                                                   : matched::BandType::band;
        spec.frequencyHz = (double) params.get (frequencyId (b));
        spec.gainDb = (double) params.get (gainId (b));
        spec.q = type == CompDsp::bandBell ? (double) params.get (CompProcessor::bandIds (b).q) : matched::butterworthQ;
        spec.order = 2;
        spec.on = type != CompDsp::bandOff && spec.gainDb != 0.0;
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
            const auto r = std::pow (hz / highpass, 2.0 * (double) highOrder);
            db += 10.0 * std::log10 (r / (1.0 + r));
        }

        if (lowpass < 23900.0)
            db += 10.0 * std::log10 (1.0 / (1.0 + std::pow (hz / lowpass, 2.0 * (double) lowOrder)));

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

    // A handle for each band that isn't Off, with its number; the selected one filled, on top.
    g.setFont (smallFont (9.0f));

    for (int i = 1; i <= CompDsp::numDetectorBands; ++i)
    {
        const auto b = (selected + i) % CompDsp::numDetectorBands;

        if (bandType (b) == CompDsp::bandOff)
            continue;

        const auto centre = handleFor (b);
        const auto active = params.get (gainId (b)) != 0.0f;
        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (centre);

        g.setColour (b == selected ? colours::bgWell.interpolatedWith (colours::uiBlue, 0.35f) : colours::bgWell);
        g.fillEllipse (circle);
        g.setColour (colours::uiBlue.withAlpha (active || b == selected ? 1.0f : 0.35f));
        g.drawEllipse (circle, 2.0f);
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (centre.x + 6.0f, centre.y + 3.0f, 10.0f, 11.0f), juce::Justification::centredLeft, false);
    }

    g.restoreState();

    if (highpass > 20.5)
    {
        auto text = "HP " + juce::String (juce::roundToInt (highpass)) + " Hz";

        if (highOrder > 2)
            text << " - " << juce::String (highOrder * 6) << " dB/oct";

        g.setFont (smallFont (9.0f));
        g.setColour (colours::uiBlue.withAlpha (highOrder > 2 ? 0.8f : 0.5f));
        g.drawText (text, juce::Rectangle<float> (xFor (highpass) + 4.0f, bounds.getBottom() - 16.0f, 120.0f, 11.0f), juce::Justification::centredLeft, false);
    }

    // The band tabs over the curve's top-left corner: Off bands dimmed, the selected one blue.
    const auto strip = tabBounds (0).getUnion (tabBounds (CompDsp::numDetectorBands - 1)).expanded (2.0f);
    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (strip, 4.0f);

    for (int b = 0; b < CompDsp::numDetectorBands; ++b)
    {
        const auto tab = tabBounds (b);

        if (b == selected)
        {
            g.setColour (colours::uiBlue);
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (colours::bgWell);
            g.setFont (font (Weight::semiBold, 10.5f));
        }
        else
        {
            g.setColour (bandType (b) == CompDsp::bandOff ? juce::Colour (0xff5a5a5a) : juce::Colour (0xffdddddd));
            g.setFont (font (Weight::medium, 10.5f));
        }

        g.drawText (juce::String (b + 1), tab, juce::Justification::centred, false);
    }
}

void DetectorCurve::mouseDown (const juce::MouseEvent& event)
{
    dragging = -1;

    for (int b = 0; b < CompDsp::numDetectorBands; ++b)
    {
        if (tabBounds (b).expanded (1.0f).contains (event.position))
        {
            setSelected (b);

            if (onSelect != nullptr)
                onSelect (b);

            return;
        }
    }

    dragging = bandAt (event.position);

    if (dragging >= 0)
    {
        if (dragging != selected)
        {
            setSelected (dragging);

            if (onSelect != nullptr)
                onSelect (dragging);
        }

        params.begin (frequencyId (dragging));
        params.begin (gainId (dragging));
    }
}

void DetectorCurve::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging < 0)
        return;

    params.set (frequencyId (dragging), (float) hzFor (event.position.x));
    params.set (gainId (dragging), (float) juce::jlimit (-curveDb, curveDb, dbFor (event.position.y)));
    repaint();
}

void DetectorCurve::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        params.end (frequencyId (dragging));
        params.end (gainId (dragging));
    }

    dragging = -1;
}

void DetectorCurve::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // The selected band's Q, while it's a Bell.
    if (bandType (selected) != CompDsp::bandBell)
        return;

    const auto* id = CompProcessor::bandIds (selected).q;
    const auto q = params.get (id) * std::exp (wheel.deltaY * 0.6f);

    params.begin (id);
    params.set (id, juce::jlimit (0.3f, 8.0f, q));
    params.end (id);
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
