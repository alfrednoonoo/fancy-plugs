#pragma once

#include "Style.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace fancy::ui
{

// Menus, tooltips and dialogs, drawn inside the plugin window (Phase 2.1 item S, styled as
// reference/phase2_1/16__menus_mockup.png). They sit on the component holding the layout, so
// they follow the UI size and never fall back to JUCE's default look in a top-level window.

// The editor implements this. Pop-ups find it by walking up from the component that opens them.
class PopupHost
{
public:
    virtual ~PopupHost() = default;

    // The component the pop-ups are added to: the one holding the layout, under the UI scale.
    virtual juce::Component& getPopupParent() = 0;

    // The plugin's accent, for menu bars, check marks, typed values and dialog buttons.
    virtual juce::Colour getAccentColour() = 0;

    // Keeps a pop-up alive until disposePopup. The host deletes any left when it closes.
    virtual void adoptPopup (std::unique_ptr<juce::Component> popup) = 0;
    virtual void disposePopup (juce::Component* popup) = 0;
};

PopupHost* findPopupHost (juce::Component& from);

// The accent of the plugin a component sits in, or teal outside one.
juce::Colour accentFor (juce::Component& component);

// A list of choices: items, sections, separators and submenus.
class Menu
{
public:
    using Icon = std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;

    struct Item
    {
        enum class Kind { item, section, title, separator };

        Kind kind = Kind::item;
        int id = 0;
        juce::String text, hint;
        bool enabled = true, ticked = false;
        std::shared_ptr<Menu> subMenu;
        Icon icon;
    };

    Menu& addItem (int id, const juce::String& text, bool enabled = true, bool ticked = false, const juce::String& hint = {});
    Menu& addSubMenu (const juce::String& text, Menu subMenu, bool enabled = true, bool ticked = false);

    // An uppercase grey label over a group, such as "Factory".
    Menu& addSection (const juce::String& text);

    // An uppercase grey label at the top of a right-click menu, aligned with the row edge.
    Menu& addTitle (const juce::String& text);

    Menu& addSeparator();

    // Draws an icon before the last item's text.
    Menu& withIcon (Icon icon);

    // A list of values: past 12 rows it scrolls, with the ticked row in view. Menus of
    // actions, such as the preset menu, show every row.
    Menu& setScrollingList (bool shouldScroll) { scrollingList = shouldScroll; return *this; }

    const std::vector<Item>& getItems() const noexcept { return items; }
    bool isScrollingList() const noexcept { return scrollingList; }
    bool isEmpty() const noexcept { return items.empty(); }

    using Callback = std::function<void (int chosenId)>;

    // Opens under the target, at least as wide as it. The callback gets 0 if nothing was chosen.
    // The target is drawn as open until the menu closes, if it's a MenuTarget.
    void show (juce::Component& target, Callback callback) const;

    // Opens with its top-left corner at a point in the target, for a right-click menu.
    void showAt (juce::Component& target, juce::Point<float> position, Callback callback) const;

private:
    std::vector<Item> items;
    bool scrollingList = false;
};

// A dropdown that draws itself differently while its menu is open.
class MenuTarget
{
public:
    virtual ~MenuTarget() = default;

    void setMenuOpen (bool isOpen);
    bool isMenuOpen() const noexcept { return menuOpen; }

private:
    bool menuOpen = false;
};

// The layer under an open menu: clicks on it close the menu. Public for tests.
class MenuLayer final : public juce::Component
{
public:
    class Panel;

    MenuLayer (const Menu& menu, juce::Component& target, std::optional<juce::Point<float>> position, Menu::Callback callback);
    ~MenuLayer() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // The panels open now, the top-level menu first.
    int getNumPanels() const noexcept { return (int) panels.size(); }
    Panel& getPanel (int index) const;

    // The id of the highlighted row in the deepest panel, or 0.
    int getHighlightedId() const;

    // Closes every panel and reports the choice. 0 means none.
    void finish (int chosenId);

    void openSubMenu (Panel& parent, int row);
    void closePanelsAfter (const Panel& panel);

    juce::Colour getAccent() const noexcept { return accent; }

    static MenuLayer* findOpen (juce::Component& parent);

private:
    void openPanel (const Menu& menu, juce::Rectangle<float> anchor, bool asSubMenu, std::optional<juce::Point<float>> position);

    juce::Colour accent;
    Menu::Callback callback;
    juce::Component::SafePointer<juce::Component> targetComponent;
    juce::Component::SafePointer<juce::Component> previousFocus;
    std::vector<std::unique_ptr<Panel>> panels;
    std::vector<std::shared_ptr<Menu>> menus;
    bool finished = false;
};

class MenuLayer::Panel final : public juce::Component, private juce::Timer
{
public:
    using Item = Menu::Item;

    Panel (MenuLayer& owner, std::shared_ptr<Menu> menu, float minimumWidth);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Moves the highlight to the next enabled row, by +1 or -1.
    void moveHighlight (int direction);
    void chooseHighlighted();

    int getHighlightedRow() const noexcept { return highlighted; }
    void setHighlightedRow (int row);
    int getIdealWidth() const noexcept { return idealWidth; }
    float getContentHeight() const noexcept { return contentHeight; }
    float getRowTop (int row) const;
    int getOpenSubMenuRow() const noexcept { return openSubRow; }
    void setOpenSubMenuRow (int row) { openSubRow = row; repaint(); }

    // Limits the height, scrolling the rest; the ticked row is brought into view.
    void setVisibleHeight (float height);
    bool isScrolling() const noexcept { return scrollRange > 0.0f; }

    const Menu& getMenu() const noexcept { return *menu; }

    static constexpr float border = 1.0f, padding = 4.0f, rowHeight = 26.0f, sectionHeight = 21.0f,
                           separatorHeight = 9.0f, maxScrollRows = 12.0f, scrollbarWidth = 10.0f;

private:
    void timerCallback() override;
    int rowAt (juce::Point<float> position) const;
    float rowHeightFor (const Menu::Item& item) const;
    juce::Rectangle<float> rowBounds (int row) const;
    juce::Rectangle<float> thumbBounds() const;
    void scrollTo (float offset);

    MenuLayer& layer;
    std::shared_ptr<Menu> menu;
    std::vector<float> rowTops;
    float contentHeight = 0.0f, scroll = 0.0f, scrollRange = 0.0f;
    int idealWidth = 0;
    int highlighted = -1, openSubRow = -1, pendingSubRow = -1;
    bool draggingThumb = false;
    float thumbGrab = 0.0f;
};

// The tooltip overlay. The editor owns one; it watches the mouse and shows the tooltip of the
// control under it after 600 ms, or 300 ms for a locked control.
class Tooltips final : public juce::Component, private juce::Timer
{
public:
    explicit Tooltips (juce::Component& parentToWatch);

    // Shows a tip at once, under a component, until the mouse leaves it or 5 s pass.
    void showNow (juce::Component& near, const juce::String& text);
    void hide();

    const juce::String& getShownText() const noexcept { return shownText; }

    void paint (juce::Graphics&) override;

    static constexpr int normalDelayMs = 600, lockedDelayMs = 300, maxWidth = 250;

private:
    void timerCallback() override;
    void showFor (juce::Component& near, const juce::String& text);

    juce::Component& watched;
    juce::Component::SafePointer<juce::Component> hovered, shownFor;
    juce::Point<float> lastMouse;
    juce::String shownText;
    juce::TextLayout layout;
    double hoverStart = 0.0, shownAt = 0.0;
    juce::int64 lastMouseDown = 0;
    bool pinned = false;
};

// Overlay dialog for destructive actions, prompts and messages. Esc cancels, Enter acts.
struct DialogSpec
{
    juce::String title, body;
    juce::String action { "OK" };
    bool destructive = false;
    bool showCancel = true;

    struct Field
    {
        juce::String label, text;
    };

    std::vector<Field> fields;

    // Checks the fields before the action runs. A non-empty message keeps the dialog open.
    std::function<juce::String (const juce::StringArray&)> validate;

    // A grey line under the fields that follows what's typed, such as where a preset will be
    // saved (Phase 3 Q2.1).
    std::function<juce::String (const juce::StringArray&)> note;
};

class DialogLayer final : public juce::Component, private juce::TextEditor::Listener
{
public:
    using Callback = std::function<void (bool accepted, const juce::StringArray& fields)>;

    DialogLayer (DialogSpec spec, juce::Colour accent, Callback callback);
    ~DialogLayer() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void accept();
    void cancel();

    juce::TextEditor* getField (int index) const;
    juce::StringArray getValues() const;
    juce::String getNote() const;
    const DialogSpec& getSpec() const noexcept { return spec; }
    const juce::String& getError() const noexcept { return error; }

    static DialogLayer* findOpen (juce::Component& parent);

private:
    class Button;

    void textEditorTextChanged (juce::TextEditor&) override { repaint(); }

    juce::Rectangle<float> panelBounds() const;
    float bodyHeight() const;
    void close (bool accepted);

    DialogSpec spec;
    juce::Colour accentColour;
    Callback callback;
    juce::String error;
    std::vector<std::unique_ptr<juce::TextEditor>> fields;
    std::unique_ptr<Button> cancelButton, actionButton;
    bool closed = false;
};

void showDialog (juce::Component& from, DialogSpec spec, DialogLayer::Callback callback = {});

// Shared look of a text field in a dialog or a typed value: #141414, accent border and caret.
void styleField (juce::TextEditor& editor, juce::Colour accent);

} // namespace fancy::ui
