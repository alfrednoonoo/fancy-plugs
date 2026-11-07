#include "EqGraph.h"

#include <fancy/Decibels.h>
#include <fancy/ui/Style.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>

using namespace fancy::ui;

namespace
{
constexpr double axisOffsetHz = 40.0;
const double axisLow = std::log2 (20.0 + axisOffsetHz);
const double axisHigh = std::log2 (24000.0 + axisOffsetHz);

constexpr float rangeLinePosition = 0.855f;     // of half the height, from Figma's two frames
constexpr float handleRadius = 7.5f, selectedRadius = 10.5f, hitRadius = 13.0f;
constexpr float fineDivisor = 8.0f;
constexpr int analyzerIntervalMs = 33;
constexpr float spectrumZeroLine = -60.0f;      // analyzer dB on the graph's 0 dB line

constexpr float labelOpacity = 0.4f;
constexpr float gridOpacity = 0.2f;

constexpr float bandFillOpacity[] { 0.3f, 0.8f, 0.3f, 0.3f, 0.3f, 0.3f, 0.3f };

float spectrumScale (float rangeDb) noexcept
{
    return juce::jmax (5.0f, 60.0f / rangeDb);
}
} // namespace

double xForHz (double hz, double width) noexcept
{
    return width * (std::log2 (hz + axisOffsetHz) - axisLow) / (axisHigh - axisLow);
}

double hzForX (double x, double width) noexcept
{
    return std::pow (2.0, axisLow + (axisHigh - axisLow) * x / width) - axisOffsetHz;
}

juce::Colour bandColour (int band)
{
    static const juce::Colour palette[] { colours::accentRed, colours::accentPurple, colours::accentBlue, colours::accentTeal,
                                          colours::accentGreen, colours::accentYellow, colours::accentOrange };

    return palette[(size_t) band % std::size (palette)];
}

EqGraph::EqGraph (EqProcessor& processorToUse)
    : processor (processorToUse), analyzerThread (*this)
{
    processor.analyzerActive.store (true);
    analyzerThread.startThread (juce::Thread::Priority::low);
}

EqGraph::~EqGraph()
{
    analyzerThread.stopThread (2000);
    processor.analyzerActive.store (false);
}

void EqGraph::AnalyzerThread::run()
{
    eq::SpectrumAnalyzer analyzer;
    std::vector<float> scratch ((size_t) eq::SampleFifo::capacity);

    while (! threadShouldExit())
    {
        graph.analyse (analyzer, scratch);
        wait (analyzerIntervalMs);
    }
}

void EqGraph::analyse (eq::SpectrumAnalyzer& analyzer, std::vector<float>& scratch)
{
    auto& apvts = processor.getApvts();

    const auto read = [&] (const char* id) { return apvts.getRawParameterValue (id)->load (std::memory_order_relaxed); };

    constexpr int sizes[] { 4096, 8192, 16384 };

    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    eq::SpectrumAnalyzer::Settings settings;
    settings.size = sizes[juce::jlimit (0, 2, juce::roundToInt (read ("anaFft")))];
    settings.window = read ("anaWindow") >= 0.5f ? eq::SpectrumAnalyzer::Window::blackmanHarris : eq::SpectrumAnalyzer::Window::hann;
    settings.hopDivisor = read ("anaOverlap") >= 0.5f ? 4 : 2;
    settings.smoothing = (double) read ("anaSmooth") * 0.01;
    settings.pinkTilt = read ("anaTilt") >= 0.5f;

    analyzer.configure (rate, settings);

    const auto count = processor.analyzerFifo.pop (scratch.data(), (int) scratch.size());

    if (count > 0)
        analyzer.addSamples (scratch.data(), count);

    if (read ("anaPeak") < 0.5f)
        analyzer.resetPeaks();

    const auto width = columns.load (std::memory_order_relaxed);

    if (width <= 0)
        return;

    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    if ((int) spectrum.size() != width || frozenSpectrum)
        return;

    for (int x = 0; x < width; ++x)
    {
        const auto low = hzForX ((double) x, (double) width);
        const auto high = hzForX ((double) x + 1.0, (double) width);

        spectrum[(size_t) x] = analyzer.levelBetween (low, high);
        peaks[(size_t) x] = analyzer.peakBetween (low, high);
    }
}

