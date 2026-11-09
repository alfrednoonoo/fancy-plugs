#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace fancy
{

class FancyProcessor;

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
    juce::String data {}; // for state beyond the parameters; see FancyProcessor::factoryPresetLoaded
    juce::String category {}; // the preset browser's category; empty for Default
};

// Gives factory presets their categories, by name (Phase 3 Q2.1). Names not listed keep none.
void setCategories (std::vector<FactoryPreset>& presets, std::initializer_list<std::pair<const char*, const char*>> categories);

// Message-thread only: it touches files and resets parameters.
class PresetManager
{
public:
    PresetManager (FancyProcessor& processor, std::vector<FactoryPreset> factoryPresets);

    // One preset as the browser lists it (Phase 3 Q2.1).
    struct Entry
    {
        juce::String name;
        juce::String category; // a factory category, "User", or a user preset's folder
        bool isUser = false;
        int factoryIndex = -1;
        juce::String path;     // a user preset's file, relative to the preset folder, without
                               // its extension and with '/' between folder and name

        // How favourites name it: "factory:<name>" or "user:<path>".
        juce::String getKey() const;
    };

    // What the browser shows: a category, or favourites, narrowed by a search.
    struct Filter
    {
        enum class Kind { all, favourites, factoryCategory, userCategory };

        Kind kind = Kind::all;
        juce::String category;
        juce::String search;

        bool operator== (const Filter& other) const
        {
            return kind == other.kind && category == other.category && search == other.search;
        }
    };

    static constexpr const char* userCategory = "User";

    // Factory presets in their order, then user presets: those directly in the preset folder,
    // then each folder in name order. User files that don't parse as this plugin's preset are
    // left out. load (int) takes an index into this.
    std::vector<Entry> getEntries() const;
    std::vector<Entry> getFiltered (const Filter& filter) const;

    // Whether every word of the search starts a word of the entry's name or category.
    static bool matchesSearch (const Entry& entry, const juce::String& search);

    // Factory categories in the order the presets first use them.
    juce::StringArray getFactoryCategories() const;

    // "User" if any preset sits directly in the preset folder, then each folder with presets.
    juce::StringArray getUserCategories() const;

    // Names in getEntries order.
    juce::StringArray getNames() const;
    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    // The loaded preset's name, or "Default" when none has been loaded.
    juce::String getCurrentName() const;

    // The loaded user preset's category, or empty for a factory preset.
    juce::String getCurrentCategory() const;

    // Whether anything has changed since the preset loaded, or since the defaults.
    bool isModified() const;

    // Whether the loaded preset is a user preset, which can be renamed and deleted.
    bool isUserPresetLoaded() const;

    // Whether this entry is the loaded preset.
    bool isCurrent (const Entry& entry) const;

    void load (int index);
    void load (const Entry& entry);

    // Loads the next (+1) or previous (-1) preset in the filtered list, wrapping at the ends.
    // Returns false if the list is empty.
    bool step (int direction, const Filter& filter);

    // Loads the current preset again, discarding changes.
    void reload();

    // Saves the current settings as a user preset, in a folder if the category isn't empty.
    bool saveUser (const juce::String& name, const juce::String& category = {});
    bool renameUser (const juce::String& newName);
    bool deleteUser();

    // Whether a factory or user preset already has this name.
    bool nameExists (const juce::String& name) const;

    // Whether saving with this name and category would replace an existing file.
    bool userPresetExists (const juce::String& name, const juce::String& category) const;

    bool isFavourite (const Entry& entry) const;
    void setFavourite (const Entry& entry, bool shouldBeFavourite);
    int countFavourites() const;

    juce::File getUserDirectory() const;
    juce::File getFavouritesFile() const;
    void openUserDirectory() const;

    // Tests keep their presets out of the real folder.
    static void setUserRootForTests (const juce::File& root);

    // Called by FancyProcessor around saving and restoring a project.
    void addToState (juce::ValueTree& state) const;
    void stateRestored();

private:
    struct UserFile
    {
        juce::File file;
        juce::String path, category;
    };

    std::vector<UserFile> getUserFiles() const;
    bool isValidPresetFile (const juce::File& file) const;
    juce::File getUserFile (const juce::String& path) const;
    juce::String getCurrentPath() const;
    void loadFactory (const FactoryPreset& preset);
    void setCurrent (const juce::String& name, const juce::String& path, bool isUser);
    void takeSnapshot();
    void takeDefaultSnapshot();

    void readFavourites() const;
    void writeFavourites() const;
    void renameFavourite (const juce::String& oldKey, const juce::String& newKey);

    FancyProcessor& owner;
    std::vector<FactoryPreset> factory;

    std::vector<float> reference;
    int referenceEdits = 0;
    bool modifiedWhenSaved = false;

    mutable std::optional<juce::StringArray> favourites;
};

} // namespace fancy
