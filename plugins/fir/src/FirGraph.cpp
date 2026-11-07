#include "FirGraph.h"

#include <fancy/plugin/ParamFormat.h>
#include <fancy/ui/Style.h>

#include <cmath>
#include <iterator>

using namespace fancy::ui;
using namespace fir;

namespace
{
// design/phase2-mockup-src/fir.html: lx and ly.
constexpr float plotLeft = 34.0f, plotRight = 10.0f, plotTop = 10.0f, plotBottom = 22.0f;
constexpr float pointRadius = 5.0f, grabRadius = 8.0f, curveGrab = 8.0f;
constexpr int analyzerIntervalMs = 33;

// Converts a learned profile, power per bin over the window's energy, to the level the graph
// shows, for the square-root Hann window at the current size.
double profileToLevelScale (int fftSize) noexcept
{
    auto sum = 0.0, energy = 0.0;

    for (int n = 0; n < fftSize; ++n)
    {
        const auto window = std::sqrt (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * n / fftSize));
        sum += window;
        energy += window * window;
    }

    return energy * (2.0 / sum) * (2.0 / sum);
}
} // namespace

juce::Colour modeColour (int mode)
{
    const juce::Colour colours[] { colours::accentPurple, colours::accentYellow, colours::uiTeal, colours::accentRed, colours::uiBlue };
    return colours[juce::jlimit (0, 4, mode)];
}

float FirGraph::xForHz (double hz, float width) noexcept
{
    return plotLeft + (width - plotLeft - plotRight) * (float) std::log10 (hz / 20.0) / 3.0f;
}

double FirGraph::hzForX (float x, float width) noexcept
{
    return 20.0 * std::pow (10.0, 3.0 * (double) ((x - plotLeft) / (width - plotLeft - plotRight)));
}

float FirGraph::yForDb (float db, float height, float top, float floor) noexcept
{
    return plotTop + (height - plotTop - plotBottom) * (top - db) / (top - floor);
}

float FirGraph::dbForY (float y, float height, float top, float floor) noexcept
{
    return top - (y - plotTop) / (height - plotTop - plotBottom) * (top - floor);
}

FirGraph::FirGraph (FirProcessor& processorToUse)
    : processor (processorToUse), analyzerThread (*this)
{
    processor.analyzerActive.store (true);
    analyzerThread.startThread (juce::Thread::Priority::low);
}

FirGraph::~FirGraph()
{
    analyzerThread.stopThread (2000);
    processor.analyzerActive.store (false);
}

void FirGraph::AnalyzerThread::run()
{
    fancy::dsp::SpectrumAnalyzer before, after;
    std::vector<float> scratch ((size_t) fancy::dsp::SampleFifo::capacity);

    while (! threadShouldExit())
    {
        graph.analyse (before, after, scratch);
        wait (analyzerIntervalMs);
    }
}

void FirGraph::analyse (fancy::dsp::SpectrumAnalyzer& before, fancy::dsp::SpectrumAnalyzer& after, std::vector<float>& scratch)
{
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    before.configure (rate, {});
    after.configure (rate, {});

    if (const auto count = processor.inputFifo.pop (scratch.data(), (int) scratch.size()); count > 0)
        before.addSamples (scratch.data(), count);

    if (const auto count = processor.outputFifo.pop (scratch.data(), (int) scratch.size()); count > 0)
        after.addSamples (scratch.data(), count);

    const auto width = columns.load (std::memory_order_relaxed);

    if (width <= 0)
        return;

    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    if ((int) inputSpectrum.size() != width || frozenSpectra)
        return;

    const auto fullWidth = plotLeft + (float) width + plotRight;

    for (int x = 0; x < width; ++x)
    {
        const auto low = hzForX (plotLeft + (float) x, fullWidth), high = hzForX (plotLeft + (float) x + 1.0f, fullWidth);

        inputSpectrum[(size_t) x] = before.levelBetween (low, high);
        outputSpectrum[(size_t) x] = after.levelBetween (low, high);
    }
}

void FirGraph::resized()
{
    const auto width = juce::jmax (1, getWidth() - (int) (plotLeft + plotRight));

    if (! frozenSpectra)
    {
        const juce::SpinLock::ScopedLockType lock (spectrumLock);
        inputSpectrum.assign ((size_t) width, -200.0f);
        outputSpectrum.assign ((size_t) width, -200.0f);
    }

    columns.store (width);
}

