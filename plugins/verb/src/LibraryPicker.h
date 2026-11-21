#pragma once

#include "VerbProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Fancy Verb's IR picker (Phase 4 Wave 1, docs/verb.md section 3): the File module's dropdown as
// a list with a search field at its top and a star on each library row. Favourites, kept in the
// user's settings file, are listed first. Then the IR folder's files and the file actions, as the
// old menu had them. Drawn in the shared menu style, on the editor's pop-up layer.
class LibraryPicker final : public juce::Component
{
public:
    struct Row
    {
        enum class Kind { section, library, file, action };

        Kind kind = Kind::section;
        juce::String text, key, hint; // key: "Category/Name" for the library
        int id = 0;                    // a file's index, or an action's id
        bool ticked = false;
    };

    using Callback = std::function<void (const Row&)>;

    // Opens below (or above) the target. Files are the IR folder's; actions go at the end and are
    // hidden while searching.
    static void show (juce::Component& target, VerbProcessor& processor, const juce::String& currentKey,
                      juce::Array<juce::File> files, const juce::File& currentFile,
                      std::vector<Row> actions, Callback callback);

    ~LibraryPicker() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // For tests: the rows as shown, and typing into the search field.
    const std::vector<Row>& getRows() const noexcept { return rows; }
    void setSearch (const juce::String& text);
    void chooseRow (int index);
    void toggleStar (int index);

    static LibraryPicker* findOpen (juce::Component& parent);

private:
    class List;
    class Search;

    LibraryPicker (juce::Component& target, VerbProcessor& processor, const juce::String& currentKey,
                   juce::Array<juce::File> files, const juce::File& currentFile, std::vector<Row> actions, Callback callback);

    void rebuildRows();
    void close();
    void moveHighlight (int direction);
    juce::Rectangle<int> panelBounds() const noexcept { return panel; }

    VerbProcessor& processor;
    juce::Component::SafePointer<juce::Component> targetComponent;
    juce::String current;
    juce::Array<juce::File> folderFiles;
    juce::File currentFileChosen;
    std::vector<Row> actionRows, rows;
    Callback onChosen;
    juce::Colour accent;

    juce::Rectangle<int> panel;
    std::unique_ptr<Search> search;
    std::unique_ptr<List> list;
    bool finished = false;
};
