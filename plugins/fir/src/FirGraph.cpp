#include "FirGraph.h"

#include <fancy/plugin/ParamFormat.h>
#include <fancy/ui/Popups.h>
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
    const juce::Colour colours[] { colours::accentPurple, colours::accentYellow, colours::uiTeal, colours::accentRed, colours::uiBlue, colours::accentGreen };
    return colours[juce::jlimit (0, fir::numModes - 1, mode)];
}

namespace
{
// Phase 4 Wave 2: the ratio curve is drawn in green, the threshold in the mode's teal.
juce::Colour ratioColour() { return colours::accentGreen; }
constexpr double ratioAxisLow = 0.5, ratioAxisHigh = 20.0;
} // namespace

// TwoWaySwitch ------------------------------------------------------------------------------------

TwoWaySwitch::TwoWaySwitch (const juce::String& first, const juce::String& second)
    : names { first, second }, fills { colours::uiTeal, colours::uiTeal }, background (colours::bgWell)
{
}

void TwoWaySwitch::setColours (juce::Colour first, juce::Colour second)
{
    fills[0] = first;
    fills[1] = second;
    repaint();
}

void TwoWaySwitch::setSelected (int index)
{
    if (index != selected)
    {
        selected = index;
        repaint();
    }
}

juce::Rectangle<float> TwoWaySwitch::segmentBounds (int index) const
{
    const auto area = getLocalBounds().toFloat().reduced (2.0f);
    const auto width = area.getWidth() / 2.0f;
    return area.withX (area.getX() + (float) index * width).withWidth (width);
}

void TwoWaySwitch::paint (juce::Graphics& g)
{
    g.setColour (background);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    g.setFont (font (Weight::regular, textHeight));

    for (int i = 0; i < 2; ++i)
    {
        const auto area = segmentBounds (i);

        if (i == selected)
        {
            g.setColour (fills[i]);
            g.fillRoundedRectangle (area, 2.0f);
        }

        g.setColour (i == selected ? colours::textPrimary : colours::textSecondary);
        g.drawText (names[i], area, juce::Justification::centred, false);
    }
}

