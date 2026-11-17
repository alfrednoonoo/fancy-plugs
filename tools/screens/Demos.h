#pragma once

#include <juce_core/juce_core.h>

// Renders the website's demo clips as frames (Demos.cpp). Returns the process exit code.
int renderDemos (const juce::File& outputDir, const juce::String& onlySlug);
