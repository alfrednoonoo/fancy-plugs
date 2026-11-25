#include "LibraryPicker.h"
#include "VerbLibrary.h"

#include <fancy/ui/Popups.h>

using namespace fancy::ui;

namespace
{
constexpr int edge = 6, border = 1, padding = 4, searchHeight = 26, rowHeight = 22, sectionHeight = 20;
constexpr int starWidth = 22, minWidth = 248, maxListHeight = 330;
constexpr float radius = 6.0f;

// The preset browser's five-point star, in a 16 px box.
juce::Path starPath (juce::Rectangle<float> box)
{
    static const float points[] { 8.0f, 1.8f, 9.85f, 5.7f, 14.1f, 6.2f, 10.95f, 9.1f, 11.8f, 13.3f,
                                  8.0f, 11.2f, 4.2f, 13.3f, 5.05f, 9.1f, 1.9f, 6.2f, 6.15f, 5.7f };
    juce::Path path;

    for (int i = 0; i < 10; ++i)
    {
        const juce::Point<float> point { box.getX() + points[i * 2] * box.getWidth() / 16.0f,
                                         box.getY() + points[i * 2 + 1] * box.getHeight() / 16.0f };

        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    path.closeSubPath();
    return path;
}

bool matches (const juce::String& haystack, const juce::String& search)
{
    const auto lower = haystack.toLowerCase();

    for (const auto& word : juce::StringArray::fromTokens (search.toLowerCase(), " \t/", {}))
        if (word.isNotEmpty() && ! lower.contains (word))
            return false;

    return true;
}

int heightOf (const LibraryPicker::Row& row)
{
    return row.kind == LibraryPicker::Row::Kind::section ? sectionHeight : rowHeight;
}
} // namespace

// The search field ------------------------------------------------------------------------------

class LibraryPicker::Search final : public juce::TextEditor
{
public:
    explicit Search (LibraryPicker& ownerToUse) : owner (ownerToUse)
    {
        setColour (juce::TextEditor::backgroundColourId, colours::bgWell);
        setColour (juce::TextEditor::textColourId, colours::menuText);
        setColour (juce::TextEditor::highlightColourId, owner.accent.withAlpha (0.4f));
        setColour (juce::TextEditor::outlineColourId, colours::menuBorder);
        setColour (juce::TextEditor::focusedOutlineColourId, owner.accent);
        setColour (juce::CaretComponent::caretColourId, colours::menuText);
        setFont (font (Weight::regular, 13.0f));
        setIndents (24, 0);
        setEscapeAndReturnKeysConsumed (false);
        setTextToShowWhenEmpty ("Search the library", colours::menuHint);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        juce::TextEditor::paintOverChildren (g);

        // A small magnifier at the left.
        const auto centre = juce::Point<float> (12.0f, (float) getHeight() * 0.5f - 1.0f);
        g.setColour (colours::menuHint);
        g.drawEllipse (centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f, 1.4f);
        g.drawLine (centre.x + 3.0f, centre.y + 3.0f, centre.x + 6.0f, centre.y + 6.0f, 1.4f);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey
            || key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
            return owner.keyPressed (key);

        return juce::TextEditor::keyPressed (key);
    }

private:
    LibraryPicker& owner;
};

// The list -------------------------------------------------------------------------------------

class LibraryPicker::List final : public juce::Component
{
public:
    explicit List (LibraryPicker& ownerToUse) : owner (ownerToUse) {}

    int contentHeight() const
    {
        auto total = 0;

        for (const auto& row : owner.rows)
            total += heightOf (row);

        return juce::jmax (total, rowHeight);
    }

    int rowAt (float y) const
    {
        auto top = (float) -scroll;

        for (size_t i = 0; i < owner.rows.size(); ++i)
        {
            const auto h = (float) heightOf (owner.rows[i]);

            if (y >= top && y < top + h)
                return (int) i;

            top += h;
        }

        return -1;
    }

    juce::Rectangle<float> rowBounds (int index) const
    {
        auto top = -scroll;

        for (int i = 0; i < index; ++i)
            top += heightOf (owner.rows[(size_t) i]);

        return { 0.0f, (float) top, (float) getWidth(), (float) heightOf (owner.rows[(size_t) index]) };
    }

    void scrollTo (int offset)
    {
        scroll = juce::jlimit (0, juce::jmax (0, contentHeight() - getHeight()), offset);
        repaint();
    }

