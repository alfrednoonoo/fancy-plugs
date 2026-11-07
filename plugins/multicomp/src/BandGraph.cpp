#include "BandGraph.h"

#include <fancy/ui/Style.h>

#include <cmath>
#include <iterator>

using namespace fancy::ui;
using multicomp::maxBands;

namespace
{
// design/phase2-mockup-src/xcomp.html: lx and ly.
constexpr float plotLeft = 34.0f, plotRight = 10.0f, plotTop = 12.0f, plotBottom = 22.0f;
constexpr float floorDb = -60.0f;
constexpr float reductionPixelsPerDb = 3.2f;

constexpr float handleRadius = 8.0f, grabRadius = 12.0f, crossoverGrab = 5.0f;
constexpr float ratioDragPixels = 200.0f, ratioWheelStep = 0.08f;

constexpr int analyzerIntervalMs = 33;
} // namespace

juce::Colour multiBandColour (int band)
{
    const juce::Colour palette[] { colours::uiEqRed, colours::accentPurple, colours::uiEqBlue, colours::uiEqGreen,
                                   colours::accentOrange, colours::accentYellow, colours::accentTeal, colours::accentBlue };

    return palette[band % (int) std::size (palette)];
}

float BandGraph::xForHz (double hz, float width) noexcept
{
    return plotLeft + (width - plotLeft - plotRight) * (float) std::log10 (hz / multicomp::lowestHz) / 3.0f;
}

double BandGraph::hzForX (float x, float width) noexcept
{
    return multicomp::lowestHz * std::pow (10.0, 3.0 * (double) ((x - plotLeft) / (width - plotLeft - plotRight)));
}

float BandGraph::yForDb (float db, float height) noexcept
{
    return plotTop + (height - plotTop - plotBottom) * (db / floorDb);
}

float BandGraph::dbForY (float y, float height) noexcept
{
    return floorDb * (y - plotTop) / (height - plotTop - plotBottom);
}

BandGraph::BandGraph (MultiCompProcessor& processorToUse)
    : processor (processorToUse), analyzerThread (*this)
{
    processor.analyzerActive.store (true);
    analyzerThread.startThread (juce::Thread::Priority::low);
}

BandGraph::~BandGraph()
{
    analyzerThread.stopThread (2000);
    processor.analyzerActive.store (false);
}

void BandGraph::AnalyzerThread::run()
{
    fancy::dsp::SpectrumAnalyzer analyzer;
    std::vector<float> scratch ((size_t) fancy::dsp::SampleFifo::capacity);

    while (! threadShouldExit())
    {
        graph.analyse (analyzer, scratch);
        wait (analyzerIntervalMs);
    }
}

void BandGraph::analyse (fancy::dsp::SpectrumAnalyzer& analyzer, std::vector<float>& scratch)
{
    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    analyzer.configure (rate, {});

    const auto count = processor.analyzerFifo.pop (scratch.data(), (int) scratch.size());

    if (count > 0)
        analyzer.addSamples (scratch.data(), count);

    const auto width = columns.load (std::memory_order_relaxed);

    if (width <= 0)
        return;

    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    if ((int) spectrum.size() != width || frozenSpectrum)
        return;

    for (int x = 0; x < width; ++x)
    {
        const auto left = plotLeft + (float) x, fullWidth = plotLeft + (float) width + plotRight;

        spectrum[(size_t) x] = analyzer.levelBetween (hzForX (left, fullWidth), hzForX (left + 1.0f, fullWidth));
    }
}

void BandGraph::resized()
{
    const auto width = juce::jmax (1, getWidth() - (int) (plotLeft + plotRight));

    if (! frozenSpectrum)
    {
        const juce::SpinLock::ScopedLockType lock (spectrumLock);
        spectrum.assign ((size_t) width, -200.0f);
    }

    columns.store (width);
}

void BandGraph::setSelectedBand (int band)
{
    selectedBand = band;
    repaint();
}

void BandGraph::update (const std::array<float, maxBands>& gainReduction)
{
    const auto read = [this] (const char* id) { return processor.getApvts().getRawParameterValue (id)->load (std::memory_order_relaxed) >= 0.5f; };

    reduction = gainReduction;
    showGrid = read ("showGrid");
    showSpectrum = read ("showSpectrum");

    repaint();
}

