#pragma once

#include <fancy/dsp/DeEsser.h>
#include <fancy/plugin/FancyProcessor.h>

#include <array>
#include <atomic>

// Fancy DS-Mini (docs/ds-mini.md): a profile and Amount, mapped onto Fancy DS's engine.
class DsMiniProcessor final : public fancy::FancyProcessor
{
public:
    using DeEsser = fancy::dsp::DeEsser;

    struct Profile
    {
        const char* name;
        bool shelf;
        float freqHz, widthOctaves, attackMs, releaseMs;
        const char* caption;
    };

    static const std::array<Profile, 6> profiles;

    // Profile changes crossfade between two engines over this long.
    static constexpr double fadeMs = 50.0;

    // The folder in Fancy DS's user presets that "Save as Fancy DS preset" writes to.
    static constexpr const char* dsPresetFolder = "From DS-Mini";

    // Fancy DS's settings for a profile and Amount, as the values Fancy DS's parameters would
    // be set to. Amount sets Threshold (-4 dB per step) and Range (-1.8 dB per step).
    struct DsSettings
    {
        float threshold, freq, width, range, attack, release;
        bool shelf;
    };

    static DsSettings mapProfile (int profile, float amount);

    DsMiniProcessor();
    ~DsMiniProcessor() override;

    void prepareAudio (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;

    int getProfile() const;

    // The engine's settings for a profile, at the current Amount and EDIT settings, rounded as
    // Fancy DS's parameters round them, so the two plugins are bit-identical.
    DeEsser::Targets targetsFor (int profile) const;

    // The engine the editor reads: the one playing the current profile.
    DeEsser& getEngine() noexcept { return engines[(size_t) active.load (std::memory_order_acquire)]; }

    // Both engines, so the history keeps its time scale after a profile change.
    void setHistoryColumns (int columns) noexcept;

    // The reduction since the last call, from both engines.
    float takeReduction() noexcept;

    // A Fancy DS preset with the current settings, and writing it to Fancy DS's user presets.
    juce::ValueTree createDsPresetState() const;
    juce::File getDsPresetFile (const juce::String& presetName) const;
    bool saveAsDsPreset (const juce::String& presetName);

    // Message thread.
    void setParameter (const juce::String& parameterId, float value);

private:
    float read (const char* parameterId) const;
    bool isOn (const char* parameterId) const;

    void startFade (int profile);

    std::array<DeEsser, 2> engines;
    std::array<int, 2> engineProfiles { 0, 0 };
    std::atomic<int> active { 0 };
    int fadeLeft = 0, fadeLength = 1;

    // The input, for the engine that's fading out.
    juce::AudioBuffer<float> scratch;
    static constexpr int scratchSize = 512;

    // Private copies of Fancy DS's parameters, only for rounding.
    std::unique_ptr<juce::AudioParameterFloat> dsThreshold, dsFreq, dsWidth, dsRange, dsAttack, dsRelease;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsMiniProcessor)
};
