#include "EqSpectral.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace eq
{

namespace
{
constexpr double kneeDb = 6.0;
constexpr float weightFloor = 1.0e-4f;

// As the band's own gain computer (EqDsp.cpp): how far past Threshold, with a 6 dB soft knee.
double distancePast (double distanceDb) noexcept
{
    constexpr auto half = kneeDb * 0.5;

    if (distanceDb <= -half)
        return 0.0;

    if (distanceDb < half)
        return (distanceDb + half) * (distanceDb + half) / (2.0 * kneeDb);

    return distanceDb;
}
} // namespace

void SpectralStage::prepare (double sampleRate)
{
    rate = sampleRate;
    frames.prepare ({ frameLengthFor (sampleRate), false, false, 0, 1 });
    bins = frames.getBins();
    hop = frames.getHop();

    for (auto& spectrum : spectra)
        spectrum.assign ((size_t) frames.getSpectrumSize(), 0.0f);

    power.assign ((size_t) bins, 0.0f);

    for (auto& source : levels)
        for (auto& domain : source)
            domain.assign ((size_t) bins, -300.0f);

    for (auto& domain : gainDb)
        domain.assign ((size_t) bins, 0.0f);

    for (auto& state : bandStates)
    {
        state.weight.assign ((size_t) bins, 0.0f);
        state.change.assign ((size_t) bins, 0.0f);
        state.shaped = false;
    }

    // Calibration: the three-bin power of a unit sine (amplitude 1, RMS 0.5 in power), averaged
    // over a sine on a bin and one halfway between, reads 0.5. With this window the sum varies by
    // under 0.05 dB across a bin.
    {
        const auto n = frames.getFrameLength();
        const auto k0 = n / 8;
        auto sum = 0.0;

        for (const auto offset : { 0.0, 0.5 })
        {
            auto threeBins = 0.0;

            for (int k = k0 - 1; k <= k0 + 1; ++k)
            {
                auto re = 0.0, im = 0.0;

                for (int i = 0; i < n; ++i)
                {
                    const auto window = std::sqrt (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * (double) i / (double) n));
                    const auto x = window * std::cos (2.0 * std::numbers::pi * ((double) k0 + offset) * (double) i / (double) n);
                    re += x * std::cos (2.0 * std::numbers::pi * (double) k * (double) i / (double) n);
                    im -= x * std::sin (2.0 * std::numbers::pi * (double) k * (double) i / (double) n);
                }

                threeBins += re * re + im * im;
            }

            sum += 0.5 * threeBins;
        }

        calibration = 0.5 / sum;
    }

    curveBin.resize (curvePoints);
    curveFraction.resize (curvePoints);
    const auto binHz = rate / (double) frames.getFftLength();

    for (int point = 0; point < curvePoints; ++point)
    {
        const auto position = std::min (curveHz (point) / binHz, (double) (bins - 2));
        curveBin[(size_t) point] = (int) position;
        curveFraction[(size_t) point] = (float) (position - std::floor (position));
    }

    reset();
}

void SpectralStage::reset() noexcept
{
    frames.reset();

    for (auto& state : bandStates)
    {
        std::fill (state.change.begin(), state.change.end(), 0.0f);
        state.running = false;
        state.deepest = 0.0;
    }

    for (auto& point : curve)
        point.store (0.0f, std::memory_order_relaxed);
}

void SpectralStage::setBands (const std::array<BandSpec, maxBands>& bands, const std::array<DynamicSpec, maxBands>& dynamics,
                              const std::array<bool, maxBands>& spectral) noexcept
{
    bandSpecs = bands;
    bandDynamics = dynamics;
    spectralBands = spectral;
}

// The band's shape at -1 dB, as a weight from 0 to 1 per bin. A notch weighs as the bell it acts
// as when dynamic.
void SpectralStage::updateWeights (int band) noexcept
{
    auto shape = bandSpecs[(size_t) band];
    shape.on = true;
    shape.channel = Channel::stereo;
    shape.gainDb = 0.0;

    if (shape.type == BandType::notch)
        shape.type = BandType::band;

    auto& state = bandStates[(size_t) band];

    if (state.shaped && shape == state.shape)
        return;

    state.shape = shape;
    state.shaped = true;

    auto designed = shape;
    designed.gainDb = -1.0;
    const auto filter = design (designed, rate);

    auto deepest = 0.0f;

    for (int k = 0; k < bins; ++k)
    {
        const auto hz = std::min ((double) k * rate / (double) frames.getFftLength(), 0.499 * rate);
        const auto s = std::sin (std::numbers::pi * hz / rate);
        const auto db = 10.0 * std::log10 (std::max (filter.powerAt (s * s), 1.0e-30));
        state.weight[(size_t) k] = (float) std::max (0.0, -db);
        deepest = std::max (deepest, state.weight[(size_t) k]);
    }

    state.first = bins;
    state.last = -1;

    for (int k = 0; k < bins; ++k)
    {
        auto& weight = state.weight[(size_t) k];
        weight = deepest > 0.0f ? std::min (1.0f, weight / deepest) : 0.0f;

        if (weight < weightFloor)
        {
            weight = 0.0f;
            continue;
        }

        state.first = std::min (state.first, k);
        state.last = k;
    }
}

