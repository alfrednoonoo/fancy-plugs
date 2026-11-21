#pragma once

#include "CompGraphs.h"
#include "CompProcessor.h"

#include <fancy/ui/LevelHistory.h>
#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

class CompEditor final : public fancy::ui::ShellEditor
{
public:
    explicit CompEditor (CompProcessor& processor);

    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // The drawer open on one tab (DrawerTabs::Tab), with the Q2.5 mockup's values.
    void showDrawerPreview (int tab);

    // Phase 4 Wave 2: the Options page with Host sync chosen (reference/phase4/43__comp_w2_mockup.png).
    void showSyncPreview();

private:
    class Panels final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void onTick (float seconds) override;
    void showTab (int tab);
    void updateSyncGroup(); // Phase 4 Wave 2
    void updateDimming();
    void updateHistoryCaption();
    CompParameters makeParameters();

    Panels panels;

    juce::Label detectorLabel;
    fancy::ui::ParamDropdown detectorDropdown;
    fancy::ui::EditLink editLink;

    fancy::ui::ThresholdMeter thresholdMeter;
    fancy::ui::GainReductionMeter gainReductionMeter;

    fancy::ui::Knob ratioKnob, kneeKnob, attackKnob, releaseKnob;
    fancy::ui::ParamCheckbox weirdCheckbox, classicCheckbox, autoCheckbox;
    fancy::ui::Knob preCompKnob, rmsKnob, lowpassKnob, highpassKnob;
    fancy::ui::ParamCheckbox linkCheckbox, makeupCheckbox;
    fancy::ui::ListenCheckbox previewCheckbox, removedCheckbox;
    fancy::ui::ParamCheckbox limitCheckbox;

    fancy::ui::OutMeter outMeter;
    juce::Label wetLabel, dryLabel;
    fancy::ui::Fader wetFader, dryFader;

    fancy::ui::EditDrawer drawer;
    juce::Label oversamplingLabel, phaseLabel, releaseCurveLabel;
    fancy::ui::ParamDropdown oversamplingDropdown, phaseDropdown, releaseCurveDropdown;
    fancy::ui::ParamCheckbox feedbackCheckbox, attack90Checkbox;
    fancy::ui::Knob linkAmountKnob, drawerOutputKnob;

    // Phase 3 Q2.5: the drawer's tabs.
    DrawerTabs drawerTabs;

    DetectorCurve detectorCurve;
    juce::Label lowShelfLabel, bellLabel, highShelfLabel;
    fancy::ui::Knob scLowFreqKnob, scLowGainKnob, scMidFreqKnob, scMidGainKnob, scMidQKnob, scHighFreqKnob, scHighGainKnob;
    fancy::ui::ParamCheckbox autoThresholdCheckbox;
    juce::Label autoOffsetLabel;

    juce::Label typeLabel, positionLabel, characterNote;
    fancy::ui::ParamSegments typeSegments, positionSegments;
    fancy::ui::Knob driveKnob;
    CharacterCurve characterCurve;

    fancy::ui::ParamCheckbox upwardCheckbox;
    fancy::ui::Knob upThresholdKnob, upRatioKnob, maxLiftKnob, floorKnob;
    UpwardCurve upwardCurve;

    // Phase 4 Wave 1: Hold and Range on the Options page, and the History page.
    fancy::ui::Knob holdKnob, rangeKnob;

    // Phase 4 Wave 2: Host sync's group on the Options page, shown while it's the Detector input.
    juce::Label syncCaption, syncRateLabel, syncNote;
    fancy::ui::ParamDropdown syncRateDropdown;
    fancy::ui::Knob syncShapeKnob;
    int shownTab = 0;
    DrawerTabs zoomSwitch;
    fancy::ui::ToggleCheckbox gridCheckbox;
    HistoryCaption historyCaption;
    fancy::ui::LevelHistory levelHistory;
    std::vector<fancy::dsp::LevelColumn> historyBuffer;
    float shownPeak = -1.0f;

    std::vector<juce::Component*> tabContents[DrawerTabs::numTabs];
    juce::String graphSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompEditor)
};
