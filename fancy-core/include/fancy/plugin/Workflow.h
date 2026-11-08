#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <set>
#include <tuple>
#include <vector>

namespace fancy
{

class FancyProcessor;

// A plugin's settings: every parameter's normalised value, except bypass, and the state's
// children that aren't parameters, such as Verb's chain or FIR's curves and noise profile.
// UI properties and the preset name aren't settings, so they aren't here (Phase 2.1 W).
struct Snapshot
{
    std::vector<float> values;   // by parameter index; NaN for bypass
    juce::ValueTree extra;       // its children are the state's non-parameter children

    bool isValid() const noexcept { return ! values.empty(); }
    bool sameAs (const Snapshot& other) const;
};

// Undo and redo, and the A/B slots, for one plugin instance. Message thread only. Owned by
// FancyProcessor, so it lives as long as the plugin, and isn't saved with the project.
//
// A user edit is a parameter change inside a change gesture (only the plugin's own editor
// starts gestures, never a host), any parameter change inside a UserEdit scope, or a change to
// the state outside the parameters, counted by FancyProcessor::getEditCounter(). Edits are
// made into one step by commit(), which the editor calls 30 times a second once no gesture or
// scope is open and no mouse button is down, so one drag or one click is one step.
class Workflow final : private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr int maxSteps = 100;

    explicit Workflow (FancyProcessor& processor);
    ~Workflow() override;

    // Undo ---------------------------------------------------------------------------------

    bool canUndo() const noexcept { return ! undoSteps.empty(); }
    bool canRedo() const noexcept { return ! redoSteps.empty(); }
    juce::String getUndoName() const;
    juce::String getRedoName() const;
    int getNumUndoSteps() const noexcept { return (int) undoSteps.size(); }
    int getNumRedoSteps() const noexcept { return (int) redoSteps.size(); }

    bool undo();
    bool redo();

    // Makes pending edits into a step, unless a gesture or a scope is still open.
    void commit();

    // Whether edits are waiting for commit().
    bool hasPendingEdits() const;

    // Takes the current settings as the starting point for the next step, without a step:
    // after the host loads a state, which is never recorded.
    void rebase();

    // A/B ----------------------------------------------------------------------------------

    // 0 for A, 1 for B.
    int getActiveSlot() const noexcept { return activeSlot; }

    // Swaps the live settings with the other slot. One undo step.
    void switchSlot();
    void selectSlot (int slot) { if (slot != activeSlot) switchSlot(); }

    // Copies the live settings into the other slot. One undo step.
    void copyToOtherSlot();

    // The other slot's settings, which are the live ones until it's first used.
    Snapshot getOtherSlot();

    // Saving and loading the slots with the project.
    juce::ValueTree createSlotsTree();
    void restoreSlots (const juce::ValueTree& tree);

    static const juce::Identifier slotsType;

    // Snapshots ----------------------------------------------------------------------------

    Snapshot capture() const;

    // Changes only what differs: parameters with no gesture, so the host follows without
    // writing automation; the non-parameter state, and stateApplied(), only if it changed.
    void restore (const Snapshot& snapshot);

    // Used by FancyProcessor::UserEdit.
    void beginUserEdit (const juce::String& name);
    void endUserEdit();

private:
    struct Step
    {
        enum class Kind { edit, switchSlot, copySlot };

        Kind kind = Kind::edit;
        juce::String name;
        std::vector<std::tuple<int, float, float>> values; // index, before, after
        juce::ValueTree extraBefore, extraAfter;            // invalid if unchanged
        std::shared_ptr<const Snapshot> slotBefore, slotAfter;
    };

    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

    void ensureBase();
    void startPending();
    juce::ValueTree captureExtra() const;
    void restoreExtra (const juce::ValueTree& extra);
    void apply (const Step& step, bool forwards);
    void push (Step step);
    void swapSlots();
    std::vector<float> readValues() const;

    FancyProcessor& owner;
    juce::Array<juce::AudioProcessorParameter*> parameters;
    int bypassIndex = -1;

    std::vector<Step> undoSteps, redoSteps;

    // The settings when the last step was made, and the values when this one began.
    std::vector<float> baseValues;
    juce::ValueTree baseExtra;
    int baseCounter = 0;
    bool hasBase = false;

    std::set<int> touched;
    bool touchedAll = false;
    bool pending = false;
    int openGestures = 0;
    int openScopes = 0;
    juce::String scopeName;
    bool restoring = false;

    int activeSlot = 0;
    Snapshot otherSlot;

    JUCE_DECLARE_NON_COPYABLE (Workflow)
};

} // namespace fancy
