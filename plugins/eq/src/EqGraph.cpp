#include "EqGraph.h"

#include <fancy/Decibels.h>
#include <fancy/plugin/ParamFormat.h>
#include <fancy/ui/Popups.h>
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
    fancy::dsp::SpectrumAnalyzer analyzer;
    std::vector<float> scratch ((size_t) fancy::dsp::SampleFifo::capacity);

    while (! threadShouldExit())
    {
        graph.analyse (analyzer, scratch);
        wait (analyzerIntervalMs);
    }
}

void EqGraph::analyse (fancy::dsp::SpectrumAnalyzer& analyzer, std::vector<float>& scratch)
{
    auto& apvts = processor.getApvts();

    const auto read = [&] (const char* id) { return apvts.getRawParameterValue (id)->load (std::memory_order_relaxed); };

    constexpr int sizes[] { 4096, 8192, 16384 };

    const auto rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    fancy::dsp::SpectrumAnalyzer::Settings settings;
    settings.size = sizes[juce::jlimit (0, 2, juce::roundToInt (read ("anaFft")))];
    settings.window = read ("anaWindow") >= 0.5f ? fancy::dsp::SpectrumAnalyzer::Window::blackmanHarris : fancy::dsp::SpectrumAnalyzer::Window::hann;
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

        // The total follows the selected band's channel.
        needsRecalculation = needsRecalculation || mixedChannels;
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

    // On a mono bus every band acts on the one channel.
    const auto stereoBus = processor.getMainBusNumInputChannels() >= 2;

    if (stereoBus != lastStereoBus)
    {
        lastStereoBus = stereoBus;
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

        // Dynamic bands redraw as their change moves, in 0.05 dB steps (Phase 2.1 D).
        DynamicState dynamics;
        dynamics.dynamic = used && processor.isDynamic (b);

        if (dynamics.dynamic)
        {
            dynamics.rangeDb = (float) processor.readDynamics (b).rangeDb;
            dynamics.changeDb = std::round ((b == previewBand ? previewChangeDb : processor.getDynamicChange (b)) * 20.0f) / 20.0f;
        }

        if (! (dynamics == lastDynamics[(size_t) b]))
        {
            lastDynamics[(size_t) b] = dynamics;
            needsRecalculation = true;
        }
    }

    if (needsRecalculation)
        recalculate();

    if (analyzerMode != 0)
        repaint();
}

