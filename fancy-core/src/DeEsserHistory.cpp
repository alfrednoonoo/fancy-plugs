#include <fancy/ui/DeEsserHistory.h>

namespace fancy::ui
{

void DeEsserHistory::setWidth (int width, dsp::DeEsser& engine)
{
    columns.assign ((size_t) juce::jmax (1, width), { -200.0f, 0.0f });
    incoming.resize (columns.size());
    writeIndex = 0;
    engine.setHistoryColumns (width);
}

void DeEsserHistory::update (dsp::DeEsser& engine)
{
    if (frozen)
        return;

    const auto count = engine.popHistory (incoming.data(), (int) incoming.size());

    for (int i = 0; i < count && ! columns.empty(); ++i)
    {
        columns[(size_t) writeIndex] = incoming[(size_t) i];
        writeIndex = (writeIndex + 1) % (int) columns.size();
    }
}

void DeEsserHistory::show (std::vector<Column> fixedColumns)
{
    frozen = true;
    columns = std::move (fixedColumns);
    writeIndex = 0;
}

float DeEsserHistory::yForDb (float db, float height, float floorDb) noexcept
{
    return 4.0f + juce::jlimit (0.0f, 1.0f, db / floorDb) * (height - 8.0f);
}

float DeEsserHistory::dbForY (float y, float height, float floorDb) noexcept
{
    return juce::jlimit (floorDb, 0.0f, (y - 4.0f) / (height - 8.0f) * floorDb);
}

void DeEsserHistory::paint (juce::Graphics& g, float width, float height, float thresholdDb, const Style& style) const
{
    if (columns.empty())
        return;

    juce::Path level, reduction;
    const auto count = (int) columns.size();
    const auto pixelsPerDb = (height - 8.0f) / style.reductionDb;

    level.startNewSubPath (0.0f, height);
    reduction.startNewSubPath (0.0f, 0.0f);

    for (int i = 0; i < count; ++i)
    {
        const auto& column = columns[(size_t) ((writeIndex + i) % count)];
        level.lineTo ((float) i, yForDb (column.levelDb, height, style.floorDb));
        reduction.lineTo ((float) i, 4.0f + juce::jlimit (0.0f, height, column.reductionDb * pixelsPerDb));
    }

    level.lineTo ((float) count, height);
    level.closeSubPath();
    reduction.lineTo ((float) count, 0.0f);
    reduction.closeSubPath();

    g.saveState();
    g.reduceClipRegion (juce::Rectangle<float> (0.0f, 0.0f, width, height).toNearestInt());

    g.setColour (colours::textSecondary.withAlpha (0.3f));
    g.fillPath (level);

    const auto thresholdY = yForDb (thresholdDb, height, style.floorDb);
    g.setColour (style.accent.withAlpha (0.8f));

    if (style.dashedThreshold)
    {
        for (auto x = 0.0f; x < width; x += 6.0f)
            g.fillRect (x, thresholdY - 0.5f, 3.0f, 1.0f);
    }
    else
    {
        g.fillRect (0.0f, thresholdY - 0.5f, width, 1.0f);
    }

    g.setColour (style.accent.withAlpha (0.55f));
    g.fillPath (reduction);

    g.setColour (colours::textSecondary);
    g.setFont (font (Weight::regular, 10.0f));
    g.drawText (style.caption, juce::Rectangle<float> (8.0f, height - 20.0f, width - 16.0f, 16.0f),
                style.dashedThreshold ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
    g.restoreState();
}

} // namespace fancy::ui
