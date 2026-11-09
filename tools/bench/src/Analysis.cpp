#include "Analysis.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace bench
{

namespace
{

constexpr double pi = std::numbers::pi;

using Complex = std::complex<double>;

std::vector<Complex> transform (const Signal& signal, int size)
{
    std::vector<Complex> data ((size_t) size);

    for (size_t i = 0; i < std::min (signal.size(), data.size()); ++i)
        data[i] = signal[i];

    fft (data, false);
    return data;
}

void normaliseRms (Signal& signal, double rmsDb)
{
    double sum = 0.0;

    for (const auto x : signal)
        sum += (double) x * x;

    if (sum <= 0.0)
        return;

    const auto gain = std::pow (10.0, rmsDb / 20.0) / std::sqrt (sum / (double) signal.size());

    for (auto& x : signal)
        x = (float) (x * gain);
}

// 4-term Blackman-Harris, in its periodic form: with a sine on an exact bin, its spectrum is
// non-zero only within 3 bins of the sine, so nothing leaks further.
std::vector<double> blackmanHarris (int size)
{
    std::vector<double> window ((size_t) size);

    for (int n = 0; n < size; ++n)
    {
        const auto x = 2.0 * pi * n / size;
        window[(size_t) n] = 0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2.0 * x) - 0.01168 * std::cos (3.0 * x);
    }

    return window;
}

struct Spectrum
{
    std::vector<double> power; // |X|², bins 0 to N/2
    double windowSum = 0.0, windowEnergy = 0.0;
    int size = 0;

    double lobe (int centre, int halfWidth = 4) const
    {
        double sum = 0.0;

        for (int k = std::max (1, centre - halfWidth); k <= std::min ((int) power.size() - 1, centre + halfWidth); ++k)
            sum += power[(size_t) k];

        return sum;
    }

    // Peak amplitude of a sine whose lobe holds `lobePower`: the lobe sums (A/2)² N Σw².
    double amplitude (double lobePower) const
    {
        return 2.0 * std::sqrt (lobePower / ((double) size * windowEnergy));
    }
};

Spectrum spectrum (const Signal& signal)
{
    Spectrum result;
    result.size = (int) signal.size();

    const auto window = blackmanHarris (result.size);
    std::vector<Complex> data ((size_t) result.size);

    for (size_t i = 0; i < signal.size(); ++i)
    {
        data[i] = signal[i] * window[i];
        result.windowSum += window[i];
        result.windowEnergy += window[i] * window[i];
    }

    fft (data, false);
    result.power.resize ((size_t) result.size / 2 + 1);

    for (size_t k = 0; k < result.power.size(); ++k)
        result.power[k] = std::norm (data[k]);

    return result;
}

struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double process (double x) noexcept
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    static Biquad normalised (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    // RBJ cookbook.
    static Biquad notch (double hz, double rate, double q)
    {
        const auto w = 2.0 * pi * hz / rate;
        const auto alpha = std::sin (w) / (2.0 * q);
        return normalised (1.0, -2.0 * std::cos (w), 1.0, 1.0 + alpha, -2.0 * std::cos (w), 1.0 - alpha);
    }

    static Biquad highPass (double hz, double rate, double q)
    {
        const auto w = 2.0 * pi * hz / rate;
        const auto alpha = std::sin (w) / (2.0 * q);
        const auto c = std::cos (w);
        return normalised ((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }
};

} // namespace

double toDb (double amplitude) noexcept
{
    return amplitude > 0.0 ? std::max (floorDb, 20.0 * std::log10 (amplitude)) : floorDb;
}

double powerToDb (double power) noexcept
{
    return power > 0.0 ? std::max (floorDb, 10.0 * std::log10 (power)) : floorDb;
}

int nextPowerOfTwo (int n) noexcept
{
    int size = 1;

    while (size < n)
        size <<= 1;

    return size;
}

void fft (std::vector<Complex>& data, bool inverse)
{
    const auto n = data.size();

    if (n < 2)
        return;

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;

        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
            std::swap (data[i], data[j]);
    }

    // One table of twiddles, each computed directly, so large sizes keep their precision.
    std::vector<Complex> twiddles (n / 2);
    const auto sign = inverse ? 1.0 : -1.0;

    for (size_t k = 0; k < n / 2; ++k)
        twiddles[k] = std::polar (1.0, sign * 2.0 * pi * (double) k / (double) n);

    for (size_t length = 2; length <= n; length <<= 1)
    {
        const auto step = n / length;
        const auto half = length / 2;

        for (size_t start = 0; start < n; start += length)
        {
            for (size_t j = 0; j < half; ++j)
            {
                const auto u = data[start + j];
                const auto v = data[start + j + half] * twiddles[j * step];
                data[start + j] = u + v;
                data[start + j + half] = u - v;
            }
        }
    }

    if (inverse)
        for (auto& x : data)
            x /= (double) n;
}

Signal whiteNoise (int numSamples, double rmsDb, uint32_t seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<double> gauss (0.0, 1.0);
    Signal signal ((size_t) numSamples);

    for (auto& x : signal)
        x = (float) gauss (random);

    normaliseRms (signal, rmsDb);
    return signal;
}

// Paul Kellet's refined pink filter on Gaussian white noise.
Signal pinkNoise (int numSamples, double rmsDb, uint32_t seed)
{
    std::mt19937 random (seed);
    std::normal_distribution<double> gauss (0.0, 1.0);
    Signal signal ((size_t) numSamples);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

    for (int i = -48000; i < numSamples; ++i) // a second to settle the slowest pole
    {
        const auto w = gauss (random);
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        const auto pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
        b6 = w * 0.115926;

        if (i >= 0)
            signal[(size_t) i] = (float) pink;
    }

    normaliseRms (signal, rmsDb);
    return signal;
}

Signal uniformNoise (int numSamples, float amplitude, uint32_t seed)
{
    std::mt19937 random (seed);
    std::uniform_real_distribution<float> noise (-amplitude, amplitude);
    Signal signal ((size_t) numSamples);

    for (auto& x : signal)
        x = noise (random);

    return signal;
}

Signal sine (int numSamples, double hz, double rate, double peakDb, double phase)
{
    const auto amplitude = std::pow (10.0, peakDb / 20.0);
    Signal signal ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        signal[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * hz * i / rate + phase));

    return signal;
}