// "-6 dB", "+2.5 dB", "-3.6 dB": the graph's dynamic read-out.
juce::String EqGraph::formatDbText (float db)
{
    const auto rounded = std::round (db * 10.0f) / 10.0f;
    const auto whole = std::abs (rounded - std::round (rounded)) < 0.05f;
    const auto magnitude = juce::String (std::abs (rounded), whole ? 0 : 1);
    return (rounded > 0.0f ? "+" : rounded < 0.0f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String()) + magnitude + " dB";
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

    // Phase 3 Q2.4: with every band Stereo the total is the sum of the bands, as it always was.
    // Otherwise the EQ is a 2 x 2 matrix from (L, R) to (L, R) at each frequency, the bands
    // multiplying in order, and the total is one path through it.
    mixedChannels = false;

    for (int b = 0; b < eq::maxBands; ++b)
        mixedChannels = mixedChannels || (lastStereoBus && lastUsed[(size_t) b] && lastSpecs[(size_t) b].on
                                          && lastSpecs[(size_t) b].channel != eq::Channel::stereo);

    totalChannel = eq::Channel::mid;

    if (juce::isPositiveAndBelow (selectedBand, eq::maxBands) && lastUsed[(size_t) selectedBand]
        && lastSpecs[(size_t) selectedBand].channel != eq::Channel::stereo)
        totalChannel = lastSpecs[(size_t) selectedBand].channel;

    using Complex = std::complex<double>;

    struct Matrix
    {
        Complex t00 { 1.0 }, t01 { 0.0 }, t10 { 0.0 }, t11 { 1.0 };
    };

    std::vector<Matrix> matrices (mixedChannels ? (size_t) width : (size_t) 0);

    const auto applyBand = [] (Matrix& t, Complex h, eq::Channel channel)
    {
        switch (channel)
        {
            case eq::Channel::left:  t.t00 *= h; t.t01 *= h; break;
            case eq::Channel::right: t.t10 *= h; t.t11 *= h; break;

            case eq::Channel::mid:
            case eq::Channel::side:
            {
                const auto same = 0.5 * (h + 1.0);
                const auto cross = channel == eq::Channel::mid ? 0.5 * (h - 1.0) : 0.5 * (1.0 - h);
                const Matrix n { same * t.t00 + cross * t.t10, same * t.t01 + cross * t.t11,
                                 cross * t.t00 + same * t.t10, cross * t.t01 + same * t.t11 };
                t = n;
                break;
            }

            case eq::Channel::stereo:
            default: t.t00 *= h; t.t01 *= h; t.t10 *= h; t.t11 *= h; break;
        }
    };

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

        // A dynamic band draws its static curve, its reach at Gain + Range, and adds its
        // current change to the total. A dynamic notch is a bell that only cuts.
        const auto& dynamics = lastDynamics[(size_t) b];
        band.dynamic = dynamics.dynamic && band.spec.on;

        auto reachShape = shape, liveShape = shape;

        if (band.dynamic)
        {
            const auto notch = shape.type == eq::BandType::notch;

            if (notch)
            {
                shape.type = eq::BandType::band;
                shape.gainDb = 0.0;
                reachShape = liveShape = shape;
            }

            band.reachDb = notch ? juce::jmin (0.0f, dynamics.rangeDb) : dynamics.rangeDb;
            band.changeDb = dynamics.changeDb;
            reachShape.gainDb = shape.gainDb + band.reachDb;
            liveShape.gainDb = shape.gainDb + band.changeDb;
        }

        const auto filter = eq::design (shape, lastRate);
        const auto reachFilter = eq::design (reachShape, lastRate);
        const auto liveFilter = eq::design (liveShape, lastRate);

        band.reach.resize ((size_t) width);

        for (int x = 0; x < width; ++x)
        {
            const auto hz = juce::jmin (hzForX ((double) x, (double) width), 0.499 * lastRate);
            const auto w = 2.0 * std::numbers::pi * hz / lastRate;
            const auto response = filter.responseAt (w);
            const auto db = (float) fancy::gainToDb (std::abs (response), -200.0);

            band.db[(size_t) x] = band.spec.on ? db : 0.0f;
            band.reach[(size_t) x] = band.dynamic ? (float) fancy::gainToDb (std::abs (reachFilter.responseAt (w)), -200.0) : db;

            if (band.spec.on)
            {
                const auto live = band.dynamic ? liveFilter.responseAt (w) : response;
                composite[(size_t) x] += band.dynamic ? (float) fancy::gainToDb (std::abs (live), -200.0) : db;
                phase[(size_t) x] += linear ? 0.0f : (float) std::arg (live);

                // Linear phase works on each band's magnitude.
                if (mixedChannels)
                    applyBand (matrices[(size_t) x], linear ? Complex (std::abs (live)) : live, band.spec.channel);
            }
        }

        const auto w = 2.0 * std::numbers::pi * juce::jmin (band.spec.frequencyHz, 0.499 * lastRate) / lastRate;
        const auto handleDb = (float) fancy::gainToDb (std::abs (filter.responseAt (w)), -200.0);

        band.handle = { (float) xForHz (band.spec.frequencyHz, (double) width),
                        juce::jlimit (selectedRadius, (float) getHeight() - selectedRadius, yForDb (handleDb)) };
    }

    if (mixedChannels)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto& t = matrices[(size_t) x];
            Complex path;

            // Mid is what a signal with L = R keeps of itself, Side the same for L = -R, and Left
            // and Right what a signal in that channel alone keeps there.
            switch (totalChannel)
            {
                case eq::Channel::left:  path = t.t00; break;
                case eq::Channel::right: path = t.t11; break;
                case eq::Channel::side:  path = 0.5 * (t.t00 - t.t01 - t.t10 + t.t11); break;
                case eq::Channel::mid:
                case eq::Channel::stereo:
                default:                 path = 0.5 * (t.t00 + t.t01 + t.t10 + t.t11); break;
            }

            composite[(size_t) x] = (float) fancy::gainToDb (std::abs (path), -200.0);
            phase[(size_t) x] = linear ? 0.0f : (float) std::arg (path);
        }
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
    paintMatch (g);
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

    // Which channel the white total is for, while any band isn't Stereo (Phase 3 Q2.4). It sits
    // left of the right axis's top label.
    if (mixedChannels)
    {
        static const char* const names[] { "Mid", "Left", "Right", "Mid", "Side" };

        g.drawText (juce::String ("total: ") + names[juce::jlimit (0, 4, (int) totalChannel)],
                    width - 170, 4, 120, 14, juce::Justification::centredRight, false);
    }
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

        // Phase 2.1 D: the dynamic reach, a lighter fill between the static curve and the curve
        // at Gain + Range, with a dashed edge.
        if (band.dynamic && band.reach.size() == (size_t) width)
        {
            juce::Path reach;
            reach.startNewSubPath (0.0f, yForDb (clampDb (band.reach[0])));

            for (int x = 1; x < width; ++x)
                reach.lineTo ((float) x, yForDb (clampDb (band.reach[(size_t) x])));

            auto between = reach;

            for (int x = width - 1; x >= 0; --x)
                between.lineTo ((float) x, yForDb (clampDb (band.db[(size_t) x])));

            between.closeSubPath();

            g.setColour (colour.withAlpha (0.18f));
            g.fillPath (between);

            juce::Path dashed;
            const float dashes[] { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, reach, dashes, 2);

            g.setColour (colour);
            g.fillPath (dashed);
        }
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

    // The selected dynamic band's reach and where it is now, as panel 3 of the mockup shows.
    if (juce::isPositiveAndBelow (selectedBand, eq::maxBands) && bands[(size_t) selectedBand].dynamic)
    {
        const auto& band = bands[(size_t) selectedBand];
        const auto text = "reach " + formatDbText (band.reachDb) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 now ")) + formatDbText (band.changeDb);

        g.setFont (font (Weight::regular, 12.0f));
        g.setColour (colours::menuHint);
        g.drawText (text, 76, 4, 260, 14, juce::Justification::centredLeft, false);
    }
}

