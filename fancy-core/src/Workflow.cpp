#include <fancy/plugin/FancyProcessor.h>

#include <cmath>

namespace fancy
{

namespace
{
const juce::Identifier extraType ("EXTRA");
const juce::Identifier slotType ("SLOT");
const juce::Identifier paramType ("PARAM");
const juce::Identifier activeProperty ("active");
const juce::Identifier idProperty ("id");
const juce::Identifier valueProperty ("value");

const juce::String slotNames[] { "A", "B" };
} // namespace

const juce::Identifier Workflow::slotsType ("WORKFLOW");

bool Snapshot::sameAs (const Snapshot& other) const
{
    if (values.size() != other.values.size())
        return false;

    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto a = values[i], b = other.values[i];

        if (! (std::isnan (a) && std::isnan (b)) && a != b)
            return false;
    }

    return extra.isEquivalentTo (other.extra);
}

Workflow::Workflow (FancyProcessor& processor)
    : owner (processor), parameters (processor.getParameters())
{
    for (int i = 0; i < parameters.size(); ++i)
    {
        parameters[i]->addListener (this);

        if (parameters[i] == processor.getBypassParameter())
            bypassIndex = i;
    }
}

Workflow::~Workflow()
{
    for (auto* parameter : parameters)
        parameter->removeListener (this);
}

// Snapshots ------------------------------------------------------------------------------------

std::vector<float> Workflow::readValues() const
{
    std::vector<float> values ((size_t) parameters.size());

    for (int i = 0; i < parameters.size(); ++i)
        values[(size_t) i] = i == bypassIndex ? std::nanf ("") : parameters[i]->getValue();

    return values;
}

juce::ValueTree Workflow::captureExtra() const
{
    juce::ValueTree extra (extraType);

    for (const auto& child : owner.getApvts().state)
        if (! child.hasType (paramType))
            extra.appendChild (child.createCopy(), nullptr);

    // State kept outside the tree, such as FIR's noise profile.
    owner.addToState (extra);
    return extra;
}

Snapshot Workflow::capture() const
{
    return { readValues(), captureExtra() };
}

void Workflow::restoreExtra (const juce::ValueTree& extra)
{
    auto& state = owner.getApvts().state;

    for (int i = state.getNumChildren(); --i >= 0;)
        if (! state.getChild (i).hasType (paramType))
            state.removeChild (i, nullptr);

    for (const auto& child : extra)
        state.appendChild (child.createCopy(), nullptr);

    owner.stateApplied();
}

void Workflow::restore (const Snapshot& snapshot)
{
    if (! snapshot.isValid())
        return;

    const juce::ScopedValueSetter<bool> pause (restoring, true);

    // Twice, because one parameter can move others: EQ's log-scale switch re-expresses each
    // band's frequency in 0-1. The second pass puts back the values the snapshot holds.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < parameters.size() && i < (int) snapshot.values.size(); ++i)
        {
            const auto value = snapshot.values[(size_t) i];

            if (! std::isnan (value) && parameters[i]->getValue() != value)
                parameters[i]->setValueNotifyingHost (value);
        }
    }

    if (snapshot.extra.isValid() && ! snapshot.extra.isEquivalentTo (captureExtra()))
        restoreExtra (snapshot.extra);
}

// Recording ------------------------------------------------------------------------------------

void Workflow::rebase()
{
    baseValues = readValues();
    baseExtra = captureExtra();
    baseCounter = owner.getEditCounter();
    touched.clear();
    touchedAll = false;
    pending = false;
    hasBase = true;
}

void Workflow::ensureBase()
{
    if (! hasBase)
        rebase();
}

void Workflow::startPending()
{
    ensureBase();

    // The values just before this edit, so a host's automation since the last step isn't
    // mistaken for part of it.
    if (! pending)
    {
        baseValues = readValues();
        pending = true;
    }
}

