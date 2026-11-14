// Builds Fancy Verb's IR library (Phase 3 Q2.8, docs/verb.md) from the recipes, with the
// plugin's own generators: verb-library <recipes.json> <library.bin>

#include "VerbLibrary.h"

#include <cstdio>

int main (int argc, char* argv[])
{
    if (argc != 3)
    {
        std::fprintf (stderr, "usage: verb-library <recipes.json> <library.bin>\n");
        return 2;
    }

    const juce::File recipesFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    const auto recipes = verb::parseRecipes (recipesFile.loadFileAsString());

    if (recipes.empty())
    {
        std::fprintf (stderr, "verb-library: no recipes in %s\n", recipesFile.getFullPathName().toRawUTF8());
        return 1;
    }

    const auto packed = verb::packLibrary (recipes);
    const verb::Library check (packed);

    if (check.getEntries().size() != recipes.size())
    {
        std::fprintf (stderr, "verb-library: packing failed\n");
        return 1;
    }

    output.getParentDirectory().createDirectory();

    if (! output.replaceWithData (packed.getData(), packed.getSize()))
    {
        std::fprintf (stderr, "verb-library: can't write %s\n", output.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("verb-library: %d IRs, %.1f MB\n", (int) recipes.size(), (double) packed.getSize() / (1024.0 * 1024.0));
    return 0;
}
