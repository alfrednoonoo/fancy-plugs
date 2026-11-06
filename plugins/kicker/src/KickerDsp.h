#pragma once

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <cmath>

// See docs/kicker.md. Contains no JUCE types, so tests can drive it directly.
class KickerDsp
{
public:
    struct Targets
    {
        double frequencyHz = 50.0;
        double q = 2.0;
        double wetGain = 0.25;
        double dryGain = 1.0;
        double outputGain = 1.0;
        bool monoWet = false;
        bool softClip = false;
        bool bypass = false;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        logFrequency.setTime (30.0, sampleRate);
        logQ.setTime (30.0, sampleRate);
        wet.setTime (20.0, sampleRate);
        dry.setTime (20.0, sampleRate);
        output.setTime (20.0, sampleRate);
        monoAmount.setTime (20.0, sampleRate);
        softClipAmount.setTime (20.0, sampleRate);
        bypassAmount.setTime (10.0, sampleRate);

        for (auto& filter : subsonic)
            filter.setButterworthHighPass (15.0, sampleRate);

        reset();
    }

    void reset()
    {
        for (auto& filter : svf)
            filter.reset();

        for (auto& filter : subsonic)
            filter.reset();
    }

    void setTargets (const Targets& targets)
    {
        applyTargets (targets, [] (fancy::dsp::Smoother& smoother, double value) { smoother.setTarget (value); });
    }

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets)
    {
        applyTargets (targets, [] (fancy::dsp::Smoother& smoother, double value) { smoother.snapTo (value); });
        updateCoefficients (logFrequency.getCurrent(), logQ.getCurrent());
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (! logFrequency.isSettled() || ! logQ.isSettled())
            {
                const auto nextLogFrequency = logFrequency.next();
                updateCoefficients (nextLogFrequency, logQ.next());
            }

            const auto wetGain = wet.next();
            const auto dryGain = dry.next();
            const auto outputGain = output.next();
            const auto mono = monoAmount.next();
            const auto clip = softClipAmount.next();
            const auto bypass = bypassAmount.next();

            const auto mid = numChannels > 1 ? 0.5 * ((double) channels[0][i] + (double) channels[1][i])
                                             : (double) channels[0][i];

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto in = (double) channels[ch][i];

                // The filters keep running under bypass, so they are ready when it ends.
                const auto wetOut = subsonic[ch].process (svf[ch].tick (coefficients, in + (mid - in) * mono).band * coefficients.k);

                if (bypass >= 1.0)
                    continue;

                auto out = (in * dryGain + wetOut * wetGain) * outputGain;

                if (clip > 0.0)
                    out += (softClip (out) - out) * clip;

                channels[ch][i] = (float) (out + (in - out) * bypass);
            }
        }

        guardAgainstBadInput();
    }

    static double softClip (double x) noexcept
    {
        constexpr double knee = 0.7;
        const auto magnitude = std::abs (x);

        if (magnitude <= knee)
            return x;

        return std::copysign (knee + (1.0 - knee) * std::tanh ((magnitude - knee) / (1.0 - knee)), x);
    }

private:
    template <typename Assign>
    void applyTargets (const Targets& targets, Assign assign)
    {
        const auto nyquistLimit = 0.45 * sampleRate;

        assign (logFrequency, std::log (std::min (targets.frequencyHz, nyquistLimit)));
        assign (logQ, std::log (targets.q));
        assign (wet, targets.wetGain);
        assign (dry, targets.dryGain);
        assign (output, targets.outputGain);
        assign (monoAmount, targets.monoWet ? 1.0 : 0.0);
        assign (softClipAmount, targets.softClip ? 1.0 : 0.0);
        assign (bypassAmount, targets.bypass ? 1.0 : 0.0);
    }

    void updateCoefficients (double logHz, double logOfQ) noexcept
    {
        coefficients.set (std::exp (logHz), std::exp (logOfQ), sampleRate);
    }

    void guardAgainstBadInput() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (! svf[ch].isFinite() || ! subsonic[ch].isFinite())
            {
                svf[ch].reset();
                subsonic[ch].reset();
            }
        }
    }

    double sampleRate = 48000.0;

    fancy::dsp::SvfCoefficients coefficients;
    fancy::dsp::Svf svf[2];
    fancy::dsp::Biquad subsonic[2];

    fancy::dsp::Smoother logFrequency, logQ, wet, dry, output, monoAmount, softClipAmount, bypassAmount;
};
