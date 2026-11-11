#pragma once

#include "SubGenerator.h"

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <array>
#include <cmath>

// See docs/kicker.md and, for Sub mode, docs/kicker-sub.md. Contains no JUCE types, so tests can
// drive it directly.
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

        // Sub mode.
        bool sub = false;
        double subCutoffHz = 100.0;
        int subDivisions = 2;
        double subGain = 0.35481338923357547; // -9 dB
        double subSensitivityGain = 0.01;     // -40 dB
        double subLowpassHz = 120.0;
        bool subMono = true;
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

        modeAmount.setTime (20.0, sampleRate);
        subLevel.setTime (20.0, sampleRate);
        subLogCutoff.setTime (30.0, sampleRate);
        subLogLowpass.setTime (30.0, sampleRate);
        subMonoAmount.setTime (20.0, sampleRate);

        for (auto& filter : subsonic)
            filter.setButterworthHighPass (15.0, sampleRate);

        for (auto& generator : generators)
            generator.prepare (sampleRate);

        reset();
    }

    void reset()
    {
        for (auto& filter : svf)
            filter.reset();

        for (auto& filter : subsonic)
            filter.reset();

        for (auto& generator : generators)
            generator.reset();
    }

    void setTargets (const Targets& targets)
    {
        // Sub's generators start afresh when they come back into use.
        if (targets.sub && modeAmount.isSettled() && modeAmount.getCurrent() == 0.0)
            for (auto& generator : generators)
                generator.reset();

        // Switching Mono sub fades from one set of generators to the other; the incoming set
        // starts afresh rather than from wherever it stopped.
        if (targets.subMono != subMonoOn)
        {
            if (targets.subMono)
                generators[0].reset();
            else
                generators[1].reset(), generators[2].reset();

            subMonoOn = targets.subMono;
        }

        applyTargets (targets, [] (fancy::dsp::Smoother& smoother, double value) { smoother.setTarget (value); });
    }

    // Jumps straight to the targets, for use before processing starts.
    void snapToTargets (const Targets& targets)
    {
        subMonoOn = targets.subMono;
        applyTargets (targets, [] (fancy::dsp::Smoother& smoother, double value) { smoother.snapTo (value); });
        updateCoefficients (logFrequency.getCurrent(), logQ.getCurrent());
        updateSubSettings();
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        if (modeAmount.isSettled() && modeAmount.getCurrent() == 0.0)
            processBoost (channels, numChannels, numSamples);
        else
            processWithSub (channels, numChannels, numSamples);

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

    // For tests: the generator used with Mono sub on.
    const SubGenerator& getMonoGenerator() const noexcept { return generators[0]; }

private:
    // Boost mode, settled: exactly the v1.2.0 code, so it stays bit-identical (docs/kicker-sub.md).
    void processBoost (float* const* channels, int numChannels, int numSamples)
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

            advanceSubSmoothers();

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
    }

    // Sub mode, or a crossfade between the modes.
    void processWithSub (float* const* channels, int numChannels, int numSamples)
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
            const auto mode = modeAmount.next();
            const auto subGain = subLevel.next();
            const auto subMono = subMonoAmount.next();

            if (! subLogCutoff.isSettled() || ! subLogLowpass.isSettled())
            {
                subLogCutoff.next();
                subLogLowpass.next();
                updateSubSettings();
            }

            const auto mid = numChannels > 1 ? 0.5 * ((double) channels[0][i] + (double) channels[1][i])
                                             : (double) channels[0][i];

            // The sub: one generator from L+R with Mono sub on, one per channel with it off.
            std::array<double, 2> sub {};

            if (subMono > 0.0)
            {
                const auto shared = generators[0].process (mid);
                sub[0] = sub[1] = shared * subMono;
            }

            if (subMono < 1.0)
                for (int ch = 0; ch < numChannels; ++ch)
                    sub[(size_t) ch] += generators[(size_t) ch + 1].process ((double) channels[ch][i]) * (1.0 - subMono);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto in = (double) channels[ch][i];
                const auto wetOut = subsonic[ch].process (svf[ch].tick (coefficients, in + (mid - in) * mono).band * coefficients.k);

                if (bypass >= 1.0)
                    continue;

                const auto boost = in * dryGain + wetOut * wetGain;
                const auto withSub = in * dryGain + sub[(size_t) ch] * subGain;
                auto out = (boost + (withSub - boost) * mode) * outputGain;

                if (clip > 0.0)
                    out += (softClip (out) - out) * clip;

                channels[ch][i] = (float) (out + (in - out) * bypass);
            }
        }
    }

    // Sub's smoothers move in Boost too, so its settings are current when it's switched on.
    void advanceSubSmoothers() noexcept
    {
        modeAmount.next();
        subLevel.next();
        subMonoAmount.next();

        if (! subLogCutoff.isSettled() || ! subLogLowpass.isSettled())
        {
            subLogCutoff.next();
            subLogLowpass.next();
            updateSubSettings();
        }
    }

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

        assign (modeAmount, targets.sub ? 1.0 : 0.0);
        assign (subLevel, targets.subGain);
        assign (subLogCutoff, std::log (std::min (targets.subCutoffHz, nyquistLimit)));
        assign (subLogLowpass, std::log (std::min (targets.subLowpassHz, nyquistLimit)));
        assign (subMonoAmount, targets.subMono ? 1.0 : 0.0);

        subDivisions = targets.subDivisions;
        subSensitivity = targets.subSensitivityGain;

        if (generatorsReady)
            updateSubSettings();
    }

    void updateSubSettings() noexcept
    {
        SubGenerator::Settings settings;
        settings.cutoffHz = std::exp (subLogCutoff.getCurrent());
        settings.lowpassHz = std::exp (subLogLowpass.getCurrent());
        settings.divisions = subDivisions;
        settings.sensitivityGain = subSensitivity;

        for (auto& generator : generators)
            generator.setSettings (settings);

        generatorsReady = true;
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

    // Sub mode: generators[0] for Mono sub, [1] and [2] for each channel.
    std::array<SubGenerator, 3> generators;
    fancy::dsp::Smoother modeAmount, subLevel, subLogCutoff, subLogLowpass, subMonoAmount;
    int subDivisions = 2;
    double subSensitivity = 0.01;
    bool generatorsReady = false;
    bool subMonoOn = true;
};
