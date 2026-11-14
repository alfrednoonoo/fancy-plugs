#include "EqMatch.h"

#include <algorithm>
#include <numbers>

namespace eq::match
{

namespace
{
constexpr int baseFftSize = 8192;
constexpr int hopDivisor = 4;
constexpr double silenceRms = 1.0e-3;       // -60 dBFS
constexpr double floorPower = 1.0e-30;

constexpr double meanLowHz = 200.0, meanHighHz = 5000.0;
constexpr double fadeOctaves = 1.0 / 3.0;

// The fit.
constexpr double shelfThresholdDb = 1.0;    // an end this far from 0 gets a shelf
constexpr double shelfEdgeShare = 0.7;      // if the range's very edge holds this much of it
constexpr double shelfLimitHz = 1000.0;     // a low shelf stays below it, a high shelf above
constexpr double ridge = 0.03;              // 1 dB of band gain costs as much as this RMS error
constexpr double smallestErrorDb = 0.25;    // nothing is added for less
constexpr double smallestGainDb = 0.1;      // a fitted band under this is dropped
constexpr int refineIterations = 12;

double powerToDb (double power) noexcept
{
    return 10.0 * std::log10 (std::max (power, floorPower));
}

int rateScale (double sampleRate) noexcept
{
    auto scale = 1;

    while (scale < 8 && sampleRate > 50000.0 * scale)
        scale *= 2;

    return scale;
}

// What the fit measures against: the frequencies, and how much each counts.
struct Grid
{
    Grid (double sampleRate, const Settings& settings)
        : rate (sampleRate), lowHz (settings.lowHz), highHz (settings.highHz)
    {
        for (int i = 0; i < numPoints; ++i)
        {
            const auto hz = hzAt (i);
            const auto s = std::sin (std::numbers::pi * std::min (hz, 0.499 * rate) / rate);

            sinSquared[(size_t) i] = s * s;
            inside[(size_t) i] = hz >= settings.lowHz && hz <= settings.highHz;

            // 100 Hz to 10 kHz counts double; outside the range hardly at all.
            weight[(size_t) i] = ! inside[(size_t) i] ? 0.25 : (hz >= 100.0 && hz <= 10000.0 ? 2.0 : 1.0);
            rootWeight[(size_t) i] = std::sqrt (weight[(size_t) i]);
            weightSum += weight[(size_t) i];
        }
    }

    void curveOf (const BandSpec& band, Curve& out) const
    {
        const auto filter = eq::design (band, rate);

        for (int i = 0; i < numPoints; ++i)
            out[(size_t) i] = powerToDb (filter.powerAt (sinSquared[(size_t) i]));
    }

    // Weighted RMS error, in dB.
    double error (const Curve& model, const Curve& target) const
    {
        auto sum = 0.0;

        for (int i = 0; i < numPoints; ++i)
        {
            const auto difference = model[(size_t) i] - target[(size_t) i];
            sum += weight[(size_t) i] * difference * difference;
        }

        return std::sqrt (sum / weightSum);
    }

    // What the refinement minimises: the squared error, and a little for every dB of gain, so
    // two bands never grow against each other.
    double cost (const Curve& model, const Curve& target, const std::vector<BandSpec>& bands) const
    {
        const auto rms = error (model, target);
        auto gains = 0.0;

        for (const auto& band : bands)
            gains += band.gainDb * band.gainDb;

        return rms * rms + ridge * ridge * gains;
    }

    // A bell stays inside the range; a low shelf in its lower part and a high shelf in its upper.
    double clampFrequency (BandType type, double hz) const
    {
        if (type == BandType::lowShelf)
            return std::clamp (hz, lowHz, std::max (lowHz, shelfLimitHz));

        if (type == BandType::highShelf)
            return std::clamp (hz, std::min (highHz, shelfLimitHz), highHz);

        return std::clamp (hz, lowHz, highHz);
    }