void BandGraph::showPreviewSpectrum()
{
    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    frozenSpectrum = true;

    const auto width = juce::jmax (1, getWidth() - (int) (plotLeft + plotRight));

    spectrum.assign ((size_t) width, -200.0f);

    // The mockup's made-up programme material, with the same random generator.
    auto seed = (juce::int64) 7;

    const auto random = [&seed]
    {
        seed = (seed * 16807) % 2147483647;
        return (float) seed / 2147483647.0f;
    };

    for (int x = 0; x < width; ++x)
    {
        const auto hz = (float) hzForX (plotLeft + (float) x, (float) getWidth());
        const auto octaves = std::log10 (hz / 60.0f);
        const auto sign = octaves < 0.0f ? -1.0f : 1.0f;

        spectrum[(size_t) x] = -14.0f - 9.0f * sign * std::pow (std::abs (octaves), 1.25f) - 6.0f * random() - (hz < 60.0f ? 8.0f : 0.0f);
    }
}

juce::Point<float> BandGraph::handleOf (int band) const
{
    const auto width = (float) getWidth();
    const auto left = xForHz (processor.bandBottomHz (band), width);
    const auto right = xForHz (processor.bandTopHz (band), width);

    return { 0.5f * (left + right), yForDb ((float) processor.readBand (band).thresholdDb, (float) getHeight()) };
}

int BandGraph::bandAt (juce::Point<float> position) const
{
    for (int band = 0; band < processor.getNumBands(); ++band)
        if (handleOf (band).getDistanceFrom (position) <= grabRadius)
            return band;

    return -1;
}

int BandGraph::crossoverAt (juce::Point<float> position) const
{
    const auto bottom = (float) getHeight() - plotBottom;

    if (position.y < plotTop - 2.0f || position.y > bottom + 2.0f)
        return -1;

    for (int crossover = 0; crossover < processor.getNumBands() - 1; ++crossover)
        if (std::abs (xForHz (processor.bandTopHz (crossover), (float) getWidth()) - position.x) <= crossoverGrab)
            return crossover;

    return -1;
}

int BandGraph::bandUnder (float x) const
{
    const auto numBands = processor.getNumBands();
    const auto hz = hzForX (x, (float) getWidth());

    for (int band = 0; band < numBands - 1; ++band)
        if (hz < processor.bandTopHz (band))
            return band;

    return numBands - 1;
}

void BandGraph::select (int band)
{
    if (onBandSelected != nullptr)
        onBandSelected (band);
}

void BandGraph::changeRatio (int band, float distance)
{
    auto& ratio = *processor.getApvts().getParameter (MultiCompProcessor::id (band, "ratio"));

    ratio.beginChangeGesture();
    ratio.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, ratio.getValue() + distance));
    ratio.endChangeGesture();
}