int analysisLength (double rate) noexcept
{
    return nextPowerOfTwo ((int) std::ceil (rate));
}

double binCentred (double hz, double rate) noexcept
{
    const auto size = analysisLength (rate);
    const auto bin = std::max (1.0, std::round (hz * size / rate));
    return bin * rate / size;
}

Sweep logSweep (double startHz, double endHz, double seconds, double rate, double peakDb)
{
    Sweep sweep;
    sweep.startHz = startHz;
    sweep.endHz = endHz;
    sweep.rate = rate;

    const auto numSamples = (int) std::round (seconds * rate);
    const auto amplitude = std::pow (10.0, peakDb / 20.0);
    const auto l = seconds / std::log (endHz / startHz);
    const auto fadeIn = (int) (0.010 * rate), fadeOut = (int) (0.005 * rate);

    sweep.signal.resize ((size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = i / rate;
        auto gain = amplitude;

        if (i < fadeIn)
            gain *= 0.5 - 0.5 * std::cos (pi * i / fadeIn);
        else if (i >= numSamples - fadeOut)
            gain *= 0.5 - 0.5 * std::cos (pi * (numSamples - 1 - i) / fadeOut);

        sweep.signal[(size_t) i] = (float) (gain * std::sin (2.0 * pi * startHz * l * (std::exp (t / l) - 1.0)));
    }

    return sweep;
}

int measureDelay (const Signal& input, const Signal& output, int minLag, int maxLag)
{
    double energy = 0.0;

    for (const auto x : output)
        energy += (double) x * x;

    if (energy < 1.0e-12 * (double) output.size())
        return minLag - 1;

    const auto size = nextPowerOfTwo ((int) (input.size() + output.size()));
    auto x = transform (input, size);
    auto y = transform (output, size);

    for (size_t k = 0; k < x.size(); ++k)
        y[k] *= std::conj (x[k]);

    fft (y, true);

    auto best = minLag;
    auto bestValue = -1.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const auto index = (size_t) ((lag % size + size) % size);
        const auto value = std::abs (y[index].real());

        if (value > bestValue)
        {
            bestValue = value;
            best = lag;
        }
    }

    return best;
}