    double rate, lowHz, highHz;
    std::array<double, numPoints> sinSquared {}, weight {}, rootWeight {};
    std::array<bool, numPoints> inside {};
    double weightSum = 0.0;
};

bool isShelf (const BandSpec& band) noexcept
{
    return band.type != BandType::band;
}

int numParameters (const BandSpec& band) noexcept
{
    return isShelf (band) ? 2 : 3;
}

// A band's parameters as the fit moves them: log2 Freq, Gain, log2 Q.
double getParameter (const BandSpec& band, int index) noexcept
{
    return index == 0 ? std::log2 (band.frequencyHz) : (index == 1 ? band.gainDb : std::log2 (band.q));
}

void setParameter (BandSpec& band, int index, double value, const Grid& grid) noexcept
{
    if (index == 0)
        band.frequencyHz = grid.clampFrequency (band.type, std::pow (2.0, value));
    else if (index == 1)
        band.gainDb = std::clamp (value, -maxGainDb, maxGainDb);
    else
        band.q = std::clamp (std::pow (2.0, value), minQ, maxQ);
}

constexpr double parameterSteps[] { 0.02, 0.05, 0.03 };

BandSpec makeBand (BandType type, double hz, double gainDb, double q)
{
    BandSpec band;
    band.type = type;
    band.frequencyHz = std::clamp (hz, gridLowHz, 20000.0);
    band.gainDb = std::clamp (gainDb, -maxGainDb, maxGainDb);
    band.q = std::clamp (q, minQ, maxQ);
    band.order = 2;
    band.on = true;
    return band;
}

// Solves a x = b in place, by elimination with partial pivoting. False if a is singular.
bool solve (std::vector<double>& a, std::vector<double>& b, int n)
{
    for (int column = 0; column < n; ++column)
    {
        auto pivot = column;

        for (int row = column + 1; row < n; ++row)
            if (std::abs (a[(size_t) (row * n + column)]) > std::abs (a[(size_t) (pivot * n + column)]))
                pivot = row;

        if (std::abs (a[(size_t) (pivot * n + column)]) < 1.0e-14)
            return false;

        if (pivot != column)
        {
            for (int k = 0; k < n; ++k)
                std::swap (a[(size_t) (column * n + k)], a[(size_t) (pivot * n + k)]);

            std::swap (b[(size_t) column], b[(size_t) pivot]);
        }

        for (int row = column + 1; row < n; ++row)
        {
            const auto factor = a[(size_t) (row * n + column)] / a[(size_t) (column * n + column)];

            if (factor == 0.0)
                continue;

            for (int k = column; k < n; ++k)
                a[(size_t) (row * n + k)] -= factor * a[(size_t) (column * n + k)];

            b[(size_t) row] -= factor * b[(size_t) column];
        }
    }

    for (int row = n - 1; row >= 0; --row)
    {
        auto sum = b[(size_t) row];

        for (int k = row + 1; k < n; ++k)
            sum -= a[(size_t) (row * n + k)] * b[(size_t) k];

        b[(size_t) row] = sum / a[(size_t) (row * n + row)];
    }

    return true;
}

// The bands' curves and their sum.
struct Model
{
    void build (const std::vector<BandSpec>& bands, const Grid& grid)
    {
        curves.resize (bands.size());
        total.fill (0.0);

        for (size_t b = 0; b < bands.size(); ++b)
        {
            grid.curveOf (bands[b], curves[b]);

            for (int i = 0; i < numPoints; ++i)
                total[(size_t) i] += curves[b][(size_t) i];
        }
    }

