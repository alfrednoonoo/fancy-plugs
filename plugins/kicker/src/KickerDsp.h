#pragma once

#include "SubGenerator.h"

#include <fancy/dsp/Biquad.h>
#include <fancy/dsp/Smoother.h>
#include <fancy/dsp/Svf.h>

#include <algorithm>
#include <array>
#include <atomic>
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

        // Phase 4 Wave 1 (docs/kicker.md). The defaults leave Sub mode as it was, to the bit.
        double subLengthMs = 0.0;     // 0 is Off: the sub lasts as long as the kick
        bool roomCut = false;         // Make-room: a high-pass on the source kick
        double roomHz = 60.0;
        bool subPolarity = false;     // inverts the generated sub
    };

    // Phase 4 Wave 1: the generated sub's peak level since the last call, for its meter.
    float takeSubPeak() noexcept { return subPeak.value.exchange (0.0f, std::memory_order_relaxed); }

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

        roomAmount.setTime (20.0, sampleRate);
        polarity.setTime (20.0, sampleRate);
        roomAmount.snapTo (0.0);
        polarity.snapTo (1.0);
        onsetSlowCoefficient = 1.0 - std::exp (-1.0 / (0.03 * sampleRate));
        lengthFadeSamples = std::max (1, (int) std::lround (0.02 * sampleRate));

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

        for (auto& filter : roomFilters)
            filter.reset();

        lengthCount.fill (0);
        onsetSlow.fill (0.0);
        wasRunning.fill (false);
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

    // Phase 4 (docs/kicker.md, sidechain Duck and the oversampled clipper). With `deferred`,
    // process() leaves each output sample before its soft clip and bypass mix, and records the
    // sample's clip and bypass amounts there, so the caller can clip it oversampled and mix the
    // bypass with a delayed input. With neither argument, it's Kicker as it was, to the bit.
    struct Deferred
    {
        float* clip = nullptr;
        float* bypass = nullptr;
    };

    // `duckGain`, if given, is a gain per sample on the whole output, before the soft clip.
    void process (float* const* channels, int numChannels, int numSamples, const float* duckGain = nullptr, const Deferred* deferred = nullptr)
    {
        if (modeAmount.isSettled() && modeAmount.getCurrent() == 0.0)
            processBoost (channels, numChannels, numSamples, duckGain, deferred);
        else
            processWithSub (channels, numChannels, numSamples, duckGain, deferred);

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
    void processBoost (float* const* channels, int numChannels, int numSamples, const float* duckGain, const Deferred* deferred)
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

            if (deferred != nullptr)
            {
                deferred->clip[i] = (float) clip;
                deferred->bypass[i] = (float) bypass;
            }

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

                if (duckGain != nullptr)
                    out *= (double) duckGain[i];

                if (deferred != nullptr)
                {
                    channels[ch][i] = (float) out;
                    continue;
                }

                if (clip > 0.0)
                    out += (softClip (out) - out) * clip;

                channels[ch][i] = (float) (out + (in - out) * bypass);
            }
        }
    }

    // Phase 4 Wave 1: Sub length. A new hit is the generator starting, or its level jumping
    // over its own recent average; from there the sub plays for Sub length, then fades over 20 ms
    // (60 dB, in dB steps) to silence. Off, it's always 1.
    double lengthGain (int g) noexcept
    {
        if (lengthSamples == 0)
            return 1.0;

        auto& generator = generators[(size_t) g];
        const auto envelope = generator.getEnvelope();
        const auto running = generator.isRunning();
        auto& slow = onsetSlow[(size_t) g];

        if ((running && ! wasRunning[(size_t) g]) || (envelope > 1.6 * slow && lengthCount[(size_t) g] > lengthFadeSamples))
            lengthCount[(size_t) g] = 0;

        wasRunning[(size_t) g] = running;
        slow += (envelope - slow) * onsetSlowCoefficient;

        const auto count = lengthCount[(size_t) g]++;

        if (count < lengthSamples)
            return 1.0;

        const auto fade = count - lengthSamples;

        if (fade >= lengthFadeSamples)
            return 0.0;

        return std::pow (10.0, -3.0 * (double) fade / (double) lengthFadeSamples);
    }

    // Sub mode, or a crossfade between the modes.
    void processWithSub (float* const* channels, int numChannels, int numSamples, const float* duckGain, const Deferred* deferred)
    {
        auto peakHere = 0.0;

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

            if (deferred != nullptr)
            {
                deferred->clip[i] = (float) clip;
                deferred->bypass[i] = (float) bypass;
            }

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
                const auto shared = generators[0].process (mid) * lengthGain (0);
                sub[0] = sub[1] = shared * subMono;
            }

            if (subMono < 1.0)
                for (int ch = 0; ch < numChannels; ++ch)
                    sub[(size_t) ch] += generators[(size_t) ch + 1].process ((double) channels[ch][i]) * lengthGain (ch + 1) * (1.0 - subMono);

            // Phase 4 Wave 1: polarity, and the sub meter.
            const auto flip = polarity.isSettled() && polarity.getCurrent() == 1.0 ? 1.0 : polarity.next();

            if (flip != 1.0)
                for (auto& value : sub)
                    value *= flip;

            peakHere = std::max (peakHere, std::max (std::abs (sub[0]), std::abs (sub[1])) * subGain * mode);

            const auto room = roomAmount.isSettled() && roomAmount.getCurrent() == 0.0 ? 0.0 : roomAmount.next();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto in = (double) channels[ch][i];
                const auto wetOut = subsonic[ch].process (svf[ch].tick (coefficients, in + (mid - in) * mono).band * coefficients.k);

                if (bypass >= 1.0)
                    continue;

                const auto boost = in * dryGain + wetOut * wetGain;

                // Make-room: the source kick through a high-pass in Sub mode, so the sub has the
                // low end to itself. The filter runs only while it's in use.
                auto source = in;

                if (room > 0.0)
                    source += (roomFilters[ch].tick (roomCoefficients, in).high - in) * room;

                const auto withSub = source * dryGain + sub[(size_t) ch] * subGain;
                auto out = (boost + (withSub - boost) * mode) * outputGain;

                if (duckGain != nullptr)
                    out *= (double) duckGain[i];

                if (deferred != nullptr)
                {
                    channels[ch][i] = (float) out;
                    continue;
                }

                if (clip > 0.0)
                    out += (softClip (out) - out) * clip;

                channels[ch][i] = (float) (out + (in - out) * bypass);
            }
        }

        auto shown = subPeak.value.load (std::memory_order_relaxed);

        while ((float) peakHere > shown && ! subPeak.value.compare_exchange_weak (shown, (float) peakHere, std::memory_order_relaxed))
        {
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

        // Phase 4 Wave 1.
        assign (roomAmount, targets.roomCut ? 1.0 : 0.0);
        assign (polarity, targets.subPolarity ? -1.0 : 1.0);
        lengthSamples = targets.subLengthMs > 0.0 ? std::max (1, (int) std::lround (targets.subLengthMs * 0.001 * sampleRate)) : 0;

        if (targets.roomHz != roomHz || ! roomReady)
        {
            roomHz = targets.roomHz;
            roomCoefficients.set (std::min (roomHz, 0.45 * sampleRate), 0.9, sampleRate);
            roomReady = true;
        }

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

    // Phase 4 Wave 1.
    fancy::dsp::Smoother roomAmount, polarity;
    fancy::dsp::SvfCoefficients roomCoefficients;
    fancy::dsp::Svf roomFilters[2];
    double roomHz = 60.0;
    bool roomReady = false;
    int lengthSamples = 0, lengthFadeSamples = 960;
    std::array<int, 3> lengthCount {};
    std::array<double, 3> onsetSlow {};
    std::array<bool, 3> wasRunning {};
    double onsetSlowCoefficient = 0.001;
    // The sub meter's peak for the UI. Wrapped so the engine stays copyable (the tests build
    // one by value); a copy just takes the current reading.
    struct PeakCell
    {
        std::atomic<float> value { 0.0f };
        PeakCell() = default;
        PeakCell (const PeakCell& other) noexcept : value (other.value.load (std::memory_order_relaxed)) {}
        PeakCell& operator= (const PeakCell& other) noexcept { value.store (other.value.load (std::memory_order_relaxed), std::memory_order_relaxed); return *this; }
    };

    PeakCell subPeak;
};
