#include <fancy/ui/ShellEditor.h>

#include <algorithm>
#include <iterator>

namespace fancy::ui
{

namespace
{
const juce::Identifier uiScaleProperty ("uiScale");
constexpr int timerHz = 30;
constexpr float scales[] { 1.0f, 1.25f, 1.5f, 2.0f };
constexpr int scaleItemBase = 1000;
constexpr int aboutItem = 2000;

const char* const attribution =
    "UI design based on 'ReaPlugs (vst GUI)' by Eugene Reznik, CC BY 4.0 - "
    "https://www.figma.com/community/file/967866927752067729. Modified from the original.";
} // namespace

// EditLink --------------------------------------------------------------------

EditLink::EditLink (juce::Colour accent)
    : accentColour (accent)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void EditLink::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (accentColour);
    g.fillPath (icons::gear (bounds.withWidth (11.0f).withSizeKeepingCentre (11.0f, 11.0f)));

    g.setFont (font (Weight::regular, 12.0f));
    g.drawText ("EDIT", bounds.withTrimmedLeft (15.0f), juce::Justification::centredLeft, false);
}

void EditLink::mouseUp (const juce::MouseEvent& event)
{
    if (onClick != nullptr && contains (event.getPosition()))
        onClick();
}

// EditDrawer ------------------------------------------------------------------

EditDrawer::EditDrawer()
{
    closeButton.onClick = [this] { setVisible (false); };
    addAndMakeVisible (closeButton);
}

void EditDrawer::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgSection);

    g.setFont (font (Weight::semiBold, 12.0f).withExtraKerningFactor (0.06f));
    g.setColour (colours::textPrimary);
    g.drawText ("EDIT", getLocalBounds().withHeight (32).withTrimmedLeft (16), juce::Justification::centredLeft, false);
}

void EditDrawer::resized()
{
    closeButton.setBounds (getWidth() - 56, 8, 44, 16);
}

// ShellEditor -----------------------------------------------------------------

void ShellEditor::Root::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgHeader);
}

ShellEditor::ShellEditor (FancyProcessor& processor, int baseWidth, int baseHeight, HeaderStyle style)
    : AudioProcessorEditor (processor),
      fancyProcessor (processor),
      width (baseWidth),
      height (baseHeight),
      headerStyle (style),
      saveButton (style == HeaderStyle::compact ? "+" : "+ New"),
      parameterButton (style == HeaderStyle::compact ? "Prm" : "Parameter"),
      ioPill (juce::String {}),
      outputKnob (*processor.getApvts().getParameter ("output"), juce::String {}, colours::uiTeal, Knob::headerStyle),
      powerPill (*processor.getApvts().getParameter ("bypass"))
{
    setLookAndFeel (&lookAndFeel.get());

    addAndMakeVisible (root);
    root.addAndMakeVisible (body);

    for (juce::Component* item : std::initializer_list<juce::Component*> { &presetDropdown, &saveButton, &parameterButton, &ioPill, &outputKnob, &powerPill })
        root.addAndMakeVisible (item);

    presetDropdown.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { showSavePrompt(); };
    parameterButton.onClick = [this] { showParameterMenu(); };

    const auto savedScale = (float) (double) processor.getApvts().state.getProperty (uiScaleProperty, 1.0);
    applyScale (std::find (std::begin (scales), std::end (scales), savedScale) != std::end (scales) ? savedScale : 1.0f);

    timerCallback();
    startTimerHz (timerHz);
}

ShellEditor::~ShellEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::RangedAudioParameter& ShellEditor::parameter (const juce::String& id)
{
    return *fancyProcessor.getApvts().getParameter (id);
}

void ShellEditor::applyScale (float scale)
{
    uiScale = scale;
    root.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt ((float) width * scale), juce::roundToInt ((float) height * scale));
}

void ShellEditor::resized()
{
    root.setBounds (0, 0, width, height);
    body.setBounds (0, headerHeight, width, height - headerHeight);
    layoutHeader();
}