void FirGraph::update()
{
    auto& apvts = processor.getApvts();
    const auto value = [&] (const char* id) { return apvts.getRawParameterValue (id)->load (std::memory_order_relaxed); };

    mode = processor.getMode();
    editMode = processor.getEditMode();
    topDb = value ("graphTop");
    floorDb = value ("graphFloor");
    showAnalysis = value ("showAnalysis") >= 0.5f;

    repaint();
}

void FirGraph::showPreviewSpectra()
{
    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    frozenSpectra = true;

    const auto width = juce::jmax (1, getWidth() - (int) (plotLeft + plotRight));

    inputSpectrum.assign ((size_t) width, -200.0f);
    outputSpectrum.assign ((size_t) width, -200.0f);

    // The mockup's made-up programme and noise, with the same random generator.
    auto seed = (juce::int64) 11;

    const auto random = [&seed]
    {
        seed = (seed * 16807) % 2147483647;
        return (float) seed / 2147483647.0f;
    };

    for (int x = 0; x < width; ++x)
    {
        const auto hz = (float) hzForX (plotLeft + (float) x, (float) getWidth());
        const auto octave = std::log10 (hz);

        const auto signal = -30.0f - 14.0f * std::pow (std::abs (octave - 2.6f), 1.6f) - 4.0f * random();
        const auto noise = -62.0f - 6.0f * (octave - 1.5f) + (std::abs (hz - 60.0f) < 6.0f ? 20.0f : 0.0f)
                         + (std::abs (hz - 120.0f) < 8.0f ? 12.0f : 0.0f) - 3.0f * random();

        inputSpectrum[(size_t) x] = 10.0f * std::log10 (std::pow (10.0f, signal / 10.0f) + std::pow (10.0f, noise / 10.0f));
        outputSpectrum[(size_t) x] = signal - 1.0f - 2.0f * random();
    }
}

bool FirGraph::editsCurve() const noexcept
{
    return mode == eq || mode == gate || mode == compressor;
}

float FirGraph::yFor (float db) const noexcept
{
    return yForDb (db, (float) getHeight(), topDb, floorDb);
}

float FirGraph::dbAt (float y) const noexcept
{
    return dbForY (y, (float) getHeight(), topDb, floorDb);
}

double FirGraph::profileDbAt (double hz) const
{
    auto& shared = processor.getShared();

    if (! shared.hasProfile.load())
        return -200.0;

    const auto position = juce::jlimit (0.0, (double) (profilePoints - 1), hz / (profileHighHz / (double) (profilePoints - 1)));
    const auto index = juce::jmin ((int) position, profilePoints - 2);
    const auto fraction = position - (double) index;

    const auto power = (double) shared.profile[(size_t) index].load() + fraction * (double) (shared.profile[(size_t) index + 1].load() - shared.profile[(size_t) index].load());
    const auto offset = (double) processor.getApvts().getRawParameterValue ("offset")->load();

    return 10.0 * std::log10 (std::max (power * profileToLevelScale (processor.getFftSize()), 1.0e-20)) + offset;
}

int FirGraph::pointAt (juce::Point<float> position) const
{
    if (! editsCurve() || editMode == fir::freehand)
        return -1;

    const auto& points = processor.getPoints (mode);

    for (int i = (int) points.size() - 1; i >= 0; --i)
    {
        const juce::Point<float> handle (xForHz (points[(size_t) i].hz, (float) getWidth()), yFor ((float) points[(size_t) i].db));

        if (handle.getDistanceFrom (position) <= grabRadius)
            return i;
    }

    return -1;
}

