#pragma once

#include "Style.h"

#include <fancy/dsp/LevelHistory.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace fancy::ui
{

// The level history (docs/comp.md, Phase 4 Wave 1, section 5;
// reference/phase4/29__comp_w1_mockup.png): the input waveform in grey with the output in teal
// over it, the gain reduction as a yellow line from the top, the threshold as a dashed pair of
// lines, and beat and bar lines. Oldest on the left. A click pauses it and another goes on.
class LevelHistory final : public juce::Component, public juce::SettableTooltipClient
{
public:
    // 1.5, 3, 6 and 12 s: 476, 952, 1904 and 3808 columns.
    static constexpr int numZooms = 4;
    static constexpr int maxColumns = dsp::LevelHistoryRecorder::columnsPerSpan << (numZooms - 1);

    LevelHistory();

    // New columns from the recorder. Ignored while paused.
    void push (const dsp::LevelColumn* newColumns, int count);

    void setZoom (int zoom);
    int getZoom() const noexcept { return zoom; }
    static double secondsFor (int zoom) noexcept;

    void setGrid (bool shouldShow);
    bool isGridShown() const noexcept { return grid; }

    void setPaused (bool shouldPause);
    bool isPaused() const noexcept { return paused; }

    // The largest reduction in the shown span since the last clear, in dB.
    float getPeakReduction() const noexcept;
    void clearPeak() noexcept;

    // Fixed columns, oldest first, for rendered screenshots. Updates stop.
    void show (const std::vector<dsp::LevelColumn>& fixedColumns);

    std::function<void()> onPausedChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // The reduction scale, from 0 at the top to this at the bottom.
    static constexpr float reductionRangeDb = 24.0f;

private:
    int shownColumns() const noexcept { return dsp::LevelHistoryRecorder::columnsPerSpan << zoom; }
    const dsp::LevelColumn& column (int age) const noexcept; // 0 = newest

    std::vector<dsp::LevelColumn> ring;
    int writeIndex = 0, filled = 0;
    std::int64_t pushed = 0, clearedAt = 0;
    int zoom = 1;
    bool grid = true, paused = false, frozen = false;
};

} // namespace fancy::ui