void EqGraph::resized()
{
    const auto width = getWidth();

    for (auto& band : bands)
        band.db.assign ((size_t) width, 0.0f);

    composite.assign ((size_t) width, 0.0f);
    phase.assign ((size_t) width, 0.0f);

    if (! frozenSpectrum)
    {
        const juce::SpinLock::ScopedLockType lock (spectrumLock);
        spectrum.assign ((size_t) width, -200.0f);
        peaks.assign ((size_t) width, -200.0f);
    }

    columns.store (width);
    needsRecalculation = true;
}

void EqGraph::setDisplayRange (float newRangeDb)
{
    if (newRangeDb != rangeDb)
    {
        rangeDb = newRangeDb;
        needsRecalculation = true;
    }
}

void EqGraph::setSelectedBand (int band)
{
    if (band != selectedBand)
    {
        selectedBand = band;
        repaint();
    }
}

void EqGraph::showPreviewSpectrum()
{
    const auto width = getWidth();

    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    frozenSpectrum = true;
    spectrum.resize ((size_t) width);

    // Falling with frequency and uneven, like the line in the mockup.
    for (int x = 0; x < width; ++x)
    {
        const auto position = (float) x / (float) width;
        const auto ripple = 5.0f * std::sin (41.0f * position) + 4.0f * std::sin (97.0f * position + 1.0f) + 3.0f * std::sin (211.0f * position);

        spectrum[(size_t) x] = -82.0f - 35.0f * position * position + ripple * (0.4f + position);
    }

    repaint();
}

float EqGraph::yForDb (float db) const noexcept
{
    const auto half = (float) getHeight() * 0.5f;
    return half - db * half * rangeLinePosition / rangeDb;
}

float EqGraph::dbForY (float y) const noexcept
{
    const auto half = (float) getHeight() * 0.5f;
    return (half - y) * rangeDb / (half * rangeLinePosition);
}

float EqGraph::spectrumForDb (float db) const noexcept
{
    return spectrumZeroLine + db * spectrumScale (rangeDb);
}

float EqGraph::yForSpectrum (float db) const noexcept
{
    return yForDb ((db - spectrumZeroLine) / spectrumScale (rangeDb));
}

void EqGraph::update()
{
    auto& apvts = processor.getApvts();

    const auto read = [&] (const char* id) { return apvts.getRawParameterValue (id)->load (std::memory_order_relaxed); };

    const auto newGrid = read ("showGrid") >= 0.5f;
    const auto newPhase = read ("showPhase") >= 0.5f;
    const auto newMode = juce::roundToInt (read ("analyzer"));

    showPeak = read ("anaPeak") >= 0.5f;

    if (newGrid != showGrid || newPhase != showPhase || newMode != analyzerMode)
    {
        showGrid = newGrid;
        showPhase = newPhase;
        analyzerMode = newMode;
        needsRecalculation = true;
    }

    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    if (rate != lastRate)
    {
        lastRate = rate;
        needsRecalculation = true;
    }

    for (int b = 0; b < eq::maxBands; ++b)
    {
        const auto spec = processor.readBand (b);
        const auto used = processor.isBandUsed (b);

        if (! (spec == lastSpecs[(size_t) b]) || used != lastUsed[(size_t) b])
        {
            lastSpecs[(size_t) b] = spec;
            lastUsed[(size_t) b] = used;
            needsRecalculation = true;
        }
    }

    if (needsRecalculation)
        recalculate();

    if (analyzerMode != 0)
        repaint();
}

void EqGraph::recalculate()
{
    needsRecalculation = false;

    const auto width = getWidth();

    if (width <= 0)
        return;

    std::fill (composite.begin(), composite.end(), 0.0f);
    std::fill (phase.begin(), phase.end(), 0.0f);

    const auto linear = processor.readTargets().linearPhase;

    for (int b = 0; b < eq::maxBands; ++b)
    {
        auto& band = bands[(size_t) b];

        band.spec = lastSpecs[(size_t) b];
        band.used = lastUsed[(size_t) b];

        if (! band.used)
            continue;

        // A band that's switched off still shows where it is.
        auto shape = band.spec;
        shape.on = true;

        const auto filter = eq::design (shape, lastRate);

        for (int x = 0; x < width; ++x)
        {
            const auto hz = juce::jmin (hzForX ((double) x, (double) width), 0.499 * lastRate);
            const auto response = filter.responseAt (2.0 * std::numbers::pi * hz / lastRate);
            const auto db = (float) fancy::gainToDb (std::abs (response), -200.0);

            band.db[(size_t) x] = band.spec.on ? db : 0.0f;

            if (band.spec.on)
            {
                composite[(size_t) x] += db;
                phase[(size_t) x] += linear ? 0.0f : (float) std::arg (response);
            }
        }

        const auto w = 2.0 * std::numbers::pi * juce::jmin (band.spec.frequencyHz, 0.499 * lastRate) / lastRate;
        const auto handleDb = (float) fancy::gainToDb (std::abs (filter.responseAt (w)), -200.0);

        band.handle = { (float) xForHz (band.spec.frequencyHz, (double) width),
                        juce::jlimit (selectedRadius, (float) getHeight() - selectedRadius, yForDb (handleDb)) };
    }

    repaint();
}