void FirGraph::paintGrid (juce::Graphics& g)
{
    const auto width = (float) getWidth(), height = (float) getHeight();
    const auto bottom = height - plotBottom;

    g.setFont (font (Weight::regular, 11.0f));

    for (const auto hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const auto x = xForHz (hz, width);

        g.setColour (colours::bgBody);
        g.fillRect (x - 0.5f, 8.0f, 1.0f, bottom - 8.0f);

        g.setColour (colours::textSecondary);
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 18.0f, height - 16.0f, 36.0f, 14.0f), juce::Justification::centred, false);
    }

    // Lines every 24 dB from the top, and the bottom of the range.
    std::vector<float> levels;

    for (auto db = std::floor (topDb / 24.0f) * 24.0f; db > floorDb + 6.0f; db -= 24.0f)
        levels.push_back (db);

    if (levels.empty() || levels.front() != topDb)
        levels.insert (levels.begin(), topDb);

    levels.push_back (floorDb);

    for (const auto db : levels)
    {
        const auto y = yFor (db);

        g.setColour (db == 0.0f ? colours::grey5 : colours::bgBody);
        g.fillRect (plotLeft, y - 0.5f, width - plotLeft - plotRight, 1.0f);

        g.setColour (colours::textSecondary);
        g.drawText ((db > 0.0f ? "+" : "") + juce::String (juce::roundToInt (db)),
                    juce::Rectangle<float> (0.0f, y - 7.0f, plotLeft - 6.0f, 14.0f), juce::Justification::centredRight, false);
    }
}

void FirGraph::paintAnalysis (juce::Graphics& g)
{
    const juce::SpinLock::ScopedTryLockType lock (spectrumLock);

    if (! lock.isLocked() || inputSpectrum.empty())
        return;

    const auto bottom = (float) getHeight() - plotBottom;

    juce::Path fill, line;

    fill.startNewSubPath (plotLeft, bottom);

    for (int x = 0; x < (int) inputSpectrum.size(); ++x)
        fill.lineTo (plotLeft + (float) x, juce::jlimit (plotTop, bottom, yFor (inputSpectrum[(size_t) x])));

    fill.lineTo (plotLeft + (float) inputSpectrum.size(), bottom);
    fill.closeSubPath();

    g.setColour (colours::textSecondary.withAlpha (0.22f));
    g.fillPath (fill);

    for (int x = 0; x < (int) outputSpectrum.size(); ++x)
    {
        const auto y = juce::jlimit (plotTop, bottom, yFor (outputSpectrum[(size_t) x]));

        if (x == 0)
            line.startNewSubPath (plotLeft, y);
        else
            line.lineTo (plotLeft + (float) x, y);
    }

    g.setColour (colours::accentOrange);
    g.strokePath (line, juce::PathStrokeType (1.3f));
}

void FirGraph::paintModeLayer (juce::Graphics& g)
{
    const auto width = (float) getWidth();
    const auto bottom = (float) getHeight() - plotBottom;
    const auto colour = modeColour (mode);

    if (mode == convolve)
        return;

    if (mode == compressor)
    {
        // Gain reduction hanging from the 0 dB line.
        auto& shared = processor.getShared();
        const auto zero = yFor (0.0f);

        g.setColour (colours::accentYellow.withAlpha (0.6f));

        for (int column = 0; column < displayColumns; ++column)
        {
            const auto reduction = shared.reduction[(size_t) column].load (std::memory_order_relaxed);

            if (reduction <= 0.05f)
                continue;

            const auto left = xForHz (displayHz (column), width), right = xForHz (displayHz (column + 1), width);
            g.fillRect (left, zero, right - left, yFor (-reduction) - zero);
        }
    }

    juce::Path line;
    auto started = false;

    for (auto x = plotLeft; x <= width - plotRight; x += 1.0f)
    {
        const auto hz = hzForX (x, width);
        const auto db = mode == subtract ? (float) profileDbAt (hz) : (float) processor.curveDbAt (mode, hz);

        if (mode == subtract && db < -190.0f)
            return;

        const auto y = juce::jlimit (plotTop, bottom, yFor (db));

        if (! started)
            line.startNewSubPath (x, y);
        else
            line.lineTo (x, y);

        started = true;
    }

    auto fill = line;
    fill.lineTo (width - plotRight, bottom);
    fill.lineTo (plotLeft, bottom);
    fill.closeSubPath();

    g.setColour (colour.withAlpha (0.14f));
    g.fillPath (fill);

    g.setColour (colour);
    g.strokePath (line, juce::PathStrokeType (2.0f));

    if (! editsCurve() || editMode == fir::freehand)
        return;

    for (const auto& point : processor.getPoints (mode))
    {
        const juce::Point<float> centre (xForHz (point.hz, width), yFor ((float) point.db));
        const auto circle = juce::Rectangle<float> (pointRadius * 2.0f, pointRadius * 2.0f).withCentre (centre);

        g.setColour (colours::textPrimary);
        g.fillEllipse (circle);
        g.setColour (colour);
        g.drawEllipse (circle, 2.0f);
    }
}

