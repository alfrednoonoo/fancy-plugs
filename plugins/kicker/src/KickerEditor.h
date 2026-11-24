#pragma once

#include "KickerProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

#include <functional>
#include <vector>

class KickerEditor final : public fancy::ui::ShellEditor
{
public:
    explicit KickerEditor (KickerProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup levels, optionally with the EDIT drawer open, in Boost or
    // in Sub mode (docs/kicker-sub.md).
    void showPreview (bool drawerOpen, bool subMode = false);

    // Phase 4 (mockup 63): the drawer's pages, Main and Duck and clip.
    enum DrawerPage { mainPage, duckPage };
    void showDrawerPage (int page);

    // For rendered screenshots: mockup 63's values, on the main window (panel 1, with the DUCK
    // tag) or on the drawer's Duck and clip page (panel 2).
    void showDuckPreview (bool drawerOpen);

private:
    void onTick (float seconds) override;
    void updateMode();
    bool isHostSyncChosen();

    fancy::ui::CaptionPanels panels;
    fancy::ui::ParamSegments modeSegments;

    // Boost.
    fancy::ui::Knob freqKnob, wetKnob;

    // Sub.
    fancy::ui::Knob cutoffKnob, subKnob;
    fancy::ui::ParamSegments octaveSegments;

    // Both.
    fancy::ui::Knob dryKnob;
    fancy::ui::EditLink editLink;
    fancy::ui::InOutMeter meter;

    fancy::ui::EditDrawer drawer { juce::String() }; // the page switch takes the title's place
    fancy::ui::Knob qKnob, drawerOutputKnob, sensitivityKnob, subLowpassKnob;
    fancy::ui::ParamCheckbox monoCheckbox, softClipCheckbox, subMonoCheckbox;

    // Phase 4 Wave 1: the Sub group, Auto gain and Ceiling in the drawer, and the sub meter.
    class SubMeter final : public juce::Component
    {
    public:
        void update (float peak, float seconds);
        void paint (juce::Graphics&) override;

    private:
        float shownDb = -100.0f;
    };

    fancy::ui::Knob subLengthKnob, roomFreqKnob, ceilingKnob;
    fancy::ui::ParamCheckbox roomCheckbox, polarityCheckbox, autoGainCheckbox;
    SubMeter subMeter;

    // Phase 4 (mockup 63): the Duck and clip page, and the DUCK tag beside EDIT while Duck is up.
    class DuckTag final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        DuckTag();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::function<void()> onClick;
    };

    fancy::ui::ViewTabs drawerTabs { juce::StringArray { "Main", "Duck and clip" } };
    fancy::ui::Knob duckKnob, duckReleaseKnob, duckShapeKnob;
    fancy::ui::ParamDropdown duckFromDropdown, duckRateDropdown, clipOsDropdown;
    juce::Label duckTitle, duckFromLabel, syncTitle, rateLabel, clipTitle, clipOsLabel, clipNote;
    DuckTag duckTag;
    std::vector<juce::Component*> mainItems, duckItems, syncItems;
    int shownPage = mainPage;

    int shownMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickerEditor)
};