void EqGraph::setMatchOverlay (const MatchOverlay& overlay)
{
    matchOverlay = overlay;
    repaint();
}

// The Match card's curves, as panel 3 of the Q2.4 mockup: the reference in purple, the current
// in grey and the target as a dashed white line.
void EqGraph::paintMatch (juce::Graphics& g)
{
    if (! matchOverlay.visible)
        return;

    const auto width = (double) getWidth();
    const auto bottom = (float) getHeight() + 2.0f;
    const auto floorDb = dbForY ((float) getHeight() + 4.0f);
    const auto topDb = dbForY (-4.0f);

    const auto pathOf = [&] (const eq::match::Curve& curve, bool spectrum)
    {
        juce::Path path;

        for (int i = 0; i < eq::match::numPoints; ++i)
        {
            const auto x = (float) xForHz (eq::match::hzAt (i), width);
            const auto value = (float) curve[(size_t) i];
            const auto y = spectrum ? juce::jlimit (-2.0f, bottom, yForSpectrum (juce::jmax (-200.0f, value)))
                                    : yForDb (juce::jlimit (floorDb, topDb, value));

            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        return path;
    };

    if (matchOverlay.hasCurrent)
    {
        g.setColour (colours::textSecondary.withAlpha (0.8f));
        g.strokePath (pathOf (matchOverlay.current, true), juce::PathStrokeType (1.2f));
    }

    if (matchOverlay.hasReference)
    {
        g.setColour (colours::accentPurple);
        g.strokePath (pathOf (matchOverlay.reference, true), juce::PathStrokeType (1.6f));
    }

    const float dashes[] { 5.0f, 4.0f };

    if (matchOverlay.hasTarget)
    {
        juce::Path dashed;
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, pathOf (matchOverlay.target, false), dashes, 2);

        g.setColour (juce::Colours::white);
        g.fillPath (dashed);
    }

    // The key, under the corner's "post".
    g.setFont (font (Weight::regular, 12.0f));

    g.setColour (colours::accentPurple);
    g.drawText ("reference", 40, 20, 62, 14, juce::Justification::centredLeft, false);

    g.setColour (colours::textSecondary);
    g.drawText ("current", 106, 20, 50, 14, juce::Justification::centredLeft, false);

    juce::Path key, keyDashed;
    key.startNewSubPath (160.0f, 27.5f);
    key.lineTo (178.0f, 27.5f);
    juce::PathStrokeType (1.6f).createDashedStroke (keyDashed, key, dashes, 2);

    g.setColour (juce::Colours::white);
    g.fillPath (keyDashed);
    g.drawText ("target", 183, 20, 50, 14, juce::Justification::centredLeft, false);
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
            // A band that isn't Stereo shows its channel's letter after its number, in 9 px.
            static const char* const letters[] { "", "L", "R", "M", "S" };
            const auto letter = letters[juce::jlimit (0, 4, (int) band.spec.channel)];

            if (*letter != 0)
            {
                g.setFont (font (Weight::semiBold, 9.0f));
                g.drawText (juce::String (b + 1) + letter, circle.expanded (3.0f, 0.0f), juce::Justification::centred, false);
                g.setFont (font (Weight::semiBold, 11.0f));
            }
            else
            {
                g.drawText (juce::String (b + 1), circle, juce::Justification::centred, false);
            }
        }
    }
}

