#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// The level history's audio side (docs/comp.md, Phase 4 Wave 1, section 5): one column per
// 1.5 s / 476 of input and output peaks, gain reduction per channel, the threshold in use and
// beat or bar marks, into a lock-free single-reader FIFO. fancy::ui::LevelHistory draws them.
namespace fancy::dsp
{

struct LevelColumn
{
    enum Mark : std::uint8_t { none, beat, bar };

    float inputPeak = 0.0f, outputPeak = 0.0f;
    float reductionDb[2] { 0.0f, 0.0f };
    float thresholdDb = -200.0f;
    std::uint8_t mark = none;
};

class LevelHistoryRecorder
{
public:
    // The finest zoom: 476 columns (one per pixel of Comp's drawer page) over 1.5 s.
    static constexpr int columnsPerSpan = 476;
    static constexpr double finestSeconds = 1.5;
    static constexpr int capacity = 8192;

    LevelHistoryRecorder() : columns ((size_t) capacity) {}

    // The rate add() is called at. Call before processing; it starts a new column.
    void prepare (double sampleRate) noexcept
    {
        samplesPerColumn = sampleRate * finestSeconds / (double) columnsPerSpan;
        restart();
    }

    // Drops the column being built. The FIFO is left alone, so the reader never races it.
    void restart() noexcept
    {
        current = {};
        position = 0.0;
    }

    // One sample: the peaks of both channels, the reduction of each and the threshold in use.
    void add (float inputPeak, float outputPeak, float reductionLeft, float reductionRight, float thresholdDb) noexcept
    {
        current.inputPeak = std::max (current.inputPeak, inputPeak);
        current.outputPeak = std::max (current.outputPeak, outputPeak);
        current.reductionDb[0] = std::max (current.reductionDb[0], reductionLeft);
        current.reductionDb[1] = std::max (current.reductionDb[1], reductionRight);
        current.thresholdDb = thresholdDb;
        position += 1.0;
    }

    bool columnDue() const noexcept { return position >= samplesPerColumn; }

    // Ends the column with a mark (LevelColumn::Mark) and starts the next one. The leftover
    // fraction carries over, so the columns keep exact time on average.
    void finishColumn (std::uint8_t mark) noexcept
    {
        current.mark = mark;

        const auto write = writeIndex.load (std::memory_order_relaxed);
        columns[(size_t) write] = current;
        writeIndex.store ((write + 1) % capacity, std::memory_order_release);

        current = {};
        position -= samplesPerColumn;
    }

    // The reader's side: copies out up to maxColumns of the columns made since the last call.
    int pop (LevelColumn* destination, int maxColumns) noexcept
    {
        const auto write = writeIndex.load (std::memory_order_acquire);
        auto read = readIndex.load (std::memory_order_relaxed);
        auto count = 0;

        while (read != write && count < maxColumns)
        {
            destination[count++] = columns[(size_t) read];
            read = (read + 1) % capacity;
        }

        readIndex.store (read, std::memory_order_relaxed);
        return count;
    }

private:
    std::vector<LevelColumn> columns;
    std::atomic<int> writeIndex { 0 }, readIndex { 0 };
    LevelColumn current;
    double samplesPerColumn = 48000.0 * finestSeconds / (double) columnsPerSpan;
    double position = 0.0;
};

} // namespace fancy::dsp
