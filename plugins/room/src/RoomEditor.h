#pragma once

#include "RoomProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

class RoomEditor final : public fancy::ui::ShellEditor
{
public:
    explicit RoomEditor (RoomProcessor& processor);
    ~RoomEditor() override;

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: the mockup's values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    // §3.2: the energy-decay view, worked out from the settings rather than measured, so it's
    // always readable.
    class DecayDisplay final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit DecayDisplay (RoomProcessor& processor);
        void update();
        void paint (juce::Graphics&) override;

    private:
        struct Shown
        {
            double predelayMs = -1.0, size = -1.0, decay = -1.0, damping = -1.0, erGain = -1.0;
            bool freeze = false;
            bool operator== (const Shown&) const = default;
        };

        Shown read() const;

        RoomProcessor& roomProcessor;
        Shown shown;
    };

    void onTick (float seconds) override;
    void updateStates();

    RoomProcessor& roomProcessor() noexcept;

    Panels panels;
    DecayDisplay display;

    fancy::ui::Knob predelayKnob, noteKnob, sizeKnob, decayKnob, diffusionKnob, modKnob, widthKnob;
    fancy::ui::Knob dampingKnob, lowCutKnob, highCutKnob;

    fancy::ui::ParamCheckbox syncCheckbox, freezeCheckbox, wetOnlyCheckbox;
    fancy::ui::EditLink editLink;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;

    fancy::ui::EditDrawer drawer;
    juce::Label qualityLabel, stereoLabel;
    fancy::ui::ParamDropdown qualityDropdown, stereoDropdown;
    fancy::ui::Knob duckingKnob, erKnob;

    // Phase 4 Wave 1 (reference/phase4/37__room_w1_mockup.png): the Reverb EQ and Mod rate, the
    // gate and Auto gain, all in the drawer.
    juce::Label eqTitle, gateTitle, autoGainNote;
    fancy::ui::Knob eqLowKnob, eqLowGainKnob, eqMidKnob, eqMidGainKnob, eqQKnob, eqHighKnob, eqHighGainKnob, modRateKnob;
    fancy::ui::ParamCheckbox gateCheckbox, autoGainCheckbox;
    fancy::ui::Knob gateThresholdKnob, gateHoldKnob, gateDecayKnob;

    // Phase 4 Wave 2 (reference/phase4/56__room_w2_mockup.png): Duck from with the temporary
    // Listen to ducking switch beside Ducking, and the Decay shape group under Auto gain.
    juce::Label duckFromTitle, shapeTitle;
    fancy::ui::ParamDropdown duckFromDropdown;
    fancy::ui::ListenCheckbox listenCheckbox;
    fancy::ui::Knob lowDecayKnob, attackKnob;

    // The I/O pill counts the sidechain's two pins.
    int getShownInputs() override
    {
        return juce::jmax (getFancyProcessor().getTotalNumInputChannels(), getFancyProcessor().getMainBusNumInputChannels() + 2);
    }

    bool lastCapped = false;
    juce::String lastNoteTip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomEditor)
};