void EqGraph::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgHeader);

    if (showGrid)
        paintGrid (g);

    paintLabels (g);

    if (analyzerMode != 0)
        paintSpectrum (g);

    paintBands (g);
    paintHandles (g);
}

void EqGraph::paintGrid (juce::Graphics& g)
{
    const auto width = (float) getWidth();
    const auto height = (float) getHeight();

    g.setColour (juce::Colours::white.withAlpha (gridOpacity));

    const auto step = rangeDb <= 6.0f ? 3.0f : 6.0f;

    for (auto db = -rangeDb; db <= rangeDb + 0.1f; db += step)
        g.fillRect (0.0f, yForDb (db) - 0.25f, width, 0.5f);

    for (const auto decade : { 10.0, 100.0, 1000.0, 10000.0 })
    {
        for (int digit = 1; digit < 10; ++digit)
        {
            const auto hz = decade * digit;

            // Figma has no lines between 50 and 100 Hz.
            if (hz == 50.0 || (hz >= 100.0 && hz <= 20000.0))
                g.fillRect ((float) xForHz (hz, (double) width) - 0.25f, 0.0f, 0.5f, height);
        }
    }
}

void EqGraph::paintLabels (juce::Graphics& g)
{
    const auto width = getWidth();
    const auto height = getHeight();

    g.setFont (font (Weight::medium, 12.0f));
    g.setColour (juce::Colours::white.withAlpha (labelOpacity));

    const auto step = rangeDb <= 6.0f ? 3.0f : 6.0f;

    for (auto db = -rangeDb; db <= rangeDb + 0.1f; db += step)
    {
        const auto y = juce::roundToInt (yForDb (db)) - 17;
        const auto left = (db >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (db));
        const auto spectrumDb = juce::roundToInt (spectrumForDb (db));
        const auto right = (spectrumDb > 0 ? "+" : "") + juce::String (spectrumDb);

        g.drawText (left, 4, y, 30, 14, juce::Justification::centredLeft, false);
        g.drawText (right, width - 44, y, 40, 14, juce::Justification::centredRight, false);
    }

    const std::pair<double, const char*> labels[] { { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" }, { 300.0, "300" }, { 500.0, "500" },
                                                    { 1000.0, "1.0k" }, { 2000.0, "2.0k" }, { 3000.0, "3.0k" }, { 5000.0, "5.0k" },
                                                    { 10000.0, "10.0k" }, { 20000.0, "20.0k" } };

    for (const auto& [hz, text] : labels)
    {
        const auto x = juce::roundToInt (xForHz (hz, (double) width)) - 4;
        g.drawText (text, x - 40, height - 14, 40, 14, juce::Justification::centredRight, false);
    }

    if (analyzerMode != 0)
        g.drawText (analyzerMode == 1 ? "pre" : "post", 40, 4, 30, 14, juce::Justification::centredLeft, false);
}

void EqGraph::paintSpectrum (juce::Graphics& g)
{
    const juce::SpinLock::ScopedTryLockType lock (spectrumLock);

    if (! lock.isLocked())
        return;

    const auto bottom = (float) getHeight() + 2.0f;

    const auto stroke = [&] (const std::vector<float>& values, float opacity)
    {
        juce::Path path;

        for (int x = 0; x < (int) values.size(); ++x)
        {
            const auto y = juce::jmin (bottom, yForSpectrum (values[(size_t) x]));

            if (x == 0)
                path.startNewSubPath ((float) x, y);
            else
                path.lineTo ((float) x, y);
        }

        g.setColour (colours::accentOrange.withAlpha (opacity));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    };

    stroke (spectrum, 0.7f);

    if (showPeak && ! peaks.empty())
        stroke (peaks, 0.3f);
}

void EqGraph::paintBands (juce::Graphics& g)
{
    const auto width = getWidth();
    const auto zero = yForDb (0.0f);
    const auto floorDb = dbForY ((float) getHeight() + 4.0f);
    const auto topDb = dbForY (-4.0f);

    if (width <= 0)
        return;

    const auto clampDb = [&] (float db) { return juce::jlimit (floorDb, topDb, db); };

    for (int b = 0; b < eq::maxBands; ++b)
    {
        const auto& band = bands[(size_t) b];

        if (! band.used || ! band.spec.on)
            continue;

        juce::Path curve;
        curve.startNewSubPath (0.0f, yForDb (clampDb (band.db[0])));

        for (int x = 1; x < width; ++x)
            curve.lineTo ((float) x, yForDb (clampDb (band.db[(size_t) x])));

        auto region = curve;
        region.lineTo ((float) width, zero);
        region.lineTo (0.0f, zero);
        region.closeSubPath();

        const auto colour = bandColour (b);

        g.setColour (colour.withAlpha (bandFillOpacity[(size_t) b % std::size (bandFillOpacity)]));
        g.fillPath (region);

        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (1.0f));
    }

    // Figma's 0 dB line takes each band's colour in turn.
    std::vector<std::pair<float, juce::Colour>> stops;

    for (int b = 0; b < eq::maxBands; ++b)
        if (bands[(size_t) b].used && bands[(size_t) b].spec.on)
            stops.emplace_back ((float) xForHz (bands[(size_t) b].spec.frequencyHz, (double) width) / (float) width, bandColour (b));

    if (! stops.empty())
    {
        std::sort (stops.begin(), stops.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        juce::ColourGradient gradient (stops.front().second, 0.0f, zero, stops.back().second, (float) width, zero, false);

        for (const auto& [position, colour] : stops)
            gradient.addColour (juce::jlimit (0.0, 1.0, (double) position), colour);

        g.setGradientFill (gradient);
        g.fillRect (0.0f, zero - 0.5f, (float) width, 1.0f);
    }

    if (showPhase)
    {
        juce::Path path;

        for (int x = 0; x < width; ++x)
        {
            const auto wrapped = std::remainder (phase[(size_t) x], 2.0f * std::numbers::pi_v<float>);
            const auto y = yForDb (wrapped / std::numbers::pi_v<float> * rangeDb);

            if (x == 0)
                path.startNewSubPath ((float) x, y);
            else
                path.lineTo ((float) x, y);
        }

        g.setColour (colours::accentYellow.withAlpha (0.7f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

    juce::Path total;

    for (int x = 0; x < width; ++x)
    {
        const auto y = yForDb (clampDb (composite[(size_t) x]));

        if (x == 0)
            total.startNewSubPath ((float) x, y);
        else
            total.lineTo ((float) x, y);
    }

    g.setColour (juce::Colours::white);
    g.strokePath (total, juce::PathStrokeType (1.0f));
}

void EqGraph::paintHandles (juce::Graphics& g)
{
    g.setFont (font (Weight::semiBold, 11.0f));

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int b = 0; b < eq::maxBands; ++b)
        {
            const auto& band = bands[(size_t) b];
            const auto selected = b == selectedBand;

            if (! band.used || selected != (pass == 1))
                continue;

            const auto centre = band.handle;
            const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (centre);
            const auto colour = band.spec.on ? bandColour (b) : colours::grey6;

            if (selected)
            {
                g.setColour (juce::Colours::white);
                g.fillEllipse (juce::Rectangle<float> (selectedRadius * 2.0f, selectedRadius * 2.0f).withCentre (centre));
            }

            g.setColour (colour);
            g.fillEllipse (circle);

            g.setColour (selected ? juce::Colours::white : juce::Colours::black);
            g.drawText (juce::String (b + 1), circle, juce::Justification::centred, false);
        }
    }
}

juce::RangedAudioParameter& EqGraph::bandParameter (int band, const char* suffix)
{
    return *processor.getApvts().getParameter (EqProcessor::id (band, suffix));
}

void EqGraph::setValue (int band, const char* suffix, float value)
{
    auto& parameter = bandParameter (band, suffix);
    parameter.setValueNotifyingHost (parameter.convertTo0to1 (value));
}

int EqGraph::bandAt (juce::Point<float> position) const noexcept
{
    auto closest = -1;
    auto closestDistance = hitRadius;

    for (int b = 0; b < eq::maxBands; ++b)
    {
        const auto& band = bands[(size_t) b];

        if (! band.used)
            continue;

        // The selected handle is drawn on top, so it wins ties.
        const auto distance = band.handle.getDistanceFrom (position) - (b == selectedBand ? 0.5f : 0.0f);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closest = b;
        }
    }

    return closest;
}

void EqGraph::select (int band)
{
    setSelectedBand (band);

    if (onBandSelected != nullptr)
        onBandSelected (band);
}

void EqGraph::mouseDown (const juce::MouseEvent& event)
{
    const auto band = bandAt (event.position);

    if (event.mods.isPopupMenu())
    {
        if (band >= 0)
        {
            select (band);
            showBandMenu (band);
        }

        return;
    }

    if (band < 0)
        return;

    select (band);

    dragBand = band;
    draggingQ = event.mods.isAltDown();
    dragPosition = bands[(size_t) band].handle;
    lastMouse = event.position;

    if (draggingQ)
    {
        bandParameter (band, "q").beginChangeGesture();
    }
    else
    {
        bandParameter (band, "freq").beginChangeGesture();
        bandParameter (band, "gain").beginChangeGesture();
    }
}

void EqGraph::mouseDrag (const juce::MouseEvent& event)
{
    if (dragBand < 0)
        return;

    const auto scale = event.mods.isShiftDown() ? 1.0f / fineDivisor : 1.0f;
    const auto delta = (event.position - lastMouse) * scale;

    lastMouse = event.position;

    if (draggingQ)
    {
        auto& q = bandParameter (dragBand, "q");
        q.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q.getValue() - delta.y / 200.0f));
        return;
    }

    dragPosition += delta;

    const auto hz = juce::jlimit (20.0, 24000.0, hzForX ((double) dragPosition.x, (double) getWidth()));

    setValue (dragBand, "freq", (float) hz);

    if (eq::hasGain (bands[(size_t) dragBand].spec.type))
        setValue (dragBand, "gain", juce::jlimit (-24.0f, 24.0f, dbForY (dragPosition.y)));
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragBand < 0)
        return;

    if (draggingQ)
    {
        bandParameter (dragBand, "q").endChangeGesture();
    }
    else
    {
        bandParameter (dragBand, "freq").endChangeGesture();
        bandParameter (dragBand, "gain").endChangeGesture();
    }

    dragBand = -1;
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        return;

    const auto band = bandAt (event.position);

    if (band >= 0)
    {
        processor.resetBand (band);
        return;
    }

    const auto hz = juce::jlimit (20.0, 24000.0, hzForX ((double) event.position.x, (double) getWidth()));
    const auto added = processor.addBand ((float) hz, juce::jlimit (-24.0f, 24.0f, dbForY (event.position.y)));

    if (added >= 0)
        select (added);
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    auto band = bandAt (event.position);

    if (band < 0)
        band = selectedBand;

    if (band < 0 || ! bands[(size_t) band].used)
        return;

    auto& q = bandParameter (band, "q");
    const auto step = wheel.deltaY * (event.mods.isShiftDown() ? 0.1f / fineDivisor : 0.1f);

    q.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q.getValue() + step));
}