void TwoWaySwitch::mouseDown (const juce::MouseEvent& event)
{
    for (int i = 0; i < 2; ++i)
    {
        if (segmentBounds (i).contains (event.position) && i != selected)
        {
            setSelected (i);

            if (onChange != nullptr)
                onChange (i);
        }
    }
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

float FirGraph::yForRatio (double ratio, float height) noexcept
{
    const auto bottom = height - plotBottom;
    const auto t = std::log (std::max (ratio, 1.0e-3) / ratioAxisLow) / std::log (ratioAxisHigh / ratioAxisLow);
    return bottom - (float) t * (bottom - plotTop);
}

double FirGraph::ratioForY (float y, float height) noexcept
{
    const auto bottom = height - plotBottom;
    const auto t = (double) ((bottom - y) / (bottom - plotTop));
    return ratioAxisLow * std::pow (ratioAxisHigh / ratioAxisLow, t);
}

FirGraph::FirGraph (FirProcessor& processorToUse)
    : processor (processorToUse), analyzerThread (*this)
{
    // Phase 4 Wave 2 (mockup 59): Threshold | Ratio at the graph's top left, in Compressor mode.
    curveSwitch.setColours (modeColour (compressor), ratioColour());
    curveSwitch.setTextHeight (10.5f);
    curveSwitch.setBackground (colours::bgHeader);
    curveSwitch.setBounds ((int) plotLeft + 7, 6, 128, 18);
    curveSwitch.setSelected (processor.editRatio.load() ? 1 : 0);
    curveSwitch.onChange = [this] (int index) { setEditsRatio (index == 1); };
    addChildComponent (curveSwitch);

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

    // Phase 4 Wave 1: Learn reads the same input.
    if (learnState.load() == learnRequested)
    {
        learner.start (learnCurve.load(), rate);
        learnProgress.store (0.0f);
        learnState.store (learnRunning);
    }

    if (const auto count = processor.inputFifo.pop (scratch.data(), (int) scratch.size()); count > 0)
    {
        before.addSamples (scratch.data(), count);

        if (learnState.load() == learnRunning)
        {
            learner.addSamples (scratch.data(), count);
            learnProgress.store (learner.progress());

            if (learner.isComplete())
            {
                learnedPoints = learner.result();
                learnState.store (learnDone);
            }
        }
    }

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
    resLowHz = value ("resLow");
    resHighHz = value ("resHigh");
    resDepthDb = value ("resDepth");
    resTilt = value ("resTilt");

    // Phase 4 Wave 2.
    knobRatio = processor.getKnobRatio();
    sideShown = processor.showsSide();
    editsRatio = processor.editRatio.load();
    curveSwitch.setVisible (mode == compressor);
    curveSwitch.setSelected (editsRatio ? 1 : 0);

    // Phase 4 Wave 1: a finished Learn sets the curve, as points or, in Freehand, as the table.
    if (learnState.load() == learnDone)
    {
        const auto curve = learnCurve.load();

        if (! learnedPoints.empty())
        {
            if (editMode == fir::freehand)
            {
                std::array<float, fir::curvePoints> table {};

                for (int i = 0; i < fir::curvePoints; ++i)
                    table[(size_t) i] = (float) fir::evaluateCurve (learnedPoints, true, fir::curveHz (i), fir::defaultCurveDb (curve));

                processor.setFreehand (curve, table);
            }
            else
            {
                processor.setPoints (curve, learnedPoints);
            }
        }

        learnState.store (learnIdle);
    }

    repaint();
}

void FirGraph::setEditsRatio (bool ratio)
{
    processor.editRatio.store (ratio);
    editsRatio = ratio;
    selectedPoint = -1;
    dragPoint = -1;
    curveSwitch.setSelected (ratio ? 1 : 0);
    repaint();
}

void FirGraph::startLearn (int curve)
{
    learnCurve.store (curve);
    learnProgress.store (0.0f);
    learnState.store (learnRequested);
}

void FirGraph::cancelLearn()
{
    learnState.store (learnIdle);
}

void FirGraph::showPreviewSpectra (bool resonances)
{
    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    frozenSpectra = true;
    previewReduction.clear();

    const auto width = juce::jmax (1, getWidth() - (int) (plotLeft + plotRight));

    inputSpectrum.assign ((size_t) width, -200.0f);
    outputSpectrum.assign ((size_t) width, -200.0f);

    if (resonances)
    {
        // Mockup 28: a falling programme with resonances at 700 Hz and 3 kHz, cut as Resonance
        // cuts them: down by up to Depth, never below 4 dB over the spectrum around them.
        const auto depth = processor.getApvts().getRawParameterValue ("resDepth")->load();
        const auto low = processor.getApvts().getRawParameterValue ("resLow")->load();
        const auto high = processor.getApvts().getRawParameterValue ("resHigh")->load();

        const auto peaks = [] (double hz)
        {
            const auto peak = [hz] (double centre, double gain, double q)
            {
                const auto x = std::log2 (hz / centre) * q * 1.6;
                return gain / (1.0 + x * x);
            };

            return peak (3000.0, 12.0, 8.0) + peak (700.0, 9.0, 9.0);
        };

        const auto cut = [&] (double hz)
        {
            const auto outside = std::max ({ std::log2 (low / hz), std::log2 (hz / high), 0.0 });
            return std::min ((double) depth, std::max (0.0, peaks (hz) - 4.0)) * std::max (0.0, 1.0 - 2.0 * outside);
        };

        for (int x = 0; x < width; ++x)
        {
            const auto hz = hzForX (plotLeft + (float) x, (float) getWidth());
            const auto ripple = 1.6 * std::sin (hz * 0.0123 + 1.0) + 1.1 * std::sin (hz * 0.0371 + 2.0) + 0.8 * std::sin (hz * 0.0917 + 3.0);
            const auto bed = -36.0 - 4.2 * std::log2 (std::max (hz, 30.0) / 500.0) - (hz < 80.0 ? 6.0 * std::log2 (80.0 / hz) : 0.0)
                           - (hz > 12000.0 ? 10.0 * std::log2 (hz / 12000.0) : 0.0);
            const auto in = bed + peaks (hz) + ripple;

            inputSpectrum[(size_t) x] = (float) in;
            outputSpectrum[(size_t) x] = (float) (in - cut (hz));
        }

        previewReduction.assign (displayColumns, 0.0f);

        for (int column = 0; column < displayColumns; ++column)
            for (int step = 0; step < 8; ++step)
                previewReduction[(size_t) column] = std::max (previewReduction[(size_t) column],
                                                              (float) cut (displayHz (column) * std::pow (1000.0, (double) step / (8.0 * displayColumns))));

        return;
    }

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

void FirGraph::showHarshPreview()
{
    // Mockup 59: a mix whose 2-5 kHz is loud, and what the threshold and ratio curves take from
    // it: the reduction is the level over the threshold times (1 - 1 / ratio).
    const juce::SpinLock::ScopedLockType lock (spectrumLock);

    frozenSpectra = true;

    const auto width = juce::jmax (1, getWidth() - (int) (plotLeft + plotRight));
    const auto thresholdIndex = thresholdCurve(), ratioIndex = processor.shownCurve (fir::ratioCurve);

    inputSpectrum.assign ((size_t) width, -200.0f);
    outputSpectrum.assign ((size_t) width, -200.0f);
    previewReduction.assign (displayColumns, 0.0f);

    const auto levelAt = [] (double hz, int x)
    {
        const auto ripple = 1.4 * std::sin ((double) x * 0.71 + 1.0) + 1.0 * std::sin ((double) x * 1.93 + 2.0);
        const auto base = -40.0 - 8.0 * std::pow (std::log10 (hz / 400.0), 2.0);
        const auto bump = 12.0 * std::exp (-std::pow (std::log2 (hz / 3000.0) / 0.5, 2.0));
        return base + bump + ripple;
    };

    const auto reductionAt = [&] (double hz, double level)
    {
        const auto threshold = processor.curveDbAt (thresholdIndex, hz);
        const auto ratio = fir::ratioFromCurve (knobRatio, processor.curveDbAt (ratioIndex, hz));
        return std::max (0.0, (level - threshold) * (1.0 - 1.0 / ratio));
    };

    for (int x = 0; x < width; ++x)
    {
        const auto hz = hzForX (plotLeft + (float) x, (float) getWidth());
        const auto level = levelAt (hz, x);

        inputSpectrum[(size_t) x] = (float) level;
        outputSpectrum[(size_t) x] = (float) (level - reductionAt (hz, level));
    }

    for (int column = 0; column < displayColumns; ++column)
    {
        const auto hz = displayHz (column) * std::pow (1000.0, 0.5 / displayColumns);
        previewReduction[(size_t) column] = (float) reductionAt (hz, levelAt (hz, 0) + 1.0);
    }
}

bool FirGraph::editsCurve() const noexcept
{
    return mode == eq || mode == gate || mode == compressor || mode == resonance;
}

// Phase 4 Wave 2: Resonance edits the depth curve, in percent of Depth, drawn where its ceiling
// lands (Depth x the curve x Tilt, hanging from 0 dB). Compressor edits the threshold or, with
// Ratio chosen, the ratio curve, on its own scale. The other curves are in dB as drawn. In
// Mid/Side with Side shown, the side's.
int FirGraph::curve() const noexcept
{
    const auto base = mode == resonance ? fir::depthCurve : (mode == compressor && editsRatio ? fir::ratioCurve : mode);
    return sideShown ? fir::sideCurve (base) : base;
}

int FirGraph::thresholdCurve() const noexcept
{
    const auto base = juce::jlimit (0, (int) compressor, mode);
    return sideShown ? fir::sideCurve (base) : base;
}

double FirGraph::ceilingScale (double hz) const noexcept
{
    return std::max (0.05, (double) resDepthDb * fir::SpectralEngine::tiltFactor (hz, (double) resTilt * 0.01)) * 0.01;
}

float FirGraph::yForCurve (int curveIndex, double value, double hz) const noexcept
{
    switch (fir::baseCurve (curveIndex))
    {
        case fir::depthCurve: return yFor ((float) (-value * ceilingScale (hz)));
        case fir::ratioCurve: return yForRatio (fir::ratioFromCurve (knobRatio, value), (float) getHeight());
        default:              return yFor ((float) value);
    }
}

double FirGraph::valueForCurve (int curveIndex, float y, double hz) const noexcept
{
    switch (fir::baseCurve (curveIndex))
    {
        case fir::depthCurve: return fir::clampCurveValue (curveIndex, -(double) dbAt (y) / ceilingScale (hz));
        case fir::ratioCurve: return fir::curveFromRatio (knobRatio, ratioForY (y, (float) getHeight()));
        default:              return (double) dbAt (y);
    }
}

juce::String FirGraph::formatValue (int curveIndex, double value) const
{
    switch (fir::baseCurve (curveIndex))
    {
        case fir::depthCurve: return juce::String (juce::roundToInt (value)) + " %";
        case fir::ratioCurve: return fancy::formatFixed ((float) fir::ratioFromCurve (knobRatio, value), 1) + ":1";
        default:              return fancy::formatFixed ((float) value, 1) + " dB";
    }
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

    // Phase 4 Wave 2: the side's profile while the side is shown.
    if (! (sideShown ? shared.hasSideProfile : shared.hasProfile).load())
        return -200.0;

    const auto& profile = sideShown ? shared.sideProfile : shared.profile;
    const auto position = juce::jlimit (0.0, (double) (profilePoints - 1), hz / (profileHighHz / (double) (profilePoints - 1)));
    const auto index = juce::jmin ((int) position, profilePoints - 2);
    const auto fraction = position - (double) index;

    const auto power = (double) profile[(size_t) index].load() + fraction * (double) (profile[(size_t) index + 1].load() - profile[(size_t) index].load());
    const auto offset = (double) processor.getApvts().getRawParameterValue ("offset")->load();

    return 10.0 * std::log10 (std::max (power * profileToLevelScale (processor.getFftSize()), 1.0e-20)) + offset;
}

int FirGraph::pointAt (juce::Point<float> position) const
{
    if (! editsCurve() || editMode == fir::freehand)
        return -1;

    const auto& points = processor.getPoints (curve());

    for (int i = (int) points.size() - 1; i >= 0; --i)
    {
        const juce::Point<float> handle (xForHz (points[(size_t) i].hz, (float) getWidth()), yForValue (points[(size_t) i].db, points[(size_t) i].hz));

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

    if (mode == resonance)
    {
        // Phase 3 Q2.7: outside Range dimmed, and the reduction hanging from 0 dB.
        const auto top = plotTop, height = bottom - plotTop;
        const auto left = xForHz (resLowHz, width), right = xForHz (resHighHz, width);

        g.setColour (colours::bgWell.withAlpha (0.55f));

        if (left > plotLeft)
            g.fillRect (plotLeft, top, left - plotLeft, height);

        if (right < width - plotRight)
            g.fillRect (right, top, width - plotRight - right, height);

        auto& shared = processor.getShared();
        const auto zero = yFor (0.0f);

        g.setColour (colour.withAlpha (0.6f));

        for (int column = 0; column < displayColumns; ++column)
        {
            const auto reduction = previewReduction.empty() ? shared.reduction[(size_t) column].load (std::memory_order_relaxed)
                                                            : previewReduction[(size_t) column];

            if (reduction <= 0.05f)
                continue;

            const auto x0 = xForHz (displayHz (column), width), x1 = xForHz (displayHz (column + 1), width);
            g.fillRect (x0, zero, x1 - x0, yFor (-reduction) - zero);
        }

        // Phase 4 Wave 2 (reference/phase4/42__fir_w2_mockup.png): the depth ceiling, dashed,
        // between Low and High, with the nodes on it.
        juce::Path ceiling;
        const auto from = std::max (plotLeft, left), to = std::min (width - plotRight, right);

        for (auto x = from; x <= to; x += 1.0f)
        {
            const auto hz = hzForX (x, width);
            const auto y = juce::jlimit (plotTop, bottom, yForValue (processor.curveDbAt (curve(), hz), hz));

            if (x == from)
                ceiling.startNewSubPath (x, y);
            else
                ceiling.lineTo (x, y);
        }

        if (! ceiling.isEmpty())
        {
            auto fill = ceiling;
            fill.lineTo (to, zero);
            fill.lineTo (from, zero);
            fill.closeSubPath();

            g.setColour (colour.withAlpha (0.10f));
            g.fillPath (fill);

            juce::Path dashed;
            const float dashes[] { 5.0f, 3.0f };
            juce::PathStrokeType (1.6f).createDashedStroke (dashed, ceiling, dashes, 2);
            g.setColour (colour);
            g.fillPath (dashed);
        }

        if (editMode != fir::freehand)
        {
            for (const auto& point : processor.getPoints (curve()))
            {
                const juce::Point<float> centre (xForHz (point.hz, width), yForValue (point.db, point.hz));
                const auto circle = juce::Rectangle<float> (pointRadius * 2.0f, pointRadius * 2.0f).withCentre (centre);

                g.setColour (colours::textPrimary);
                g.fillEllipse (circle);
                g.setColour (colour);
                g.drawEllipse (circle, 2.0f);
            }
        }

        return;
    }

    if (mode == compressor)
    {
        // Gain reduction hanging from the 0 dB line.
        auto& shared = processor.getShared();
        const auto zero = yFor (0.0f);

        g.setColour (colours::accentYellow.withAlpha (0.6f));

        for (int column = 0; column < displayColumns; ++column)
        {
            const auto reduction = previewReduction.empty() ? shared.reduction[(size_t) column].load (std::memory_order_relaxed)
                                                            : previewReduction[(size_t) column];

            if (reduction <= 0.05f)
                continue;

            const auto left = xForHz (displayHz (column), width), right = xForHz (displayHz (column + 1), width);
            g.fillRect (left, zero, right - left, yFor (-reduction) - zero);
        }

        // Phase 4 Wave 2 (mockup 59): both curves, the one being edited bright and with its
        // points, the other dimmed and dashed; the ratio's scale while it's edited.
        const auto ratioIndex = processor.shownCurve (fir::ratioCurve);

        paintCurveLine (g, thresholdCurve(), colour, ! editsRatio, ! editsRatio);
        paintCurveLine (g, ratioIndex, ratioColour(), editsRatio, false);

        if (editMode != fir::freehand)
            paintPoints (g, curve(), editsRatio ? ratioColour() : colour);

        if (editsRatio)
            paintRatioScale (g);

        return;
    }

    juce::Path line;
    auto started = false;

    for (auto x = plotLeft; x <= width - plotRight; x += 1.0f)
    {
        const auto hz = hzForX (x, width);
        const auto db = mode == subtract ? (float) profileDbAt (hz) : (float) processor.curveDbAt (thresholdCurve(), hz);

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

    paintPoints (g, curve(), colour);
}

void FirGraph::paintCurveLine (juce::Graphics& g, int curveIndex, juce::Colour colour, bool edited, bool filled)
{
    const auto width = (float) getWidth();
    const auto bottom = (float) getHeight() - plotBottom;

    juce::Path line;

    for (auto x = plotLeft; x <= width - plotRight; x += 1.0f)
    {
        const auto hz = hzForX (x, width);
        const auto y = juce::jlimit (plotTop, bottom, yForCurve (curveIndex, processor.curveDbAt (curveIndex, hz), hz));

        if (x == plotLeft)
            line.startNewSubPath (x, y);
        else
            line.lineTo (x, y);
    }

    if (filled)
    {
        auto fill = line;
        fill.lineTo (width - plotRight, bottom);
        fill.lineTo (plotLeft, bottom);
        fill.closeSubPath();

        g.setColour (colour.withAlpha (0.14f));
        g.fillPath (fill);
    }

    if (edited)
    {
        g.setColour (colour);
        g.strokePath (line, juce::PathStrokeType (2.0f));
        return;
    }

    juce::Path dashed;
    const float dashes[] { 4.0f, 3.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, line, dashes, 2);
    g.setColour (colour.withAlpha (0.45f));
    g.fillPath (dashed);
}

void FirGraph::paintPoints (juce::Graphics& g, int curveIndex, juce::Colour colour)
{
    const auto width = (float) getWidth();
    const auto& points = processor.getPoints (curveIndex);

    for (const auto& point : points)
    {
        const juce::Point<float> centre (xForHz (point.hz, width), yForCurve (curveIndex, point.db, point.hz));
        const auto circle = juce::Rectangle<float> (pointRadius * 2.0f, pointRadius * 2.0f).withCentre (centre);

        g.setColour (colours::textPrimary);
        g.fillEllipse (circle);
        g.setColour (colour);
        g.drawEllipse (circle, 2.0f);
    }

    // Phase 4 Wave 2 (mockup 59): the selected ratio point shows its frequency and ratio.
    if (fir::baseCurve (curveIndex) != fir::ratioCurve || selectedPoint < 0 || selectedPoint >= (int) points.size())
        return;

    const auto& point = points[(size_t) selectedPoint];
    const auto text = fancy::formatHz ((float) point.hz) + juce::String::fromUTF8 (" \xc2\xb7 ") + formatValue (curveIndex, point.db);
    const auto anchor = juce::Point<float> (xForHz (point.hz, width), yForCurve (curveIndex, point.db, point.hz));

    g.setFont (font (Weight::regular, 11.0f));

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (g.getCurrentFont(), text, 0.0f, 0.0f);

    const auto boxWidth = std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 14.0f;
    auto box = juce::Rectangle<float> (anchor.x + 8.0f, anchor.y - 30.0f, boxWidth, 18.0f);

    if (box.getRight() > width - plotRight)
        box.setX (anchor.x - 8.0f - boxWidth);

    box.setY (juce::jlimit (plotTop, (float) getHeight() - plotBottom - box.getHeight(), box.getY()));

    g.setColour (colours::bgSection);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (colours::grey5);
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    g.setColour (colours::textPrimary);
    g.drawText (text, box.withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
}

void FirGraph::paintRatioScale (juce::Graphics& g)
{
    const auto right = (float) getWidth() - plotRight - 4.0f;
    const auto height = (float) getHeight();

    g.setFont (font (Weight::regular, 9.5f));
    g.setColour (ratioColour());

    for (const auto ratio : { 1.0, 2.0, 4.0, 20.0 })
    {
        const auto y = juce::jlimit (plotTop + 18.0f, height - plotBottom - 6.0f, yForRatio (ratio, height));
        g.drawText (juce::String ((int) ratio) + ":1", juce::Rectangle<float> (right - 40.0f, y - 12.0f, 40.0f, 11.0f),
                    juce::Justification::centredRight, false);
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

    if (mode == compressor)
        items.push_back ({ "ratio", ratioColour() });
    else if (mode == subtract)
    {
        if (processor.getShared().hasProfile.load() || ! showsAdaptive())
            items.push_back ({ "noise profile", modeColour (mode) });

        if (showsAdaptive())
            items.push_back ({ "tracked profile (dashed)", modeColour (mode) });
    }

    if (mode == compressor)
        items.push_back ({ "reduction", colours::accentYellow });
    else if (mode == resonance)
        items.push_back ({ "reduction", modeColour (mode) });

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
    paintAdaptive (g);
    paintLegend (g);
}

bool FirGraph::showsAdaptive() const
{
    return mode == subtract && processor.getApvts().getRawParameterValue ("source")->load() >= 0.5f
        && (sideShown ? processor.getShared().hasSideAdaptive : processor.getShared().hasAdaptive).load();
}

// Phase 2.1 A: the adaptive estimate, dashed in the mode colour, over the learned profile.
void FirGraph::paintAdaptive (juce::Graphics& g)
{
    if (! showsAdaptive())
        return;

    auto& shared = processor.getShared();
    const auto& adaptive = sideShown ? shared.sideAdaptive : shared.adaptive;
    const auto width = (float) getWidth();
    const auto bottom = (float) getHeight() - plotBottom;
    const auto offset = (double) processor.getApvts().getRawParameterValue ("offset")->load();
    const auto scale = profileToLevelScale (processor.getFftSize());
    const auto step = profileHighHz / (double) (profilePoints - 1);

    juce::Path line;

    for (auto x = plotLeft; x <= width - plotRight; x += 1.0f)
    {
        const auto position = juce::jlimit (0.0, (double) (profilePoints - 1), hzForX (x, width) / step);
        const auto index = juce::jmin ((int) position, profilePoints - 2);
        const auto fraction = position - (double) index;
        const auto power = (double) adaptive[(size_t) index].load() + fraction * (double) (adaptive[(size_t) index + 1].load() - adaptive[(size_t) index].load());
        const auto db = (float) (10.0 * std::log10 (std::max (power * scale, 1.0e-20)) + offset);
        const auto y = juce::jlimit (plotTop, bottom, yFor (db));

        if (x == plotLeft)
            line.startNewSubPath (x, y);
        else
            line.lineTo (x, y);
    }

    juce::Path dashed;
    const float dashes[] { 4.0f, 3.0f };
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, line, dashes, 2);

    g.setColour (modeColour (subtract));
    g.fillPath (dashed);
}

// Editing ---------------------------------------------------------------------------------------

void FirGraph::drawFreehand (juce::Point<float> from, juce::Point<float> to)
{
    auto table = processor.getFreehand (curve());

    const auto width = (float) getWidth();
    const auto first = curvePosition (hzForX (juce::jmin (from.x, to.x), width));
    const auto last = curvePosition (hzForX (juce::jmax (from.x, to.x), width));

    for (auto i = (int) std::ceil (first); i <= (int) std::floor (last) && i < curvePoints; ++i)
    {
        const auto x = xForHz (curveHz (i), width);
        const auto t = to.x == from.x ? 1.0f : (x - from.x) / (to.x - from.x);

        table[(size_t) i] = (float) fir::clampCurveValue (curve(), valueAt (from.y + (to.y - from.y) * t, curveHz (i)));
    }

    // A single click still sets the nearest value.
    const auto nearest = juce::jlimit (0, curvePoints - 1, juce::roundToInt (curvePosition (hzForX (to.x, width))));
    table[(size_t) nearest] = (float) fir::clampCurveValue (curve(), valueAt (to.y, curveHz (nearest)));

    processor.setFreehand (curve(), table);
}

void FirGraph::mouseDown (const juce::MouseEvent& event)
{
    dragPoint = -1;
    draggingCurve = false;
    drawing = false;
    lastMouse = event.position;
    selectedPoint = -1;

    if (! editsCurve() || event.position.x < plotLeft || event.position.y > (float) getHeight() - plotBottom)
        return;

    if (editMode == fir::freehand)
    {
        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            draggingCurve = true;
            dragStartTable = processor.getFreehand (curve());
            return;
        }

        drawing = true;
        drawFreehand (event.position, event.position);
        return;
    }

    auto points = processor.getPoints (curve());
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
            processor.setPoints (curve(), points);
            return;
        }

        dragPoint = index;
        selectedPoint = index;
        dragStartHz = points[(size_t) index].hz;
        return;
    }

    // A click on the curve adds a point there.
    const auto hz = hzForX (event.position.x, (float) getWidth());
    const auto curveY = yForValue (processor.curveDbAt (curve(), hz), hz);

    if (std::abs (curveY - event.position.y) > curveGrab || (int) points.size() >= maxCurvePoints)
        return;

    points.push_back ({ hz, valueAt (event.position.y, hz) });
    sortPoints (points);
    processor.setPoints (curve(), points);

    dragPoint = pointAt (event.position);
    selectedPoint = dragPoint;
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
        const auto shift = (float) (valueAt ((float) event.getMouseDownY() + (float) event.getDistanceFromDragStartY(), 1000.0)
                                    - valueAt ((float) event.getMouseDownY(), 1000.0));

        if (editMode == fir::freehand)
        {
            auto table = dragStartTable;

            for (auto& value : table)
                value = (float) fir::clampCurveValue (curve(), value + shift);

            processor.setFreehand (curve(), table);
        }
        else
        {
            auto points = dragStartPoints;

            for (auto& point : points)
                point.db += (double) shift;

            processor.setPoints (curve(), points);
        }

        return;
    }

    if (dragPoint < 0)
        return;

    auto points = processor.getPoints (curve());

    if (dragPoint >= (int) points.size())
        return;

    auto& point = points[(size_t) dragPoint];

    point.hz = event.mods.isShiftDown() ? dragStartHz : juce::jlimit (curveLowHz, 20000.0, hzForX (event.position.x, width));
    point.db = valueAt (juce::jlimit (plotTop, (float) getHeight() - plotBottom, event.position.y), point.hz);

    const auto moved = point;

    processor.setPoints (curve(), points);

    // Sorting may have moved it.
    const auto& sorted = processor.getPoints (curve());

    for (int i = 0; i < (int) sorted.size(); ++i)
        if (sorted[(size_t) i] == moved)
            dragPoint = selectedPoint = i;
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

    auto points = processor.getPoints (curve());
    points[(size_t) index].db = fir::defaultCurveDb (curve());
    processor.setPoints (curve(), points);
}

void FirGraph::typePoint (int index)
{
    const auto point = processor.getPoints (curve())[(size_t) index];

    fancy::ui::DialogSpec spec;
    spec.title = "Point " + juce::String (index + 1);
    spec.action = "Set";
    spec.fields.push_back ({ "Frequency", fancy::formatHz ((float) point.hz) });
    // Phase 4 Wave 2: the depth curve's points are a percentage of Depth, and the ratio curve's
    // a ratio, which is stored relative to the Ratio knob.
    const auto curve = this->curve();
    const auto base = fir::baseCurve (curve);

    spec.fields.push_back ({ base == fir::depthCurve ? "Depth" : base == fir::ratioCurve ? "Ratio" : "Gain", formatValue (curve, point.db) });

    fancy::ui::showDialog (*this, std::move (spec),
                           [safeThis = juce::Component::SafePointer<FirGraph> (this), index, curve] (bool accepted, const juce::StringArray& values)
    {
        if (safeThis == nullptr || ! accepted)
            return;

        auto points = safeThis->processor.getPoints (curve);

        if (index >= (int) points.size())
            return;

        if (fancy::isValueText (values[0]))
            points[(size_t) index].hz = (double) fancy::parseHz (values[0]);

        if (fancy::isValueText (values[1]))
        {
            if (fir::baseCurve (curve) == fir::ratioCurve)
                points[(size_t) index].db = fir::curveFromRatio (safeThis->knobRatio, (double) fancy::parseExpansionRatio (values[1], (float) fir::maxRatio));
            else
                points[(size_t) index].db = fir::clampCurveValue (curve, (double) fancy::parseNumber (values[1]));
        }

        safeThis->processor.setPoints (curve, points);
    });
}

void FirGraph::typeAxis (bool top)
{
    auto& parameter = *processor.getApvts().getParameter (top ? "graphTop" : "graphFloor");

    fancy::ui::DialogSpec spec;
    spec.title = top ? "Graph top" : "Analysis floor";
    spec.action = "Set";
    spec.fields.push_back ({ "Level", parameter.getCurrentValueAsText() });

    fancy::ui::showDialog (*this, std::move (spec), [&parameter] (bool accepted, const juce::StringArray& values)
    {
        if (! accepted || ! fancy::isValueText (values[0]))
            return;

        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (fancy::parseNumber (values[0])));
        parameter.endChangeGesture();
    });
}