void Workflow::parameterGestureChanged (int parameterIndex, bool gestureIsStarting)
{
    // Only the editor's gestures, on the message thread. A gesture begun anywhere else isn't
    // a person moving a control.
    if (restoring || parameterIndex == bypassIndex || ! juce::MessageManager::existsAndIsCurrentThread())
        return;

    if (gestureIsStarting)
    {
        startPending();
        touched.insert (parameterIndex);
        ++openGestures;
    }
    else
    {
        openGestures = juce::jmax (0, openGestures - 1);
    }
}

void Workflow::beginUserEdit (const juce::String& name)
{
    if (restoring)
        return;

    if (openScopes++ == 0)
    {
        startPending();
        touchedAll = true;

        if (name.isNotEmpty())
            scopeName = name;
    }
}

void Workflow::endUserEdit()
{
    if (restoring)
        return;

    openScopes = juce::jmax (0, openScopes - 1);
}

bool Workflow::hasPendingEdits() const
{
    return pending || (hasBase && owner.getEditCounter() != baseCounter);
}

void Workflow::commit()
{
    if (restoring)
        return;

    if (! hasBase)
    {
        rebase();
        return;
    }

    if (openGestures > 0 || openScopes > 0)
        return;

    const auto counter = owner.getEditCounter();

    if (! pending && counter == baseCounter)
        return;

    Step step;
    const auto now = readValues();

    for (int i = 0; i < (int) now.size() && i < (int) baseValues.size(); ++i)
    {
        if (i == bypassIndex || ! (touchedAll || touched.count (i) > 0))
            continue;

        if (now[(size_t) i] != baseValues[(size_t) i])
            step.values.emplace_back (i, baseValues[(size_t) i], now[(size_t) i]);
    }

    if (counter != baseCounter || touchedAll)
    {
        auto extra = captureExtra();

        if (! extra.isEquivalentTo (baseExtra))
        {
            step.extraBefore = baseExtra;
            step.extraAfter = extra;
        }

        baseExtra = extra;
    }

    if (scopeName.isNotEmpty())
    {
        step.name = scopeName;
    }
    else if (! step.values.empty())
    {
        juce::StringArray names;

        for (const auto& change : step.values)
            names.addIfNotAlreadyThere (parameters[std::get<0> (change)]->getName (64));

        step.name = names.size() <= 2 ? names.joinIntoString (", ") : names[0] + " and " + juce::String (names.size() - 1) + " more";
    }
    else
    {
        step.name = "Edit";
    }

    baseValues = now;
    baseCounter = counter;
    touched.clear();
    touchedAll = false;
    pending = false;
    scopeName.clear();

    if (! step.values.empty() || step.extraAfter.isValid())
        push (std::move (step));
}

void Workflow::push (Step step)
{
    undoSteps.push_back (std::move (step));

    if ((int) undoSteps.size() > maxSteps)
        undoSteps.erase (undoSteps.begin());

    redoSteps.clear();
}

// Undo and redo ----------------------------------------------------------------------------------

juce::String Workflow::getUndoName() const
{
    return undoSteps.empty() ? juce::String() : undoSteps.back().name;
}

juce::String Workflow::getRedoName() const
{
    return redoSteps.empty() ? juce::String() : redoSteps.back().name;
}

void Workflow::apply (const Step& step, bool forwards)
{
    {
        const juce::ScopedValueSetter<bool> pause (restoring, true);

        switch (step.kind)
        {
            case Step::Kind::edit:
                for (int pass = 0; pass < 2; ++pass)
                {
                    for (const auto& [index, before, after] : step.values)
                    {
                        const auto value = forwards ? after : before;

                        if (parameters[index]->getValue() != value)
                            parameters[index]->setValueNotifyingHost (value);
                    }
                }

                if (step.extraAfter.isValid())
                    restoreExtra (forwards ? step.extraAfter : step.extraBefore);

                break;

            case Step::Kind::switchSlot:
                swapSlots();
                break;

            case Step::Kind::copySlot:
                otherSlot = forwards ? *step.slotAfter : *step.slotBefore;
                break;
        }
    }

    rebase();
}