void BandGraph::paintGrid (juce::Graphics& g)
{
    const auto width = (float) getWidth();
    const auto height = (float) getHeight();
    const auto bottom = height - plotBottom;

    g.setColour (colours::bgBody);

    for (const auto hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
        g.fillRect (xForHz (hz, width) - 0.5f, 10.0f, 1.0f, bottom - 10.0f);

    for (int db = 0; db >= -60; db -= 12)
        g.fillRect (plotLeft, yForDb ((float) db, height) - 0.5f, width - plotLeft - plotRight, 1.0f);
}

void BandGraph::paintSpectrum (juce::Graphics& g)
{
    const juce::SpinLock::ScopedTryLockType lock (spectrumLock);

    if (! lock.isLocked() || spectrum.empty())
        return;

    const auto height = (float) getHeight();

    juce::Path path;

    for (int x = 0; x < (int) spectrum.size(); ++x)
    {
        const auto y = yForDb (juce::jlimit (floorDb, 0.0f, spectrum[(size_t) x]), height);

        if (x == 0)
            path.startNewSubPath (plotLeft, y);
        else
            path.lineTo (plotLeft + (float) x, y);
    }

    g.setColour (colours::accentOrange.withAlpha (0.8f));
    g.strokePath (path, juce::PathStrokeType (1.3f));
}

void BandGraph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto width = bounds.getWidth(), height = bounds.getHeight();
    const auto bottom = height - plotBottom;

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (bounds, 6.0f);

    if (showGrid)
        paintGrid (g);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 11.0f));

    for (int db = 0; db >= -60; db -= 12)
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, yForDb ((float) db, height) - 7.0f, plotLeft - 6.0f, 14.0f),
                    juce::Justification::centredRight, false);

    const std::pair<double, const char*> labels[] { { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" }, { 500.0, "500" }, { 1000.0, "1k" },
                                                    { 2000.0, "2k" }, { 5000.0, "5k" }, { 10000.0, "10k" }, { 20000.0, "20k" } };

    for (const auto& [hz, text] : labels)
        g.drawText (text, juce::Rectangle<float> (xForHz (hz, width) - 18.0f, height - 16.0f, 36.0f, 14.0f), juce::Justification::centred, false);

    const auto numBands = processor.getNumBands();

    for (int band = 0; band < numBands; ++band)
    {
        const auto left = xForHz (processor.bandBottomHz (band), width);
        const auto right = xForHz (processor.bandTopHz (band), width);
        const auto colour = multiBandColour (band);
        const auto y = handleOf (band).y;

        g.setColour (colour.withAlpha (band == selectedBand ? 0.32f : 0.16f));
        g.fillRect (left, y, right - left, bottom - y);

        const auto depth = reduction[(size_t) band] * reductionPixelsPerDb;

        if (depth > 0.0f)
        {
            g.setColour (colours::accentYellow.withAlpha (0.85f));
            g.fillRoundedRectangle (left + 6.0f, plotTop, juce::jmax (0.0f, right - left - 12.0f), depth, 1.0f);
        }
    }

    if (showSpectrum)
        paintSpectrum (g);

    for (int band = 0; band < numBands; ++band)
    {
        const auto left = xForHz (processor.bandBottomHz (band), width);
        const auto right = xForHz (processor.bandTopHz (band), width);
        const auto handle = handleOf (band);
        const auto colour = multiBandColour (band);

        g.setColour (colour);
        g.fillRect (left, handle.y - 1.0f, right - left, 2.0f);

        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (handle);

        if (band == selectedBand)
        {
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (colours::textPrimary);
            g.fillEllipse (circle);
            g.setColour (colour);
            g.drawEllipse (circle.reduced (1.0f), 2.0f);
        }

        g.setColour (band == selectedBand ? colours::textPrimary : colours::bgSection);
        g.setFont (font (Weight::semiBold, 11.0f));
        g.drawText (juce::String (band + 1), circle, juce::Justification::centred, false);
    }

    for (int crossover = 0; crossover < numBands - 1; ++crossover)
    {
        const auto x = xForHz (processor.bandTopHz (crossover), width);

        g.setColour (colours::uiBlue);
        g.fillRect (x - 0.75f, 10.0f, 1.5f, bottom - 10.0f);
        g.fillRoundedRectangle (x - 3.0f, bottom - 8.0f, 6.0f, 10.0f, 2.0f);
    }
}

void BandGraph::mouseDown (const juce::MouseEvent& event)
{
    drag = Drag::none;

    const auto band = bandAt (event.position);

    if (band >= 0)
    {
        select (band);

        drag = event.mods.isAltDown() ? Drag::ratio : Drag::threshold;
        dragTarget = band;
        lastDragY = event.position.y;
        return;
    }

    const auto crossover = crossoverAt (event.position);

    if (crossover >= 0)
    {
        drag = Drag::crossover;
        dragTarget = crossover;
    }
}

void BandGraph::mouseDrag (const juce::MouseEvent& event)
{
    if (drag == Drag::threshold)
    {
        const auto db = juce::jlimit (floorDb, 0.0f, dbForY (event.position.y, (float) getHeight()));
        processor.setParameter (MultiCompProcessor::id (dragTarget, "thr"), db);
    }
    else if (drag == Drag::ratio)
    {
        changeRatio (dragTarget, (lastDragY - event.position.y) / ratioDragPixels);
        lastDragY = event.position.y;
    }
    else if (drag == Drag::crossover)
    {
        processor.setCrossover (dragTarget, hzForX (event.position.x, (float) getWidth()));
    }

    repaint();
}

void BandGraph::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::none;
}

void BandGraph::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto band = bandAt (event.position);

    if (band >= 0)
    {
        processor.resetBand (band);
        return;
    }

    if (crossoverAt (event.position) >= 0)
        return;

    const auto added = processor.splitBand (bandUnder (event.position.x), hzForX (event.position.x, (float) getWidth()));

    if (added >= 0)
        select (added);
}

void BandGraph::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto band = bandAt (event.position);

    if (band >= 0)
        changeRatio (band, wheel.deltaY * ratioWheelStep);
}
