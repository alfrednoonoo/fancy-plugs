#pragma once

#include "ExciterProcessor.h"

#include <fancy/ui/Meter.h>
#include <fancy/ui/ShellEditor.h>

class ExciterEditor final : public fancy::ui::ShellEditor
{
public:
    explicit ExciterEditor (ExciterProcessor& processor);

    juce::Colour getAccentColour() override;
    std::vector<PopupOpener> getPopupOpeners() override;

    // For rendered screenshots: mockup values, optionally with the EDIT drawer open.
    void showPreview (bool drawerOpen);

    // Phase 4 Wave 2 (mockup 62): the drawer's pages, Main and Harmonics.
    enum DrawerPage { mainPage, harmonicsPage };
    void showDrawerPage (int page);

    // For rendered screenshots: the Harmonics page at mockup 62's values (panel 2).
    void showHarmonicsPreview();

private:
    void onTick (float seconds) override;

    fancy::ui::CaptionPanels panels;
    fancy::ui::Knob freqKnob, clipBoostKnob, harmonicsKnob, mixBackKnob;
    fancy::ui::EditLink editLink;
    fancy::ui::InOutMeter meter;

    // Phase 4 Wave 1: the listen switch and the THD read-out; Ceiling and Auto gain in the drawer.
    fancy::ui::ListenCheckbox listenCheckbox;
    juce::Label thdLabel;

    fancy::ui::EditDrawer drawer { juce::String() }; // the page switch takes the title's place
    fancy::ui::Knob characterKnob, drawerOutputKnob;
    fancy::ui::Knob ceilingKnob;
    fancy::ui::ParamCheckbox autoGainCheckbox;
    fancy::ui::ParamDropdown oversamplingDropdown;
    juce::Label oversamplingLabel;

    // Phase 4 Wave 2: the Harmonics page.
    fancy::ui::ViewTabs drawerTabs { juce::StringArray { "Main", "Harmonics" } };
    fancy::ui::Knob freq2Knob, harmonics2Knob, punchKnob, highCutKnob, widthKnob;
    juce::Label band2Title, band2Note, shapeTitle, shapeNote;
    std::vector<juce::Component*> mainItems, harmonicsItems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterEditor)
};
