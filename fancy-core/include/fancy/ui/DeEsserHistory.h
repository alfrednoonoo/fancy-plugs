#pragma once

#include "Style.h"

#include <fancy/dsp/DeEsser.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace fancy::ui
{

// The de-esser's history, shared by Fancy DS and Fancy DS-Mini: the detector level as a grey fill,
// the threshold as a line, and the reduction in the accent from the top, oldest on the left. Not
// a component: each plugin's display owns one and paints it at the top of its own area.
class DeEsserHistory
{
public:
    using Column = dsp::DeEsser::HistoryColumn;

    struct Style
    {
        juce::Colour accent;
        juce::String caption;
        bool dashedThreshold = false;
        float floorDb = -60.0f, reductionDb = 30.0f;
    };

    // One column per pixel of width, which also sets how much time each column covers.
    void setWidth (int width, dsp::DeEsser& engine);

    // Reads the columns the engine has made since the last call.
    void update (dsp::DeEsser& engine);

    // Fixed columns, for rendered screenshots. Updates stop.
    void show (std::vector<Column> fixedColumns);
    bool isFrozen() const noexcept { return frozen; }

    // The history's vertical scale, from 0 dB at the top to the floor at the bottom.
    static float yForDb (float db, float height, float floorDb) noexcept;
    static float dbForY (float y, float height, float floorDb) noexcept;

    // Paints the history into (0, 0, width, height), clipped to it.
    void paint (juce::Graphics& g, float width, float height, float thresholdDb, const Style& style) const;

private:
    std::vector<Column> columns, incoming;
    int writeIndex = 0;
    bool frozen = false;
};

} // namespace fancy::ui