    void reveal (int index)
    {
        if (index < 0)
            return;

        const auto area = rowBounds (index);

        if (area.getY() < 0.0f)
            scrollTo (scroll + (int) area.getY());
        else if (area.getBottom() > (float) getHeight())
            scrollTo (scroll + (int) (area.getBottom() - (float) getHeight()));
    }

    bool isChoosable (int index) const
    {
        return index >= 0 && index < (int) owner.rows.size() && owner.rows[(size_t) index].kind != Row::Kind::section;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& rows = owner.rows;

        if (rows.empty())
        {
            g.setColour (colours::menuHint);
            g.setFont (font (Weight::regular, 12.0f));
            g.drawText ("No IRs match.", getLocalBounds().reduced (10, 0), juce::Justification::centredLeft, true);
            return;
        }

        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& row = rows[(size_t) i];
            const auto area = rowBounds (i);

            if (area.getBottom() < 0.0f || area.getY() > (float) getHeight())
                continue;

            if (row.kind == Row::Kind::section)
            {
                g.setColour (colours::menuHint);
                g.setFont (font (Weight::medium, 10.0f));
                g.drawText (row.text.toUpperCase(), area.withTrimmedLeft (10.0f).withTrimmedTop (5.0f), juce::Justification::centredLeft, true);
                continue;
            }

            if (i == highlighted)
            {
                g.setColour (colours::menuHover);
                g.fillRoundedRectangle (area, 3.0f);
                g.setColour (owner.accent);
                g.fillRoundedRectangle (area.getX(), area.getY() + 5.0f, 2.0f, area.getHeight() - 10.0f, 1.0f);
            }

            auto text = area.withTrimmedLeft (22.0f).withTrimmedRight (6.0f);

            if (row.ticked)
            {
                juce::Path tick;
                const auto x = area.getX() + 8.0f, y = area.getCentreY();
                tick.startNewSubPath (x, y);
                tick.lineTo (x + 3.0f, y + 3.0f);
                tick.lineTo (x + 9.0f, y - 4.0f);
                g.setColour (owner.accent);
                g.strokePath (tick, juce::PathStrokeType (1.6f));
            }

            if (row.kind == Row::Kind::library)
            {
                const auto star = text.removeFromRight ((float) starWidth).withSizeKeepingCentre (13.0f, 13.0f);
                const auto on = owner.processor.isFavourite (row.key);

                if (on)
                {
                    g.setColour (colours::accentYellow);
                    g.fillPath (starPath (star));
                }
                else
                {
                    g.setColour (i == highlighted ? colours::menuHint : colours::textDisabled);
                    g.strokePath (starPath (star), juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
                }
            }

            if (row.hint.isNotEmpty())
            {
                g.setColour (colours::menuHint);
                g.setFont (font (Weight::medium, 10.0f));
                g.drawText (row.hint, text.removeFromRight (24.0f), juce::Justification::centredRight, false);
            }

            g.setColour (colours::menuText);
            g.setFont (font (Weight::regular, 13.0f));
            g.drawText (row.text, text, juce::Justification::centredLeft, true);
        }

        // A thin scroll bar when the list runs past its height.
        if (const auto total = contentHeight(); total > getHeight())
        {
            const auto h = (float) getHeight() * (float) getHeight() / (float) total;
            const auto y = (float) scroll / (float) (total - getHeight()) * ((float) getHeight() - h);
            g.setColour (colours::menuBorder);
            g.fillRoundedRectangle ((float) getWidth() - 4.0f, y, 3.0f, h, 1.5f);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto row = rowAt (event.position.y);
        setHighlighted (isChoosable (row) ? row : -1);
    }

    void mouseExit (const juce::MouseEvent&) override { setHighlighted (-1); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto row = rowAt (event.position.y);

        if (! isChoosable (row) || ! contains (event.getPosition()))
            return;

        if (owner.rows[(size_t) row].kind == Row::Kind::library && event.position.x >= (float) (getWidth() - starWidth - 6))
            owner.toggleStar (row);
        else
            owner.chooseRow (row);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        scrollTo (scroll - juce::roundToInt (wheel.deltaY * 160.0f));
    }

    void setHighlighted (int row)
    {
        if (row != highlighted)
        {
            highlighted = row;
            repaint();
        }
    }

    int highlighted = -1;
    int scroll = 0;

private:
    LibraryPicker& owner;
};

// The picker -----------------------------------------------------------------------------------

void LibraryPicker::show (juce::Component& target, VerbProcessor& processor, const juce::String& currentKey,
                          juce::Array<juce::File> files, const juce::File& currentFile, std::vector<Row> actions, Callback callback)
{
    auto* host = findPopupHost (target);

    if (host == nullptr)
        return;

    std::unique_ptr<LibraryPicker> picker (new LibraryPicker (target, processor, currentKey, std::move (files), currentFile,
                                                              std::move (actions), std::move (callback)));
    auto* raw = picker.get();
    host->adoptPopup (std::move (picker));

    if (auto* menuTarget = dynamic_cast<MenuTarget*> (&target))
        menuTarget->setMenuOpen (true);

    raw->search->grabKeyboardFocus();
}

LibraryPicker::LibraryPicker (juce::Component& target, VerbProcessor& processorToUse, const juce::String& currentKey,
                              juce::Array<juce::File> files, const juce::File& currentFile, std::vector<Row> actions, Callback callback)
    : processor (processorToUse),
      targetComponent (&target),
      current (currentKey),
      folderFiles (std::move (files)),
      currentFileChosen (currentFile),
      actionRows (std::move (actions)),
      onChosen (std::move (callback)),
      accent (accentFor (target))
{
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

    auto* host = findPopupHost (target);
    auto& parent = host->getPopupParent();
    setBounds (parent.getLocalBounds());

    search = std::make_unique<Search> (*this);
    list = std::make_unique<List> (*this);
    addAndMakeVisible (*search);
    addAndMakeVisible (*list);

    search->onTextChange = [this]
    {
        rebuildRows();
        list->scrollTo (0);
        list->setHighlighted (-1);
    };

    rebuildRows();

    // Below the dropdown, or above it when there's more room there.
    const auto anchor = parent.getLocalArea (&target, target.getLocalBounds());
    const auto width = juce::jmax (minWidth, anchor.getWidth());
    const auto chrome = 2 * (border + padding) + searchHeight + 6;
    auto height = juce::jmin (list->contentHeight(), maxListHeight) + chrome;

    const auto bounds = getLocalBounds().reduced (edge);
    const auto below = bounds.getBottom() - (anchor.getBottom() + 4);
    const auto above = anchor.getY() - 4 - bounds.getY();
    int y;

    if (height <= below || below >= above)
    {
        height = juce::jmin (height, below);
        y = anchor.getBottom() + 4;
    }
    else
    {
        height = juce::jmin (height, above);
        y = anchor.getY() - 4 - height;
    }

    const auto x = juce::jlimit (bounds.getX(), juce::jmax (bounds.getX(), bounds.getRight() - width), anchor.getX());
    panel = { x, y, width, height };

    // The current IR in view, once the list has its height.
    resized();

    for (size_t i = 0; i < rows.size(); ++i)
        if (rows[i].ticked)
            list->reveal ((int) i);
}

LibraryPicker::~LibraryPicker()
{
    if (auto* menuTarget = dynamic_cast<MenuTarget*> (targetComponent.getComponent()))
        menuTarget->setMenuOpen (false);
}

LibraryPicker* LibraryPicker::findOpen (juce::Component& parent)
{
    for (int i = parent.getNumChildComponents(); --i >= 0;)
        if (auto* picker = dynamic_cast<LibraryPicker*> (parent.getChildComponent (i)))
            if (picker->isVisible() && ! picker->finished)
                return picker;

    return nullptr;
}

void LibraryPicker::rebuildRows()
{
    rows.clear();

    const auto text = search != nullptr ? search->getText().trim() : juce::String();
    const auto& entries = verb::Library::builtIn().getEntries();

    const auto libraryRow = [this] (const verb::Library::Entry& entry)
    {
        Row row;
        row.kind = Row::Kind::library;
        row.text = entry.name;
        row.key = entry.key();
        row.hint = entry.isTrueStereo() ? "TS" : juce::String();
        row.ticked = entry.key() == current;
        return row;
    };

    const auto addSection = [this] (const juce::String& title)
    {
        Row row;
        row.text = title;
        rows.push_back (row);
    };

    // Favourites first, in the order they were starred.
    {
        std::vector<Row> found;

        for (const auto& key : processor.getFavourites())
            if (const auto* entry = verb::Library::builtIn().find (key))
                if (matches (entry->category + " " + entry->name, text))
                    found.push_back (libraryRow (*entry));

        if (! found.empty())
        {
            addSection ("Favourites");
            rows.insert (rows.end(), found.begin(), found.end());
        }
    }

    for (const auto& category : verb::Library::categories())
    {
        std::vector<Row> found;

        for (const auto& entry : entries)
            if (entry.category == category && matches (entry.category + " " + entry.name, text))
                found.push_back (libraryRow (entry));

        if (! found.empty())
        {
            addSection (category);
            rows.insert (rows.end(), found.begin(), found.end());
        }
    }

    {
        std::vector<Row> found;

        for (int i = 0; i < folderFiles.size(); ++i)
        {
            if (! matches (folderFiles[i].getFileName(), text))
                continue;

            Row row;
            row.kind = Row::Kind::file;
            row.text = folderFiles[i].getFileName();
            row.id = i;
            row.ticked = folderFiles[i] == currentFileChosen;
            found.push_back (row);
        }

        if (! found.empty())
        {
            addSection ("IR folder");
            rows.insert (rows.end(), found.begin(), found.end());
        }
    }

    if (text.isEmpty() && ! actionRows.empty())
    {
        addSection ("File");
        rows.insert (rows.end(), actionRows.begin(), actionRows.end());
    }

    if (list != nullptr)
        list->repaint();
}

void LibraryPicker::resized()
{
    if (search == nullptr)
        return;

    auto inner = panel.reduced (border + padding);
    search->setBounds (inner.removeFromTop (searchHeight));
    inner.removeFromTop (6);
    list->setBounds (inner);
}

void LibraryPicker::paint (juce::Graphics& g)
{
    const auto area = panel.toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.translated (0.0f, 6.0f).expanded (2.0f), radius + 2.0f);