// Each bin's level in dB, from the three bins round it, for one source (main or sidechain) and
// one domain: L, R, M = (L + R) / 2 or S = (L - R) / 2. Worked out once per frame, when asked.
const std::vector<float>& SpectralStage::levelsFor (bool key, int domain) noexcept
{
    auto& out = levels[key ? 1 : 0][(size_t) domain];

    if (levelsReady[key ? 1 : 0][(size_t) domain])
        return out;

    levelsReady[key ? 1 : 0][(size_t) domain] = true;

    const auto& a = spectra[key ? 2 : 0];
    const auto& b = spectra[key ? 3 : 1];

    for (int k = 0; k < bins; ++k)
    {
        const auto i = (size_t) (2 * k);
        float re = a[i], im = a[i + 1];

        if (domain == 1)
        {
            re = b[i];
            im = b[i + 1];
        }
        else if (domain == 2)
        {
            re = 0.5f * (a[i] + b[i]);
            im = 0.5f * (a[i + 1] + b[i + 1]);
        }
        else if (domain == 3)
        {
            re = 0.5f * (a[i] - b[i]);
            im = 0.5f * (a[i + 1] - b[i + 1]);
        }

        power[(size_t) k] = re * re + im * im;
    }

    for (int k = 0; k < bins; ++k)
    {
        const auto sum = (double) power[(size_t) k] + (k > 0 ? (double) power[(size_t) (k - 1)] : 0.0)
                       + (k + 1 < bins ? (double) power[(size_t) (k + 1)] : 0.0);
        out[(size_t) k] = (float) (10.0 * std::log10 (sum * calibration + 1.0e-30));
    }

    return out;
}