void ShellEditor::layoutHeader()
{
    const auto compact = headerStyle == HeaderStyle::compact;
    constexpr int top = 8, itemHeight = 17, gap = 5;

    int x = compact ? 11 : 12;

    const auto place = [&] (juce::Component& item, int itemWidth)
    {
        item.setBounds (x, top, itemWidth, itemHeight);
        x += itemWidth + gap;
    };

    place (presetDropdown, compact ? 74 : 120);
    place (saveButton, compact ? 18 : 42);
    place (parameterButton, compact ? 29 : 62);
    place (ioPill, compact ? 27 : 58);

    powerPill.setBounds (width - 43, top, 32, itemHeight);
    outputKnob.setBounds (width - 72, 4, 24, 24);
}

void ShellEditor::timerCallback()
{
    const auto bypassed = fancyProcessor.getBypassParameter()->getValue() >= 0.5f;
    body.setAlpha (bypassed ? 0.5f : 1.0f);

    presetDropdown.setText (fancyProcessor.getPresets().getCurrentName());

    const auto inputs = juce::String (fancyProcessor.getMainBusNumInputChannels());
    const auto outputs = juce::String (fancyProcessor.getMainBusNumOutputChannels());

    ioPill.setText (headerStyle == HeaderStyle::compact ? inputs + "/" + outputs
                                                        : inputs + " in " + outputs + " out");

    onTick (1.0f / (float) timerHz);
}

void ShellEditor::showPresetMenu()
{
    auto& presets = fancyProcessor.getPresets();
    const auto names = presets.getNames();

    juce::PopupMenu menu;

    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, names[i] == presets.getCurrentName());

    menu.showMenuAsync (juce::PopupMenu::Options {}.withTargetComponent (presetDropdown),
                        [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (int result)
                        {
                            if (safeThis != nullptr && result > 0)
                                safeThis->fancyProcessor.getPresets().load (result - 1);
                        });
}

void ShellEditor::showSavePrompt()
{
    auto* window = new juce::AlertWindow ("Save preset", "Preset name", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", fancyProcessor.getPresets().getCurrentName());
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([safeThis = juce::Component::SafePointer<ShellEditor> (this), window] (int result)
                             {
                                 if (safeThis != nullptr && result == 1)
                                     safeThis->fancyProcessor.getPresets().saveUser (window->getTextEditorContents ("name"));
                             }),
                             true);
}

void ShellEditor::showParameterMenu()
{
    juce::PopupMenu menu;

    const auto& parameters = fancyProcessor.getParameters();

    for (int i = 0; i < parameters.size(); ++i)
    {
        const auto* item = parameters[i];
        menu.addItem (i + 1, item->getName (64) + ":  " + item->getCurrentValueAsText(), getHostContext() != nullptr);
    }

    menu.addSeparator();

    juce::PopupMenu sizes;

    for (int i = 0; i < (int) std::size (scales); ++i)
        sizes.addItem (scaleItemBase + i, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true, scales[i] == uiScale);

    menu.addSubMenu ("UI size", sizes);
    menu.addItem (aboutItem, "About");

    menu.showMenuAsync (juce::PopupMenu::Options {}.withTargetComponent (parameterButton),
                        [safeThis = juce::Component::SafePointer<ShellEditor> (this)] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            auto& self = *safeThis;

                            if (result == aboutItem)
                            {
                                self.showAbout();
                            }
                            else if (result >= scaleItemBase)
                            {
                                const auto scale = scales[result - scaleItemBase];
                                self.fancyProcessor.getApvts().state.setProperty (uiScaleProperty, scale, nullptr);
                                self.applyScale (scale);
                            }
                            else if (auto* context = self.getHostContext())
                            {
                                if (const auto hostMenu = context->getContextMenuForParameter (self.fancyProcessor.getParameters()[result - 1]))
                                    hostMenu->showNativeMenu (self.getMouseXYRelative());
                            }
                        });
}

void ShellEditor::showAbout()
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions {}
                                      .withIconType (juce::MessageBoxIconType::NoIcon)
                                      .withTitle (fancyProcessor.getName())
                                      .withMessage (attribution)
                                      .withButton ("OK")
                                      .withAssociatedComponent (this),
                                  static_cast<juce::ModalComponentManager::Callback*> (nullptr));
}

} // namespace fancy::ui