std::vector<double> logFrequencies (double lowHz, double highHz, int perOctave)
{
    std::vector<double> frequencies;

    for (int k = 0;; ++k)
    {
        const auto hz = lowHz * std::pow (2.0, (double) k / perOctave);

        if (hz > highHz * (1.0 + 1.0e-9))
            break;

        frequencies.push_back (hz);
    }

    return frequencies;
}

std::vector<ResponsePoint> sweepResponse (const Sweep& sweep, const Signal& output, int latency,
                                          const std::vector<double>& frequencies)
{
    const auto size = nextPowerOfTwo ((int) std::max (output.size(), sweep.signal.size()));
    const auto x = transform (sweep.signal, size);
    auto h = transform (output, size);

    double peak = 0.0;

    for (const auto& value : x)
        peak = std::max (peak, std::norm (value));

    const auto regularisation = 1.0e-10 * peak;

    for (size_t k = 0; k < h.size(); ++k)
        h[k] = h[k] * std::conj (x[k]) / (std::norm (x[k]) + regularisation);

    fft (h, true);

    // Keep the linear response around the latency: the harmonic responses land well before it.
    const auto pre = std::min (8192, size / 8);
    const auto post = std::min (size / 2 - pre, (int) (4.0 * sweep.rate));
    const auto fade = 64;

    std::vector<double> response ((size_t) (pre + post));

    for (int m = -pre; m < post; ++m)
    {
        const auto index = (size_t) (((latency + m) % size + size) % size);
        auto gain = 1.0;

        if (m < -pre + fade)
            gain = 0.5 - 0.5 * std::cos (pi * (m + pre) / fade);
        else if (m >= post - fade)
            gain = 0.5 - 0.5 * std::cos (pi * (post - 1 - m) / fade);

        response[(size_t) (m + pre)] = h[index].real() * gain;
    }

    std::vector<ResponsePoint> points;

    for (const auto hz : frequencies)
    {
        // The DTFT at hz, with time measured from the latency.
        const auto w = 2.0 * pi * hz / sweep.rate;
        const Complex rotate = std::polar (1.0, -w);
        Complex phasor = std::polar (1.0, w * pre);
        Complex sum = 0.0;

        for (size_t i = 0; i < response.size(); ++i)
        {
            sum += response[i] * phasor;
            phasor *= rotate;

            if ((i & 4095) == 4095) // keep the phasor on the unit circle
                phasor /= std::abs (phasor);
        }

        points.push_back ({ hz, toDb (std::abs (sum)), std::arg (sum) * 180.0 / pi });
    }

    return points;
}

Distortion distortion (const Signal& output, double hz, double rate)
{
    const auto s = spectrum (output);
    const auto bin = (int) std::round (hz * s.size / rate);
    const auto nyquistBin = s.size / 2;
    const auto fundamental = s.lobe (bin);

    Distortion result;
    result.fundamentalDbfs = toDb (s.amplitude (fundamental));

    if (fundamental <= 0.0)
        return result;

    double harmonics = 0.0;

    for (int m = 2; m <= 10; ++m)
        if (m * bin + 4 < nyquistBin)
            harmonics += s.lobe (m * bin);

    const auto low = (int) std::ceil (20.0 * s.size / rate);
    const auto high = std::min ((int) std::floor (20000.0 * s.size / rate), nyquistBin - 1);
    double total = 0.0;

    for (int k = low; k <= high; ++k)
        if (std::abs (k - bin) > 4)
            total += s.power[(size_t) k];

    const auto thd = std::sqrt (harmonics / fundamental);
    const auto thdn = std::sqrt (total / fundamental);

    result.thdPercent = 100.0 * thd;
    result.thdDb = toDb (thd);
    result.thdnPercent = 100.0 * thdn;
    result.thdnDb = toDb (thdn);
    return result;
}