    g.setColour (colours::menuPanel);
    g.fillRoundedRectangle (area, radius);
    g.setColour (colours::menuBorder);
    g.drawRoundedRectangle (area.reduced (0.5f), radius, (float) border);
}

void LibraryPicker::mouseDown (const juce::MouseEvent& event)
{
    if (! panel.contains (event.getPosition()))
        close();
}

bool LibraryPicker::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        moveHighlight (key == juce::KeyPress::upKey ? -1 : 1);
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        // The highlighted row, or the first match while searching.
        auto row = list->highlighted;

        for (int i = 0; row < 0 && i < (int) rows.size(); ++i)
            if (list->isChoosable (i))
                row = i;

        if (list->isChoosable (row))
            chooseRow (row);

        return true;
    }

    return false;
}

void LibraryPicker::moveHighlight (int direction)
{
    const auto count = (int) rows.size();
    auto row = list->highlighted;

    for (int step = 0; step < count; ++step)
    {
        row = row < 0 ? (direction > 0 ? 0 : count - 1) : (row + direction + count) % count;

        if (list->isChoosable (row))
        {
            list->setHighlighted (row);
            list->reveal (row);
            return;
        }
    }
}

void LibraryPicker::setSearch (const juce::String& text)
{
    search->setText (text, true);
    rebuildRows();
    list->scrollTo (0);
    list->setHighlighted (-1);
}

void LibraryPicker::toggleStar (int index)
{
    if (index < 0 || index >= (int) rows.size() || rows[(size_t) index].kind != Row::Kind::library)
        return;

    const auto key = rows[(size_t) index].key;
    processor.setFavourite (key, ! processor.isFavourite (key));

    const auto keepScroll = list->scroll;
    rebuildRows();
    list->scrollTo (keepScroll);
}

void LibraryPicker::chooseRow (int index)
{
    if (index < 0 || index >= (int) rows.size() || rows[(size_t) index].kind == Row::Kind::section)
        return;

    const auto chosen = rows[(size_t) index];
    auto callback = std::move (onChosen);
    close();

    if (callback != nullptr)
        callback (chosen);
}

void LibraryPicker::close()
{
    if (finished)
        return;

    finished = true;
    setVisible (false);

    if (auto* menuTarget = dynamic_cast<MenuTarget*> (targetComponent.getComponent()))
        menuTarget->setMenuOpen (false);

    // Deleted later, because this can run inside the list's own mouse callback.
    if (targetComponent != nullptr)
        if (auto* host = findPopupHost (*targetComponent))
            host->disposePopup (this);
}