void SpectralStage::runFrame() noexcept
{
    frames.analyse (0, spectra[0]);

    if (frameChannels > 1)
        frames.analyse (1, spectra[1]);

    auto keyAnalysed = false;

    for (auto& source : levelsReady)
        source.fill (false);

    for (auto& domain : gainDb)
        std::fill (domain.begin(), domain.end(), 0.0f);

    std::array<bool, 4> domainUsed {};

    for (int b = 0; b < maxBands; ++b)
    {
        auto& state = bandStates[(size_t) b];
        const auto on = spectralBands[(size_t) b];

        if (! on && ! state.running)
            continue;

        updateWeights (b);

        const auto& spec = bandSpecs[(size_t) b];
        const auto& settings = bandDynamics[(size_t) b];

        // A mono bus has one channel, which every band acts on.
        const auto channel = frameChannels < 2 ? Channel::stereo : spec.channel;
        const auto useKey = on && settings.sidechain && frameKey;

        if (useKey && ! keyAnalysed)
        {
            frames.analyse (2, spectra[2]);
            frames.analyse (3, spectra[3]);
            keyAnalysed = true;
        }

        const std::vector<float>* levelA = nullptr;
        const std::vector<float>* levelB = nullptr;

        if (on)
        {
            switch (channel)
            {
                case Channel::left:  levelA = &levelsFor (useKey, 0); break;
                case Channel::right: levelA = &levelsFor (useKey, 1); break;
                case Channel::mid:   levelA = &levelsFor (useKey, 2); break;
                case Channel::side:  levelA = &levelsFor (useKey, 3); break;
                case Channel::stereo:
                default:
                    levelA = &levelsFor (useKey, 0);

                    if (frameChannels > 1)
                        levelB = &levelsFor (useKey, 1);
                    break;
            }
        }

        const auto attack = std::exp (-(double) hop / (std::max (0.01, settings.attackMs) * 0.001 * rate));
        const auto release = std::exp (-(double) hop / (std::max (0.01, settings.releaseMs) * 0.001 * rate));
        const auto notch = spec.type == BandType::notch;
        const auto reach = std::max (0.1, settings.reachDb);

        auto deepest = 0.0f;
        auto moving = false;

        for (int k = state.first; k <= state.last; ++k)
        {
            auto& change = state.change[(size_t) k];
            auto target = 0.0;

            if (on)
            {
                auto level = (double) (*levelA)[(size_t) k];

                if (levelB != nullptr)
                    level = std::max (level, (double) (*levelB)[(size_t) k]);

                const auto distance = settings.below ? settings.thresholdDb - level : level - settings.thresholdDb;
                target = settings.rangeDb * std::min (1.0, distancePast (distance) / reach);

                if (notch)
                    target = std::min (0.0, target);

                target *= (double) state.weight[(size_t) k];
            }

            const auto coefficient = std::abs (target) > std::abs ((double) change) ? attack : release;
            auto next = target + coefficient * ((double) change - target);

            // Letting go ends exactly at 0, so a quiet bin is exactly as it was.
            if (target == 0.0 && std::abs (next) < 1.0e-6)
                next = 0.0;

            change = std::isfinite (next) ? (float) next : 0.0f;

            if (change != 0.0f)
                moving = true;

            if (std::abs (change) > std::abs (deepest))
                deepest = change;
        }

        state.running = on || moving;
        state.deepest = (double) deepest;

        if (! moving)
            continue;

        const auto add = [&] (int domain)
        {
            domainUsed[(size_t) domain] = true;
            auto& sum = gainDb[(size_t) domain];

            for (int k = state.first; k <= state.last; ++k)
                sum[(size_t) k] += state.change[(size_t) k];
        };

        switch (channel)
        {
            case Channel::left:  add (0); break;
            case Channel::right: add (1); break;
            case Channel::mid:   add (2); break;
            case Channel::side:  add (3); break;
            case Channel::stereo:
            default:             add (0); add (1); break;
        }
    }

    // Mid and Side first, formed per bin and turned back, then Left and Right.
    if (frameChannels > 1 && (domainUsed[2] || domainUsed[3]))
    {
        auto& left = spectra[0];
        auto& right = spectra[1];

        for (int k = 0; k < bins; ++k)
        {
            const auto midGain = domainUsed[2] && gainDb[2][(size_t) k] != 0.0f ? std::pow (10.0f, gainDb[2][(size_t) k] / 20.0f) : 1.0f;
            const auto sideGain = domainUsed[3] && gainDb[3][(size_t) k] != 0.0f ? std::pow (10.0f, gainDb[3][(size_t) k] / 20.0f) : 1.0f;

            if (midGain == 1.0f && sideGain == 1.0f)
                continue;

            for (const auto part : { 0, 1 })
            {
                const auto i = (size_t) (2 * k + part);
                const auto mid = 0.5f * (left[i] + right[i]) * midGain;
                const auto side = 0.5f * (left[i] - right[i]) * sideGain;
                left[i] = mid + side;
                right[i] = mid - side;
            }
        }
    }

    for (int ch = 0; ch < frameChannels; ++ch)
    {
        if (! domainUsed[(size_t) ch])
            continue;

        auto& spectrum = spectra[(size_t) ch];

        for (int k = 0; k < bins; ++k)
        {
            const auto db = gainDb[(size_t) ch][(size_t) k];

            if (db == 0.0f)
                continue;

            const auto g = std::pow (10.0f, db / 20.0f);
            spectrum[(size_t) (2 * k)] *= g;
            spectrum[(size_t) (2 * k + 1)] *= g;
        }
    }

    frames.synthesise (0, spectra[0]);

    if (frameChannels > 1)
        frames.synthesise (1, spectra[1]);

    // The graph: what a signal in both channels keeps, Left and Right averaged, plus Mid.
    for (int point = 0; point < curvePoints; ++point)
    {
        const auto k = (size_t) curveBin[(size_t) point];
        const auto f = curveFraction[(size_t) point];
        const auto at = [&] (size_t bin) { return 0.5f * (gainDb[0][bin] + gainDb[1][bin]) + gainDb[2][bin]; };
        const auto value = frameChannels > 1 ? at (k) + f * (at (k + 1) - at (k)) : gainDb[0][k] + f * (gainDb[0][k + 1] - gainDb[0][k]);
        curve[(size_t) point].store (value, std::memory_order_relaxed);
    }
}

void SpectralStage::process (float* const* channels, int numChannels, int numSamples, const float* const* sidechain, int numSidechain) noexcept
{
    numChannels = std::clamp (numChannels, 1, (int) maxChannels);
    const auto hasKey = sidechain != nullptr && numSidechain > 0;

    const auto runStage = [this] (int) { runFrame(); };

    const auto startFrame = [&]
    {
        frameChannels = numChannels;
        frameKey = hasKey;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        frames.write (0, channels[0][i]);
        frames.write (1, numChannels > 1 ? channels[1][i] : channels[0][i]);
        frames.write (2, hasKey ? sidechain[0][i] : 0.0f);
        frames.write (3, hasKey ? sidechain[std::min (1, numSidechain - 1)][i] : 0.0f);

        frames.advance (runStage, startFrame);

        channels[0][i] = frames.read (0);
        frames.clear (0);

        if (numChannels > 1)
        {
            channels[1][i] = frames.read (1);
            frames.clear (1);
        }
    }
}

} // namespace eq