void FirGraph::paintLegend (juce::Graphics& g)
{
    const auto width = (float) getWidth();

    std::vector<std::pair<juce::String, juce::Colour>> items { { "input", colours::textSecondary }, { "output", colours::accentOrange } };

    if (mode == eq)
        items.push_back ({ "curve", modeColour (mode) });
    else if (mode == gate || mode == compressor)
        items.push_back ({ "threshold", modeColour (mode) });
    else if (mode == subtract)
        items.push_back ({ "noise profile", modeColour (mode) });

    if (mode == compressor)
        items.push_back ({ "reduction", colours::accentYellow });

    g.setFont (font (Weight::regular, 11.0f));

    auto x = width - plotRight;

    for (auto it = items.rbegin(); it != items.rend(); ++it)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (g.getCurrentFont(), it->first, 0.0f, 0.0f);

        const auto textWidth = std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 2.0f;

        x -= textWidth;
        g.setColour (colours::textSecondary);
        g.drawText (it->first, juce::Rectangle<float> (x, plotTop, textWidth, 14.0f), juce::Justification::centredLeft, false);

        x -= 14.0f;
        g.setColour (it->second);
        g.fillRect (x, plotTop + 5.5f, 10.0f, 3.0f);

        x -= 12.0f;
    }
}

void FirGraph::paint (juce::Graphics& g)
{
    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    paintGrid (g);

    if (showAnalysis)
        paintAnalysis (g);

    paintModeLayer (g);
    paintLegend (g);
}

// Editing ---------------------------------------------------------------------------------------

void FirGraph::drawFreehand (juce::Point<float> from, juce::Point<float> to)
{
    auto table = processor.getFreehand (mode);

    const auto width = (float) getWidth();
    const auto first = curvePosition (hzForX (juce::jmin (from.x, to.x), width));
    const auto last = curvePosition (hzForX (juce::jmax (from.x, to.x), width));

    for (auto i = (int) std::ceil (first); i <= (int) std::floor (last) && i < curvePoints; ++i)
    {
        const auto x = xForHz (curveHz (i), width);
        const auto t = to.x == from.x ? 1.0f : (x - from.x) / (to.x - from.x);

        table[(size_t) i] = juce::jlimit (-150.0f, 48.0f, dbAt (from.y + (to.y - from.y) * t));
    }

    // A single click still sets the nearest value.
    const auto nearest = juce::jlimit (0, curvePoints - 1, juce::roundToInt (curvePosition (hzForX (to.x, width))));
    table[(size_t) nearest] = juce::jlimit (-150.0f, 48.0f, dbAt (to.y));

    processor.setFreehand (mode, table);
}

void FirGraph::mouseDown (const juce::MouseEvent& event)
{
    dragPoint = -1;
    draggingCurve = false;
    drawing = false;
    lastMouse = event.position;

    if (! editsCurve() || event.position.x < plotLeft || event.position.y > (float) getHeight() - plotBottom)
        return;

    if (editMode == fir::freehand)
    {
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            draggingCurve = true;
            dragStartTable = processor.getFreehand (mode);
            return;
        }

        drawing = true;
        drawFreehand (event.position, event.position);
        return;
    }

    auto points = processor.getPoints (mode);
    const auto index = pointAt (event.position);

    if (event.mods.isPopupMenu())
    {
        if (index >= 0)
            typePoint (index);

        return;
    }

    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
    {
        draggingCurve = true;
        dragStartPoints = points;
        return;
    }

    if (index >= 0)
    {
        if (event.mods.isAltDown())
        {
            points.erase (points.begin() + index);
            processor.setPoints (mode, points);
            return;
        }

        dragPoint = index;
        dragStartHz = points[(size_t) index].hz;
        return;
    }

    // A click on the curve adds a point there.
    const auto hz = hzForX (event.position.x, (float) getWidth());
    const auto curveY = yFor ((float) processor.curveDbAt (mode, hz));

    if (std::abs (curveY - event.position.y) > curveGrab || (int) points.size() >= maxCurvePoints)
        return;

    points.push_back ({ hz, (double) dbAt (event.position.y) });
    sortPoints (points);
    processor.setPoints (mode, points);

    dragPoint = pointAt (event.position);
    dragStartHz = hz;
}

