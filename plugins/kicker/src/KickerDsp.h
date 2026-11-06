#pragma once

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Smoother.h>

#include <algorithm>
#include <cmath>
#include <numbers>

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
        for (auto& state : svf)
            state = {};

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
                const auto wetOut = subsonic[ch].process (tick (svf[ch], in + (mid - in) * mono));

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
    struct SvfState
    {
        double ic1 = 0.0, ic2 = 0.0;
    };

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
        const auto g = std::tan (std::numbers::pi * std::exp (logHz) / sampleRate);

        k = std::exp (-logOfQ);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // Returns the constant-peak band-pass output.
    double tick (SvfState& state, double in) const noexcept
    {
        const auto v3 = in - state.ic2;
        const auto v1 = a1 * state.ic1 + a2 * v3;
        const auto v2 = state.ic2 + a2 * state.ic1 + a3 * v3;

        state.ic1 = 2.0 * v1 - state.ic1;
        state.ic2 = 2.0 * v2 - state.ic2;

        return k * v1;
    }

    void guardAgainstBadInput() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            if (! std::isfinite (svf[ch].ic1) || ! std::isfinite (svf[ch].ic2) || ! subsonic[ch].isFinite())
            {
                svf[ch] = {};
                subsonic[ch].reset();
            }
        }
    }

    double sampleRate = 48000.0;
    double k = 0.5, a1 = 1.0, a2 = 0.0, a3 = 0.0;

    SvfState svf[2];
    fancy::dsp::Biquad subsonic[2];

    fancy::dsp::Smoother logFrequency, logQ, wet, dry, output, monoAmount, softClipAmount, bypassAmount;
};