juce::RangedAudioParameter& EqGraph::bandParameter (int band, const char* suffix)
{
    return *processor.getApvts().getParameter (EqProcessor::id (band, suffix));
}

void EqGraph::setValue (int band, const char* suffix, float value)
{
    const fancy::FancyProcessor::UserEdit edit (processor);
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
            showBandMenu (band, event.position);
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

    const fancy::FancyProcessor::UserEdit edit (processor);
    q.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q.getValue() + step));
}

void EqGraph::showBandMenu (int band, juce::Point<float> position)
{
    constexpr int typeBase = 100, slopeBase = 200, channelBase = 300, freqItem = 1, gainItem = 2, powerItem = 3, resetItem = 4, deleteItem = 5;

    auto& type = bandParameter (band, "type");
    auto& slope = bandParameter (band, "slope");
    const auto& spec = bands[(size_t) band].spec;

    const auto types = type.getAllValueStrings();
    const auto slopes = slope.getAllValueStrings();
    const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    const auto ellipsis = juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));

    fancy::ui::Menu typeMenu;

    for (int i = 0; i < types.size(); ++i)
        typeMenu.addItem (typeBase + i, types[i], true, types[i] == type.getCurrentValueAsText());

    fancy::ui::Menu slopeMenu;

    for (int i = 0; i < slopes.size(); ++i)
        slopeMenu.addItem (slopeBase + i, slopes[i], true, slopes[i] == slope.getCurrentValueAsText());

    fancy::ui::Menu menu;
    menu.addTitle ("Band " + juce::String (band + 1) + dot + type.getCurrentValueAsText() + dot + fancy::formatHz ((float) spec.frequencyHz));
    menu.addItem (freqItem, "Set frequency" + ellipsis);
    menu.addItem (gainItem, "Set gain" + ellipsis, eq::hasGain (spec.type));
    menu.addSubMenu ("Type", std::move (typeMenu));

    if (eq::hasSlope (spec.type))
        menu.addSubMenu ("Slope", std::move (slopeMenu));

    // Phase 3 Q2.4: the band's Channel, as the chip has it. A mono bus has only the one.
    {
        auto& channel = bandParameter (band, "chan");
        const auto channels = channel.getAllValueStrings();

        fancy::ui::Menu channelMenu;

        for (int i = 0; i < channels.size(); ++i)
            channelMenu.addItem (channelBase + i, channels[i], true, channels[i] == channel.getCurrentValueAsText());

        menu.addSubMenu ("Channel", std::move (channelMenu), processor.getMainBusNumInputChannels() >= 2);
    }

    menu.addItem (powerItem, "Enabled", true, spec.on);
    menu.addSeparator();
    menu.addItem (resetItem, "Reset band", true, false, "Double-click");
    menu.addItem (deleteItem, "Delete band");

    menu.showAt (*this, position, [safeThis = juce::Component::SafePointer<EqGraph> (this), band] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& self = *safeThis;

        if (result == freqItem || result == gainItem)
        {
            if (self.onTypeValue != nullptr)
                self.onTypeValue (band, result == freqItem ? "freq" : "gain");
        }
        else if (result == powerItem)
        {
            self.setValue (band, "on", self.bands[(size_t) band].spec.on ? 0.0f : 1.0f);
        }
        else if (result == resetItem)
        {
            self.processor.resetBand (band);
        }
        else if (result == deleteItem)
        {
            if (self.onDeleteBand != nullptr)
                self.onDeleteBand (band);
        }
        else if (result >= channelBase)
        {
            self.setValue (band, "chan", (float) (result - channelBase));
        }
        else if (result >= slopeBase)
        {
            self.setValue (band, "slope", (float) (result - slopeBase));
        }
        else
        {
            self.setValue (band, "type", (float) (result - typeBase));
        }
    });
}