bool Workflow::undo()
{
    commit();

    if (openGestures > 0 || openScopes > 0 || undoSteps.empty())
        return false;

    auto step = std::move (undoSteps.back());
    undoSteps.pop_back();

    apply (step, false);
    redoSteps.push_back (std::move (step));
    return true;
}

bool Workflow::redo()
{
    commit();

    if (openGestures > 0 || openScopes > 0 || redoSteps.empty())
        return false;

    auto step = std::move (redoSteps.back());
    redoSteps.pop_back();

    apply (step, true);
    undoSteps.push_back (std::move (step));
    return true;
}

// A/B ------------------------------------------------------------------------------------------------

Snapshot Workflow::getOtherSlot()
{
    return otherSlot.isValid() ? otherSlot : capture();
}

void Workflow::swapSlots()
{
    const auto live = capture();
    const auto other = otherSlot.isValid() ? otherSlot : live;

    restore (other);
    otherSlot = live;
    activeSlot = 1 - activeSlot;
}

void Workflow::switchSlot()
{
    commit();

    if (openGestures > 0 || openScopes > 0)
        return;

    {
        const juce::ScopedValueSetter<bool> pause (restoring, true);
        swapSlots();
    }

    Step step;
    step.kind = Step::Kind::switchSlot;
    step.name = "Switch to " + slotNames[activeSlot];
    push (std::move (step));
    rebase();
}

void Workflow::copyToOtherSlot()
{
    commit();

    Step step;
    step.kind = Step::Kind::copySlot;
    step.name = "Copy " + slotNames[activeSlot] + " to " + slotNames[1 - activeSlot];
    step.slotBefore = std::make_shared<const Snapshot> (otherSlot);
    otherSlot = capture();
    step.slotAfter = std::make_shared<const Snapshot> (otherSlot);
    push (std::move (step));
}

juce::ValueTree Workflow::createSlotsTree()
{
    juce::ValueTree tree (slotsType);
    tree.setProperty (activeProperty, slotNames[activeSlot], nullptr);

    if (! otherSlot.isValid())
        return tree;

    juce::ValueTree slot (slotType);

    for (int i = 0; i < parameters.size() && i < (int) otherSlot.values.size(); ++i)
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]);
        const auto value = otherSlot.values[(size_t) i];

        if (ranged == nullptr || std::isnan (value))
            continue;

        juce::ValueTree parameter (paramType);
        parameter.setProperty (idProperty, ranged->getParameterID(), nullptr);
        parameter.setProperty (valueProperty, ranged->convertFrom0to1 (value), nullptr);
        slot.appendChild (parameter, nullptr);
    }

    slot.appendChild (otherSlot.extra.createCopy(), nullptr);
    tree.appendChild (slot, nullptr);
    return tree;
}

void Workflow::restoreSlots (const juce::ValueTree& tree)
{
    activeSlot = tree.getProperty (activeProperty).toString() == slotNames[1] ? 1 : 0;
    otherSlot = {};

    const auto slot = tree.getChildWithName (slotType);

    if (! slot.isValid())
        return;

    // Like the main state: unknown parameters are dropped, and missing ones keep their default.
    Snapshot snapshot;
    snapshot.values.resize ((size_t) parameters.size());

    for (int i = 0; i < parameters.size(); ++i)
        snapshot.values[(size_t) i] = i == bypassIndex ? std::nanf ("") : parameters[i]->getDefaultValue();

    for (const auto& child : slot)
    {
        if (! child.hasType (paramType))
            continue;

        auto* ranged = owner.getApvts().getParameter (child.getProperty (idProperty).toString());
        const auto value = (double) child.getProperty (valueProperty);

        if (ranged == nullptr || ranged == owner.getBypassParameter() || ! std::isfinite (value))
            continue;

        const auto& range = ranged->getNormalisableRange();
        snapshot.values[(size_t) ranged->getParameterIndex()] = ranged->convertTo0to1 (juce::jlimit (range.start, range.end, (float) value));
    }

    const auto extra = slot.getChildWithName (extraType);
    snapshot.extra = extra.isValid() ? extra.createCopy() : captureExtra();
    otherSlot = std::move (snapshot);
}

} // namespace fancy