Aliasing aliasing (const Signal& output, double hz, double rate)
{
    const auto s = spectrum (output);
    const auto bin = (int) std::round (hz * s.size / rate);
    const auto nyquistBin = s.size / 2;
    const auto fundamental = s.lobe (bin);

    Aliasing result;
    result.fundamentalDbfs = toDb (s.amplitude (fundamental));

    result.spectrumDbfs.resize ((size_t) nyquistBin + 1);

    for (size_t k = 0; k < result.spectrumDbfs.size(); ++k)
        result.spectrumDbfs[k] = (float) toDb (2.0 * std::sqrt (s.power[k]) / s.windowSum);

    if (fundamental <= 0.0)
        return result;

    // Bins that are harmonics below Nyquist, or near DC, aren't aliases.
    std::vector<bool> excluded ((size_t) nyquistBin + 1, false);

    for (int m = 1; m * bin - 5 <= nyquistBin; ++m)
        for (int k = std::max (0, m * bin - 5); k <= std::min (nyquistBin, m * bin + 5); ++k)
            excluded[(size_t) k] = true;

    for (int k = 0; k <= std::min (nyquistBin, (int) std::ceil (10.0 * s.size / rate) + 4); ++k)
        excluded[(size_t) k] = true;

    const auto low = (int) std::ceil (20.0 * s.size / rate);
    std::vector<double> floorBins (s.power.begin() + low, s.power.end());
    const auto noiseFloor = median (floorBins);

    // A component's lobe must stand 30 dB over the floor's 9 bins and above -160 dBFS.
    const auto minimum = std::max (9.0 * noiseFloor * 1000.0, std::pow (1.0e-8 / 2.0, 2.0) * s.size * s.windowEnergy);
    double total = 0.0, worst = 0.0;

    for (int k = 1; k < nyquistBin; ++k)
    {
        if (excluded[(size_t) k] || s.power[(size_t) k] < s.power[(size_t) k - 1] || s.power[(size_t) k] < s.power[(size_t) k + 1])
            continue;

        const auto lobe = s.lobe (k);

        if (lobe < minimum)
            continue;

        ++result.count;
        total += lobe;

        if (lobe > worst)
        {
            worst = lobe;
            result.worstHz = k * rate / s.size;
        }

        k += 4;
    }

    if (worst > 0.0)
    {
        result.worstDbfs = toDb (s.amplitude (worst));
        result.worstRelativeDb = powerToDb (worst / fundamental);
        result.totalRelativeDb = powerToDb (total / fundamental);
    }

    return result;
}

Null nullTest (const Signal& input, const Signal& output, int delay, int start)
{
    double difference = 0.0, reference = 0.0, peak = 0.0;
    int count = 0;

    for (int n = std::max (0, start); n < (int) output.size(); ++n)
    {
        const auto j = n - delay;

        if (j < 0 || j >= (int) input.size())
            continue;

        const auto d = (double) output[(size_t) n] - (double) input[(size_t) j];
        difference += d * d;
        reference += (double) input[(size_t) j] * input[(size_t) j];
        peak = std::max (peak, std::abs (d));
        ++count;
    }

    Null result;

    if (count == 0)
        return result;

    result.rmsDbfs = toDb (std::sqrt (difference / count));
    result.peakDbfs = toDb (peak);
    result.relativeDb = reference > 0.0 ? powerToDb (difference / reference) : floorDb;
    return result;
}

Clicks clicks (const Signal& output, double sineHz, double rate, double thresholdDb)
{
    auto notch = Biquad::notch (sineHz, rate, 10.0);
    auto highA = Biquad::highPass (4000.0, rate, 0.54119610);
    auto highB = Biquad::highPass (4000.0, rate, 1.30656296);

    const auto skip = (int) (0.1 * rate);
    const auto gap = (int) (0.010 * rate);
    const auto threshold = std::pow (10.0, thresholdDb / 20.0);

    Clicks result;
    double peak = 0.0;
    int lastAbove = -1;

    for (int n = 0; n < (int) output.size(); ++n)
    {
        const auto y = std::abs (highB.process (highA.process (notch.process (output[(size_t) n]))));

        if (n < skip)
            continue;

        peak = std::max (peak, y);

        if (y > threshold)
        {
            if (lastAbove < 0 || n - lastAbove > gap)
                ++result.count;

            lastAbove = n;
        }
    }

    result.peakDbfs = toDb (peak);
    return result;
}

double percentile (std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;

    std::sort (values.begin(), values.end());
    const auto rank = (int) std::ceil (fraction * (double) values.size()) - 1;
    return values[(size_t) std::clamp (rank, 0, (int) values.size() - 1)];
}

double median (std::vector<double> values)
{
    if (values.empty())
        return 0.0;

    std::sort (values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 1 ? values[middle] : 0.5 * (values[middle - 1] + values[middle]);
}

} // namespace bench