    std::vector<Curve> curves;
    Curve total {};
};

// Levenberg-Marquardt on every band's parameters together. Returns the weighted RMS error.
// docs/research/eq-match-model.py is the same fit in Python, which is where it was tried first.
double refine (std::vector<BandSpec>& bands, const Curve& target, const Grid& grid)
{
    Model model;
    model.build (bands, grid);

    auto error = grid.error (model.total, target);
    auto cost = grid.cost (model.total, target, bands);

    auto n = 0;

    for (const auto& band : bands)
        n += numParameters (band);

    if (n == 0)
        return error;

    std::vector<double> jacobian ((size_t) (numPoints * n)), normal ((size_t) (n * n)), gradient ((size_t) n);
    std::vector<double> matrix, step;
    Curve moved {};
    auto lambda = 1.0e-2;

    for (int iteration = 0; iteration < refineIterations; ++iteration)
    {
        auto column = 0;

        for (size_t b = 0; b < bands.size(); ++b)
        {
            for (int p = 0; p < numParameters (bands[b]); ++p, ++column)
            {
                // A forward difference, or a backward one at the top of the parameter's range.
                auto trial = bands[b];
                const auto value = getParameter (trial, p);

                setParameter (trial, p, value + parameterSteps[p], grid);
                auto change = getParameter (trial, p) - value;

                if (std::abs (change) < 0.5 * parameterSteps[p])
                {
                    setParameter (trial, p, value - parameterSteps[p], grid);
                    change = getParameter (trial, p) - value;
                }

                grid.curveOf (trial, moved);

                for (int i = 0; i < numPoints; ++i)
                    jacobian[(size_t) (i * n + column)] = std::abs (change) > 0.0
                                                              ? grid.rootWeight[(size_t) i] * (moved[(size_t) i] - model.curves[b][(size_t) i]) / change
                                                              : 0.0;
            }
        }

        std::fill (normal.begin(), normal.end(), 0.0);
        std::fill (gradient.begin(), gradient.end(), 0.0);

        for (int i = 0; i < numPoints; ++i)
        {
            const auto residual = grid.rootWeight[(size_t) i] * (model.total[(size_t) i] - target[(size_t) i]);
            const auto* row = jacobian.data() + (size_t) (i * n);

            for (int j = 0; j < n; ++j)
            {
                gradient[(size_t) j] += row[j] * residual;

                for (int k = j; k < n; ++k)
                    normal[(size_t) (j * n + k)] += row[j] * row[k];
            }
        }

        // As the cost has it: the mean over the weights, plus the gains' share.
        for (int j = 0; j < n; ++j)
        {
            gradient[(size_t) j] /= grid.weightSum;

            for (int k = j; k < n; ++k)
                normal[(size_t) (j * n + k)] /= grid.weightSum;
        }

        {
            auto index = 0;

            for (const auto& band : bands)
            {
                normal[(size_t) ((index + 1) * n + index + 1)] += ridge * ridge;
                gradient[(size_t) (index + 1)] += ridge * ridge * band.gainDb;
                index += numParameters (band);
            }
        }

        for (int j = 0; j < n; ++j)
            for (int k = 0; k < j; ++k)
                normal[(size_t) (j * n + k)] = normal[(size_t) (k * n + j)];

        auto improved = false, settled = false;

        for (int attempt = 0; attempt < 6 && ! improved; ++attempt)
        {
            matrix = normal;
            step.assign ((size_t) n, 0.0);

            for (int j = 0; j < n; ++j)
            {
                matrix[(size_t) (j * n + j)] += lambda * normal[(size_t) (j * n + j)] + 1.0e-9;
                step[(size_t) j] = -gradient[(size_t) j];
            }

            if (! solve (matrix, step, n))
            {
                lambda *= 6.0;
                continue;
            }

            auto trialBands = bands;
            auto index = 0;

            for (auto& band : trialBands)
                for (int p = 0; p < numParameters (band); ++p, ++index)
                    setParameter (band, p, getParameter (band, p) + step[(size_t) index], grid);

            Model trialModel;
            trialModel.build (trialBands, grid);

            const auto trialCost = grid.cost (trialModel.total, target, trialBands);

            if (trialCost < cost)
            {
                settled = cost - trialCost < 1.0e-6;
                bands = std::move (trialBands);
                model = std::move (trialModel);
                cost = trialCost;
                error = grid.error (model.total, target);
                lambda = std::max (lambda * 0.3, 1.0e-6);
                improved = true;
            }
            else
            {
                lambda *= 6.0;
            }
        }

        if (! improved || settled)
            break;
    }

    return error;
}

double meanBetween (const Curve& curve, double lowHz, double highHz)
{
    auto sum = 0.0;
    auto count = 0;

    for (int i = 0; i < numPoints; ++i)
    {
        const auto hz = hzAt (i);

        if (hz >= lowHz && hz <= highHz)
        {
            sum += curve[(size_t) i];
            ++count;
        }
    }

    return count > 0 ? sum / count : 0.0;
}
} // namespace

// Learner ---------------------------------------------------------------------------------------

void Learner::prepare (double sampleRate)
{
    if (sampleRate == rate && fft != nullptr)
        return;

    rate = sampleRate;
    size = baseFftSize * rateScale (sampleRate);
    hop = size / hopDivisor;
    fill = 0;

    fft = std::make_unique<juce::dsp::FFT> (juce::roundToInt (std::log2 ((double) size)));

    window.resize ((size_t) size);
    input.assign ((size_t) size, 0.0f);
    frame.assign ((size_t) size * 2, 0.0f);

    auto sum = 0.0;

    for (int n = 0; n < size; ++n)
    {
        // Blackman-Harris.
        const auto phase = 2.0 * std::numbers::pi * (double) n / (double) size;
        const auto value = 0.35875 - 0.48829 * std::cos (phase) + 0.14128 * std::cos (2.0 * phase) - 0.01168 * std::cos (3.0 * phase);

        window[(size_t) n] = (float) value;
        sum += value;
    }

    // A sine of amplitude A reads A, as the analyzer has it.
    scale = 2.0 / sum;

    const auto maxBin = size / 2 - 1;
    const auto halfPoint = std::pow (2.0, 0.5 / (double) pointsPerOctave);

    for (int i = 0; i < numPoints; ++i)
    {
        const auto position = hzAt (i) * (double) size / rate;
        const auto low = position / halfPoint, high = position * halfPoint;
        auto& span = spans[(size_t) i];

        span.average = high - low >= 1.0;

        if (span.average)
        {
            span.first = std::clamp ((int) std::ceil (low), 1, maxBin);
            span.last = std::clamp ((int) std::floor (high), span.first, maxBin);
        }
        else
        {
            span.first = std::clamp ((int) std::floor (position), 0, maxBin - 1);
            span.last = span.first + 1;
            span.fraction = std::clamp (position - (double) span.first, 0.0, 1.0);
        }
    }
}

void Learner::clear() noexcept
{
    meanPower.fill (0.0);
    seconds = 0.0;
    fill = 0;
}

void Learner::addSamples (const float* samples, int numSamples)
{
    if (fft == nullptr)
        return;

    while (numSamples > 0)
    {
        const auto count = std::min (numSamples, size - fill);

        std::copy_n (samples, count, input.begin() + fill);

        fill += count;
        samples += count;
        numSamples -= count;

        if (fill == size)
        {
            analyse();

            std::copy (input.begin() + hop, input.end(), input.begin());
            fill = size - hop;
        }
    }
}

void Learner::analyse()
{
    // Each quarter of the frame has to be above -60 dBFS, so a frame that reaches into a gap
    // doesn't count either: the average is of sounding material only.
    for (int quarter = 0; quarter < hopDivisor; ++quarter)
    {
        auto energy = 0.0;

        for (int n = quarter * hop; n < (quarter + 1) * hop; ++n)
            energy += (double) input[(size_t) n] * (double) input[(size_t) n];

        if (! (std::sqrt (energy / (double) hop) >= silenceRms))
            return;
    }

    for (int n = 0; n < size; ++n)
        frame[(size_t) n] = input[(size_t) n] * window[(size_t) n];

    std::fill (frame.begin() + size, frame.end(), 0.0f);

    fft->performFrequencyOnlyForwardTransform (frame.data());

    const auto hopSeconds = (double) hop / rate;
    const auto frames = seconds / hopSeconds + 1.0;

    const auto powerOf = [&] (int bin)
    {
        const auto magnitude = (double) frame[(size_t) bin] * scale;
        return magnitude * magnitude;
    };

    for (int i = 0; i < numPoints; ++i)
    {
        const auto& span = spans[(size_t) i];
        auto power = 0.0;

        if (span.average)
        {
            for (int bin = span.first; bin <= span.last; ++bin)
                power += powerOf (bin);

            power /= (double) (span.last - span.first + 1);
        }
        else
        {
            power = powerOf (span.first) + span.fraction * (powerOf (span.last) - powerOf (span.first));
        }

        meanPower[(size_t) i] += (power - meanPower[(size_t) i]) / frames;
    }

    seconds += hopSeconds;
}

Curve Learner::getSpectrumDb() const
{
    Curve result {};

    for (int i = 0; i < numPoints; ++i)
        result[(size_t) i] = powerToDb (meanPower[(size_t) i]);

    return result;
}

void Learner::restore (const Curve& spectrumDb, double learnedSeconds)
{
    for (int i = 0; i < numPoints; ++i)
        meanPower[(size_t) i] = std::pow (10.0, spectrumDb[(size_t) i] / 10.0);

    seconds = std::max (0.0, learnedSeconds);
    fill = 0;
}

// Target ----------------------------------------------------------------------------------------

Curve smooth (const Curve& curve, double octaves)
{
    const auto half = octaves * (double) pointsPerOctave * 0.5;
    const auto reach = (int) std::floor (half);

    if (reach < 1)
        return curve;

    Curve result {};

    for (int i = 0; i < numPoints; ++i)
    {
        auto sum = 0.0, weights = 0.0;

        for (int offset = -reach; offset <= reach; ++offset)
        {
            const auto index = i + offset;

            if (index < 0 || index >= numPoints)
                continue;

            const auto weight = 0.5 + 0.5 * std::cos (std::numbers::pi * (double) offset / (half + 1.0));

            sum += weight * curve[(size_t) index];
            weights += weight;
        }

        result[(size_t) i] = sum / weights;
    }

    return result;
}

Curve makeTarget (const Curve& referenceDb, const Curve& currentDb, const Curve& existingDb, const Settings& settings)
{
    Curve difference {};

    for (int i = 0; i < numPoints; ++i)
        difference[(size_t) i] = referenceDb[(size_t) i] - currentDb[(size_t) i];

    auto target = smooth (difference, settings.smoothingOctaves);
    const auto mean = meanBetween (target, meanLowHz, meanHighHz);

    for (int i = 0; i < numPoints; ++i)
    {
        const auto hz = hzAt (i);
        auto octavesOutside = 0.0;

        if (hz < settings.lowHz)
            octavesOutside = std::log2 (settings.lowHz / hz);
        else if (hz > settings.highHz)
            octavesOutside = std::log2 (hz / settings.highHz);

        const auto fade = 0.5 + 0.5 * std::cos (std::numbers::pi * std::min (1.0, octavesOutside / fadeOctaves));
        const auto wanted = std::clamp (settings.amount * (target[(size_t) i] - mean), -maxGainDb, maxGainDb);

        target[(size_t) i] = fade * (wanted - existingDb[(size_t) i]);
    }

    return target;
}

Curve responseOf (const std::vector<BandSpec>& bands, double sampleRate)
{
    const Grid grid (sampleRate, {});

    Model model;
    model.build (bands, grid);
    return model.total;
}

double rmsBetween (const Curve& curve, double lowHz, double highHz)
{
    auto sum = 0.0;
    auto count = 0;

    for (int i = 0; i < numPoints; ++i)
    {
        const auto hz = hzAt (i);

        if (hz >= lowHz && hz <= highHz)
        {
            sum += curve[(size_t) i] * curve[(size_t) i];
            ++count;
        }
    }

    return count > 0 ? std::sqrt (sum / count) : 0.0;
}

double rmsDifference (const Curve& a, const Curve& b, double lowHz, double highHz)
{
    Curve difference {};

    for (int i = 0; i < numPoints; ++i)
        difference[(size_t) i] = a[(size_t) i] - b[(size_t) i];

    return rmsBetween (difference, lowHz, highHz);
}

// Fit -------------------------------------------------------------------------------------------

std::vector<BandSpec> fit (const Curve& target, const Settings& settings, double sampleRate)
{
    const Grid grid (sampleRate, settings);
    const auto maxBands = std::clamp (settings.maxBands, 0, eq::maxBands);

    std::vector<BandSpec> bands;

    if (maxBands == 0)
        return bands;

    // Shelves first, where an end of the range sits away from 0. With fewer than three bands
    // the bells are worth more.
    if (maxBands >= 3)
    {
        const auto sixth = std::pow (2.0, 1.0 / 6.0);
        const auto lowEnd = meanBetween (target, settings.lowHz, settings.lowHz * 2.0);
        const auto highEnd = meanBetween (target, settings.highHz * 0.5, settings.highHz);
        const auto lowEdge = meanBetween (target, settings.lowHz, settings.lowHz * sixth);
        const auto highEdge = meanBetween (target, settings.highHz / sixth, settings.highHz);

        // A shelf's level holds to the edge of the range; a bell's near the edge has fallen away.
        const auto holds = [] (double end, double edge)
        {
            return std::abs (end) >= shelfThresholdDb && end * edge > 0.0 && std::abs (edge) >= shelfEdgeShare * std::abs (end);
        };

        if (holds (lowEnd, lowEdge))
        {
            // Up from the bottom of the range to where the target has come halfway back.
            auto corner = settings.lowHz * 2.0;

            for (int i = 0; i < numPoints; ++i)
            {
                if (hzAt (i) < settings.lowHz)
                    continue;

                corner = hzAt (i);

                if (target[(size_t) i] * (lowEnd > 0.0 ? 1.0 : -1.0) < 0.5 * std::abs (lowEnd))
                    break;
            }

            bands.push_back (makeBand (BandType::lowShelf, grid.clampFrequency (BandType::lowShelf, std::max (corner, settings.lowHz * 1.5)),
                                       lowEnd, butterworthQ));
        }

        if (holds (highEnd, highEdge))
        {
            auto corner = settings.highHz * 0.5;

            for (int i = numPoints - 1; i >= 0; --i)
            {
                if (hzAt (i) > settings.highHz)
                    continue;

                corner = hzAt (i);

                if (target[(size_t) i] * (highEnd > 0.0 ? 1.0 : -1.0) < 0.5 * std::abs (highEnd))
                    break;
            }

            bands.push_back (makeBand (BandType::highShelf, grid.clampFrequency (BandType::highShelf, std::min (corner, settings.highHz / 1.5)),
                                       highEnd, butterworthQ));
        }
    }

    auto error = refine (bands, target, grid);

    while ((int) bands.size() < maxBands)
    {
        Model model;
        model.build (bands, grid);

        // The largest remaining error inside the range.
        auto worst = -1;
        auto worstValue = 0.0;

        for (int i = 0; i < numPoints; ++i)
        {
            const auto value = grid.weight[(size_t) i] * std::abs (target[(size_t) i] - model.total[(size_t) i]);

            if (grid.inside[(size_t) i] && value > worstValue)
            {
                worstValue = value;
                worst = i;
            }
        }

        if (worst < 0)
            break;

        const auto residual = [&] (int i) { return target[(size_t) i] - model.total[(size_t) i]; };
        const auto peak = residual (worst);

        if (std::abs (peak) < smallestErrorDb)
            break;

        // Q from how wide the error is at half its height.
        const auto sign = peak > 0.0 ? 1.0 : -1.0;
        auto left = worst, right = worst;

        while (left > 0 && residual (left - 1) * sign > 0.5 * std::abs (peak))
            --left;

        while (right < numPoints - 1 && residual (right + 1) * sign > 0.5 * std::abs (peak))
            ++right;

        const auto widthOctaves = (double) (right - left + 2) / (double) pointsPerOctave;
        const auto q = 1.0 / (2.0 * std::sinh (0.5 * std::numbers::ln2 * widthOctaves));

        auto candidate = bands;
        candidate.push_back (makeBand (BandType::band, grid.clampFrequency (BandType::band, hzAt (worst)), peak, q));

        const auto candidateError = refine (candidate, target, grid);

        // Another band has to be worth it: 0.1 dB of RMS error, or a twentieth of what's left
        // when that's already small.
        if (error - candidateError < std::min (0.1, 0.05 * error))
            break;

        bands = std::move (candidate);
        error = candidateError;
    }

    bands.erase (std::remove_if (bands.begin(), bands.end(), [] (const BandSpec& band) { return std::abs (band.gainDb) < smallestGainDb; }),
                 bands.end());

    std::stable_sort (bands.begin(), bands.end(), [] (const BandSpec& a, const BandSpec& b) { return a.frequencyHz < b.frequencyHz; });

    return bands;
}

// Engine ----------------------------------------------------------------------------------------

const juce::Identifier Engine::treeType ("MATCH");

Engine::Engine() : Thread ("Fancy EQ match")
{
    scratch.resize (16384);
}

Engine::~Engine()
{
    stopThread (4000);
}

void Engine::pushMain (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (learning.load() == (int) Learning::none)
        return;

    if (auto* fifo = mainFifo.load())
        fifo->push (channels, numChannels, numSamples);
}

void Engine::pushSidechain (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (! isLearningFromSidechain() || numChannels <= 0)
        return;

    if (auto* fifo = sidechainFifo.load())
        fifo->push (channels, numChannels, numSamples);
}

void Engine::setPanelOpen (bool shouldBeOpen)
{
    if (shouldBeOpen)
    {
        if (mainStore == nullptr)
        {
            mainStore = std::make_unique<fancy::dsp::SampleFifo>();
            sidechainStore = std::make_unique<fancy::dsp::SampleFifo>();
            mainFifo.store (mainStore.get());
            sidechainFifo.store (sidechainStore.get());
        }

        if (! isThreadRunning())
            startThread (juce::Thread::Priority::low);

        return;
    }

    setLearning (Learning::none);
    stopThread (4000);
}

void Engine::prepareLearners (double rate)
{
    if (rate <= 0.0 || rate == preparedRate)
        return;

    reference.prepare (rate);
    current.prepare (rate);
    preparedRate = rate;
}

void Engine::run()
{
    while (! threadShouldExit())
    {
        const auto mode = getLearning();
        auto changed = false;

        {
            const juce::ScopedLock lock (dataLock);
            prepareLearners (sampleRate.load (std::memory_order_relaxed));
        }

        // Both FIFOs are always emptied, so nothing old is waiting when learning starts.
        for (int count = 0; (count = mainStore->pop (scratch.data(), (int) scratch.size())) > 0;)
        {
            const juce::ScopedLock lock (dataLock);

            if (mode == Learning::both || mode == Learning::current)
                current.addSamples (scratch.data(), count);
            else if (mode == Learning::reference)
                reference.addSamples (scratch.data(), count);

            changed = changed || mode != Learning::none;
        }

        for (int count = 0; (count = sidechainStore->pop (scratch.data(), (int) scratch.size())) > 0;)
        {
            const juce::ScopedLock lock (dataLock);

            if (mode == Learning::both)
            {
                reference.addSamples (scratch.data(), count);
                changed = true;
            }
        }

        if (changed)
            version.fetch_add (1, std::memory_order_relaxed);

        // A fit, if one is asked for. A newer request made meanwhile runs on the next pass.
        Curve target {};
        Settings wanted;
        auto rate = 48000.0;
        auto request = 0;

        {
            const juce::ScopedLock lock (fitLock);

            if (fitStarted != fitRequested)
            {
                request = fitStarted = fitRequested;
                target = fitTarget;
                wanted = fitSettings;
                rate = fitRate;
            }
        }

        if (request != 0)
        {
            auto result = fit (target, wanted, rate);

            const juce::ScopedLock lock (fitLock);
            fitResult = std::move (result);
            fitFinished = request;
            continue;
        }

        wait (20);
    }
}

void Engine::clear()
{
    const juce::ScopedLock lock (dataLock);

    reference.clear();
    current.clear();
    version.fetch_add (1, std::memory_order_relaxed);
}

double Engine::getReferenceSeconds() const
{
    const juce::ScopedLock lock (dataLock);
    return reference.getSeconds();
}

double Engine::getCurrentSeconds() const
{
    const juce::ScopedLock lock (dataLock);
    return current.getSeconds();
}

Curve Engine::getReferenceDb() const
{
    const juce::ScopedLock lock (dataLock);
    return reference.getSpectrumDb();
}

Curve Engine::getCurrentDb() const
{
    const juce::ScopedLock lock (dataLock);
    return current.getSpectrumDb();
}

std::vector<Engine::Written> Engine::getWritten() const
{
    const juce::ScopedLock lock (dataLock);
    return written;
}

void Engine::setWritten (std::vector<Written> newWritten)
{
    const juce::ScopedLock lock (dataLock);
    written = std::move (newWritten);
}

void Engine::requestFit (const Curve& target, const Settings& wanted, double rate)
{
    {
        const juce::ScopedLock lock (fitLock);

        fitTarget = target;
        fitSettings = wanted;
        fitRate = rate;
        ++fitRequested;
    }

    notify();
}

bool Engine::isFitting() const
{
    const juce::ScopedLock lock (fitLock);
    return fitFinished != fitRequested;
}

bool Engine::takeFit (std::vector<BandSpec>& bands)
{
    const juce::ScopedLock lock (fitLock);

    if (fitFinished != fitRequested || fitTaken == fitFinished)
        return false;

    fitTaken = fitFinished;
    bands = fitResult;
    return true;
}

void Engine::learnNow (bool toReference, const float* samples, int numSamples, double rate)
{
    const juce::ScopedLock lock (dataLock);

    prepareLearners (rate);
    (toReference ? reference : current).addSamples (samples, numSamples);
    version.fetch_add (1, std::memory_order_relaxed);
}

void Engine::restoreSpectra (const Curve& referenceDb, const Curve& currentDb, double seconds)
{
    const juce::ScopedLock lock (dataLock);

    reference.restore (referenceDb, seconds);
    current.restore (currentDb, seconds);
    version.fetch_add (1, std::memory_order_relaxed);
}

// State -----------------------------------------------------------------------------------------

namespace
{
juce::String encode (const Curve& curve)
{
    std::array<float, numPoints> values {};

    for (int i = 0; i < numPoints; ++i)
        values[(size_t) i] = (float) curve[(size_t) i];

    return juce::MemoryBlock (values.data(), sizeof (float) * values.size()).toBase64Encoding();
}

bool decode (const juce::String& text, Curve& curve)
{
    juce::MemoryBlock block;

    if (! block.fromBase64Encoding (text) || block.getSize() != sizeof (float) * (size_t) numPoints)
        return false;

    const auto* values = static_cast<const float*> (block.getData());

    for (int i = 0; i < numPoints; ++i)
    {
        if (! std::isfinite (values[i]))
            return false;

        curve[(size_t) i] = std::clamp ((double) values[i], -300.0, 100.0);
    }

    return true;
}

double finiteOr (const juce::var& value, double fallback)
{
    const auto number = (double) value;
    return value.isVoid() || ! std::isfinite (number) ? fallback : number;
}
} // namespace

juce::ValueTree Engine::toTree() const
{
    juce::ValueTree tree (treeType);

    tree.setProperty ("source", (int) source, nullptr);
    tree.setProperty ("smoothing", settings.smoothingOctaves, nullptr);
    tree.setProperty ("amount", settings.amount, nullptr);
    tree.setProperty ("bands", settings.maxBands, nullptr);
    tree.setProperty ("low", settings.lowHz, nullptr);
    tree.setProperty ("high", settings.highHz, nullptr);

    {
        const juce::ScopedLock lock (dataLock);

        if (reference.getSeconds() > 0.0)
        {
            tree.setProperty ("refSeconds", reference.getSeconds(), nullptr);
            tree.setProperty ("ref", encode (reference.getSpectrumDb()), nullptr);
        }

        if (current.getSeconds() > 0.0)
        {
            tree.setProperty ("curSeconds", current.getSeconds(), nullptr);
            tree.setProperty ("cur", encode (current.getSpectrumDb()), nullptr);
        }
    }

    juce::StringArray items;

    for (const auto& item : getWritten())
        items.add (juce::String (item.band) + ":" + juce::String (item.type) + ":" + juce::String (item.frequencyHz, 4)
                   + ":" + juce::String (item.gainDb, 4) + ":" + juce::String (item.q, 5));

    tree.setProperty ("written", items.joinIntoString (";"), nullptr);
    return tree;
}

void Engine::fromTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (treeType))
        return;

    const Settings defaults {};

    source = (int) finiteOr (tree.getProperty ("source"), 0.0) == 1 ? Source::captured : Source::sidechain;
    settings.smoothingOctaves = std::clamp (finiteOr (tree.getProperty ("smoothing"), defaults.smoothingOctaves), 1.0 / 12.0, 1.0);
    settings.amount = std::clamp (finiteOr (tree.getProperty ("amount"), defaults.amount), 0.0, 1.0);
    settings.maxBands = std::clamp ((int) finiteOr (tree.getProperty ("bands"), (double) defaults.maxBands), 1, eq::maxBands);
    settings.lowHz = std::clamp (finiteOr (tree.getProperty ("low"), defaults.lowHz), 20.0, 1000.0);
    settings.highHz = std::clamp (finiteOr (tree.getProperty ("high"), defaults.highHz), 1000.0, 20000.0);

    {
        const juce::ScopedLock lock (dataLock);
        Curve curve {};

        reference.clear();
        current.clear();

        if (decode (tree.getProperty ("ref").toString(), curve))
            reference.restore (curve, std::clamp (finiteOr (tree.getProperty ("refSeconds"), 0.0), 0.0, 1.0e6));

        if (decode (tree.getProperty ("cur").toString(), curve))
            current.restore (curve, std::clamp (finiteOr (tree.getProperty ("curSeconds"), 0.0), 0.0, 1.0e6));

        version.fetch_add (1, std::memory_order_relaxed);
    }

    std::vector<Written> entries;

    juce::StringArray items;
    items.addTokens (tree.getProperty ("written").toString(), ";", "");

    for (const auto& item : items)
    {
        juce::StringArray fields;
        fields.addTokens (item, ":", "");

        if (fields.size() != 5)
            continue;

        Written entry;
        entry.band = fields[0].getIntValue();
        entry.type = fields[1].getIntValue();
        entry.frequencyHz = fields[2].getFloatValue();
        entry.gainDb = fields[3].getFloatValue();
        entry.q = fields[4].getFloatValue();

        if (juce::isPositiveAndBelow (entry.band, eq::maxBands) && std::isfinite (entry.frequencyHz)
            && std::isfinite (entry.gainDb) && std::isfinite (entry.q))
            entries.push_back (entry);
    }

    setWritten (std::move (entries));
}

} // namespace eq::match
