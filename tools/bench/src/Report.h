#pragma once

// FancyBench's report: report.md, a CSV per test, PNG plots and bench.json (docs/phase3-Q1.md §5).

#include "TestFile.h"

#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace bench
{

struct Machine
{
    juce::String cpu, os, date;
};

Machine thisMachine();

// `results` holds one object per plugin, in the file's order; `reference` is the Fancy Comp
// reference, or void if it wasn't run.
void writeReport (const juce::File& folder, const TestSpec& spec, const std::vector<juce::var>& results,
                  const juce::var& reference, const Machine& machine);

// Plotting, also used for the smoke results.
struct Series
{
    juce::String name;
    std::vector<std::pair<double, double>> points;
};

struct PlotSpec
{
    juce::String title, xLabel, yLabel;
    bool logX = false;
    double xMin = 0.0, xMax = 1.0, yMin = 0.0, yMax = 1.0;
};

juce::Image drawPlot (const PlotSpec& spec, const std::vector<Series>& series);
bool savePng (const juce::Image& image, const juce::File& file);

} // namespace bench