void FirGraph::mouseDrag (const juce::MouseEvent& event)
{
    const auto width = (float) getWidth();

    if (drawing)
    {
        drawFreehand (lastMouse, event.position);
        lastMouse = event.position;
        return;
    }

    if (draggingCurve)
    {
        const auto shift = dbAt ((float) event.getMouseDownY() + (float) event.getDistanceFromDragStartY()) - dbAt ((float) event.getMouseDownY());

        if (editMode == fir::freehand)
        {
            auto table = dragStartTable;

            for (auto& value : table)
                value = juce::jlimit (-150.0f, 48.0f, value + shift);

            processor.setFreehand (mode, table);
        }
        else
        {
            auto points = dragStartPoints;

            for (auto& point : points)
                point.db += (double) shift;

            processor.setPoints (mode, points);
        }

        return;
    }

    if (dragPoint < 0)
        return;

    auto points = processor.getPoints (mode);

    if (dragPoint >= (int) points.size())
        return;

    auto& point = points[(size_t) dragPoint];

    point.hz = event.mods.isShiftDown() ? dragStartHz : juce::jlimit (curveLowHz, 20000.0, hzForX (event.position.x, width));
    point.db = (double) dbAt (juce::jlimit (plotTop, (float) getHeight() - plotBottom, event.position.y));

    const auto moved = point;

    processor.setPoints (mode, points);

    // Sorting may have moved it.
    const auto& sorted = processor.getPoints (mode);

    for (int i = 0; i < (int) sorted.size(); ++i)
        if (sorted[(size_t) i] == moved)
            dragPoint = i;
}

void FirGraph::mouseUp (const juce::MouseEvent&)
{
    dragPoint = -1;
    draggingCurve = false;
    drawing = false;
}

void FirGraph::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.position.x < plotLeft)
    {
        // The axis labels at the ends set the range.
        if (std::abs (event.position.y - yFor (topDb)) < 10.0f)
            typeAxis (true);
        else if (std::abs (event.position.y - yFor (floorDb)) < 10.0f)
            typeAxis (false);

        return;
    }

    const auto index = pointAt (event.position);

    if (index < 0)
        return;

    auto points = processor.getPoints (mode);
    points[(size_t) index].db = 0.0;
    processor.setPoints (mode, points);
}

void FirGraph::typePoint (int index)
{
    const auto point = processor.getPoints (mode)[(size_t) index];

    auto* window = new juce::AlertWindow ("Point", {}, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("hz", fancy::formatHz ((float) point.hz), "Frequency");
    window->addTextEditor ("db", fancy::formatFixed ((float) point.db, 1) + " dB", "Gain");
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    const auto curve = mode;

    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([safeThis = juce::Component::SafePointer<FirGraph> (this), window, index, curve] (int result)
                             {
                                 if (safeThis == nullptr || result != 1)
                                     return;

                                 auto points = safeThis->processor.getPoints (curve);

                                 if (index >= (int) points.size())
                                     return;

                                 const auto hzText = window->getTextEditorContents ("hz");
                                 const auto dbText = window->getTextEditorContents ("db");

                                 if (fancy::isValueText (hzText))
                                     points[(size_t) index].hz = (double) fancy::parseHz (hzText);

                                 if (fancy::isValueText (dbText))
                                     points[(size_t) index].db = (double) fancy::parseNumber (dbText);

                                 safeThis->processor.setPoints (curve, points);
                             }),
                             true);
}

void FirGraph::typeAxis (bool top)
{
    auto& parameter = *processor.getApvts().getParameter (top ? "graphTop" : "graphFloor");

    auto* window = new juce::AlertWindow (top ? "Graph top" : "Analysis floor", {}, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("db", parameter.getCurrentValueAsText(), "dB");
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([&parameter, window] (int result)
                             {
                                 const auto text = window->getTextEditorContents ("db");

                                 if (result != 1 || ! fancy::isValueText (text))
                                     return;

                                 parameter.beginChangeGesture();
                                 parameter.setValueNotifyingHost (parameter.convertTo0to1 (fancy::parseNumber (text)));
                                 parameter.endChangeGesture();
                             }),
                             true);
}