void EqGraph::showBandMenu (int band)
{
    constexpr int typeBase = 100, slopeBase = 200, powerItem = 1, deleteItem = 2;

    auto& type = bandParameter (band, "type");
    auto& slope = bandParameter (band, "slope");

    const auto types = type.getAllValueStrings();
    const auto slopes = slope.getAllValueStrings();

    juce::PopupMenu menu;

    for (int i = 0; i < types.size(); ++i)
        menu.addItem (typeBase + i, types[i], true, types[i] == type.getCurrentValueAsText());

    menu.addSeparator();

    juce::PopupMenu slopeMenu;

    for (int i = 0; i < slopes.size(); ++i)
        slopeMenu.addItem (slopeBase + i, slopes[i], true, slopes[i] == slope.getCurrentValueAsText());

    menu.addSubMenu ("Slope", slopeMenu, eq::hasSlope (bands[(size_t) band].spec.type));
    menu.addItem (powerItem, "On", true, bands[(size_t) band].spec.on);
    menu.addItem (deleteItem, "Delete band");

    menu.showMenuAsync (juce::PopupMenu::Options {}.withTargetComponent (this).withMousePosition(),
                        [safeThis = juce::Component::SafePointer<EqGraph> (this), band] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            if (result == powerItem)
                                safeThis->setValue (band, "on", safeThis->bands[(size_t) band].spec.on ? 0.0f : 1.0f);
                            else if (result == deleteItem)
                                safeThis->processor.removeBand (band);
                            else if (result >= slopeBase)
                                safeThis->setValue (band, "slope", (float) (result - slopeBase));
                            else
                                safeThis->setValue (band, "type", (float) (result - typeBase));
                        });
}
