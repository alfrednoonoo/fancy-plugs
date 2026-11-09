#include <fancy/ui/Popups.h>

#include <fancy/ui/Controls.h>

#include <cmath>

namespace fancy::ui
{

namespace
{
constexpr float itemFontSize = 11.5f, hintFontSize = 10.5f, sectionFontSize = 9.5f;
constexpr float textLeft = 26.0f, rightPadding = 10.0f, iconWidth = 16.0f, iconGap = 6.0f;
constexpr float panelRadius = 6.0f, rowRadius = 4.0f;
constexpr float minimumMenuWidth = 200.0f, edgeMargin = 4.0f, targetGap = 4.0f, subMenuOverlap = 2.0f;
constexpr int subMenuDelayMs = 200;

juce::Font itemFont() { return font (Weight::regular, itemFontSize); }
juce::Font hintFont() { return font (Weight::regular, hintFontSize); }
juce::Font sectionFont() { return font (Weight::semiBold, sectionFontSize).withExtraKerningFactor (0.08f); }

float textWidth (const juce::Font& f, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (f, text);
}

bool isSelectable (const Menu::Item& item)
{
    return item.kind == Menu::Item::Kind::item && item.enabled;
}

void paintShadow (juce::Graphics& g, juce::Rectangle<float> area, float radius, float offsetY, float blur, float alpha)
{
    juce::Path shape;
    shape.addRoundedRectangle (area, radius);
    juce::DropShadow (juce::Colours::black.withAlpha (alpha), juce::roundToInt (blur), { 0, juce::roundToInt (offsetY) }).drawForPath (g, shape);
}

void paintCheck (juce::Graphics& g, juce::Rectangle<float> row, juce::Colour colour)
{
    // A 4 x 8 px L turned 45 degrees, 10 px from the row's left, as the mockup draws it.
    const auto x = row.getX() + 10.0f, y = row.getCentreY();
    juce::Path tick;
    tick.startNewSubPath (x + 0.5f, y + 0.5f);
    tick.lineTo (x + 3.5f, y + 3.5f);
    tick.lineTo (x + 8.5f, y - 4.0f);
    g.setColour (colour);
    g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

void paintSubMenuArrow (juce::Graphics& g, juce::Rectangle<float> row)
{
    const auto x = row.getRight() - rightPadding - 3.0f, y = row.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath (x, y - 3.0f);
    arrow.lineTo (x + 3.0f, y);
    arrow.lineTo (x, y + 3.0f);
    g.setColour (colours::menuHint);
    g.strokePath (arrow, juce::PathStrokeType (1.1f));
}
} // namespace

PopupHost* findPopupHost (juce::Component& from)
{
    for (auto* component = &from; component != nullptr; component = component->getParentComponent())
        if (auto* host = dynamic_cast<PopupHost*> (component))
            return host;

    return nullptr;
}

juce::Colour accentFor (juce::Component& component)
{
    if (auto* host = findPopupHost (component))
        return host->getAccentColour();

    return colours::uiTeal;
}

void styleField (juce::TextEditor& editor, juce::Colour accent)
{
    editor.setColour (juce::TextEditor::backgroundColourId, colours::fieldFill);
    editor.setColour (juce::TextEditor::textColourId, colours::menuText);
    editor.setColour (juce::TextEditor::outlineColourId, accent);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, accent);
    editor.setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    editor.setColour (juce::TextEditor::highlightedTextColourId, colours::menuText);
    editor.setColour (juce::CaretComponent::caretColourId, accent);
    editor.setFont (font (Weight::regular, hintFontSize));
    editor.applyFontToAllText (font (Weight::regular, hintFontSize));
    editor.setJustification (juce::Justification::centredLeft);
    editor.setIndents (8, 0);
    editor.setPopupMenuEnabled (false);
}

// Menu ------------------------------------------------------------------------

Menu& Menu::addItem (int id, const juce::String& text, bool enabled, bool ticked, const juce::String& hint)
{
    Item item;
    item.id = id;
    item.text = text;
    item.enabled = enabled;
    item.ticked = ticked;
    item.hint = hint;
    items.push_back (std::move (item));
    return *this;
}

Menu& Menu::addSubMenu (const juce::String& text, Menu subMenu, bool enabled, bool ticked)
{
    Item item;
    item.text = text;
    item.enabled = enabled && ! subMenu.isEmpty();
    item.ticked = ticked;
    item.subMenu = std::make_shared<Menu> (std::move (subMenu));
    items.push_back (std::move (item));
    return *this;
}

Menu& Menu::addSection (const juce::String& text)
{
    Item item;
    item.kind = Item::Kind::section;
    item.text = text;
    items.push_back (std::move (item));
    return *this;
}

Menu& Menu::addTitle (const juce::String& text)
{
    Item item;
    item.kind = Item::Kind::title;
    item.text = text;
    items.push_back (std::move (item));
    return *this;
}

Menu& Menu::addSeparator()
{
    if (! items.empty() && items.back().kind != Item::Kind::separator)
    {
        Item item;
        item.kind = Item::Kind::separator;
        items.push_back (std::move (item));
    }

    return *this;
}

Menu& Menu::withIcon (Icon icon)
{
    if (! items.empty())
        items.back().icon = std::move (icon);

    return *this;
}

namespace
{
void openLayer (const Menu& menu, juce::Component& target, std::optional<juce::Point<float>> position, Menu::Callback callback)
{
    auto* host = findPopupHost (target);

    if (host == nullptr || menu.isEmpty())
    {
        if (callback != nullptr)
            callback (0);

        return;
    }

    auto& parent = host->getPopupParent();

    // One menu at a time.
    if (auto* open = MenuLayer::findOpen (parent))
        open->finish (0);

    auto layer = std::make_unique<MenuLayer> (menu, target, position, std::move (callback));
    auto* raw = layer.get();
    host->adoptPopup (std::move (layer));

    if (raw->isShowing())
        raw->grabKeyboardFocus();
}
} // namespace

void Menu::show (juce::Component& target, Callback callback) const
{
    openLayer (*this, target, std::nullopt, std::move (callback));
}

void Menu::showAt (juce::Component& target, juce::Point<float> position, Callback callback) const
{
    openLayer (*this, target, position, std::move (callback));
}

void MenuTarget::setMenuOpen (bool isOpen)
{
    menuOpen = isOpen;

    if (auto* component = dynamic_cast<juce::Component*> (this))
        component->repaint();
}

// MenuLayer -------------------------------------------------------------------

MenuLayer::MenuLayer (const Menu& menu, juce::Component& target, std::optional<juce::Point<float>> position, Menu::Callback onChosen)
    : accent (accentFor (target)), callback (std::move (onChosen)), targetComponent (&target),
      previousFocus (juce::Component::getCurrentlyFocusedComponent())
{
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

    auto* host = findPopupHost (target);
    jassert (host != nullptr);
    auto& parent = host->getPopupParent();
    setBounds (parent.getLocalBounds());

    if (auto* menuTarget = dynamic_cast<MenuTarget*> (&target))
        menuTarget->setMenuOpen (true);

    const auto anchor = parent.getLocalArea (&target, target.getLocalBounds()).toFloat();

    if (position.has_value())
        position = parent.getLocalPoint (&target, *position);

    openPanel (menu, anchor, false, position);
}

MenuLayer::Panel& MenuLayer::getPanel (int index) const
{
    return *panels[(size_t) index];
}

MenuLayer::~MenuLayer()
{
    if (auto* menuTarget = dynamic_cast<MenuTarget*> (targetComponent.getComponent()))
        menuTarget->setMenuOpen (false);
}

void MenuLayer::paint (juce::Graphics& g)
{
    for (const auto& panel : panels)
        paintShadow (g, panel->getBounds().toFloat(), panelRadius, 10.0f, 28.0f, 0.55f);
}

MenuLayer* MenuLayer::findOpen (juce::Component& parent)
{
    for (int i = parent.getNumChildComponents(); --i >= 0;)
        if (auto* layer = dynamic_cast<MenuLayer*> (parent.getChildComponent (i)))
            if (layer->isVisible() && ! layer->finished)
                return layer;

    return nullptr;
}

void MenuLayer::openPanel (const Menu& menu, juce::Rectangle<float> anchor, bool asSubMenu, std::optional<juce::Point<float>> position)
{
    menus.push_back (std::make_shared<Menu> (menu));
    auto panel = std::make_unique<Panel> (*this, menus.back(), asSubMenu || position.has_value() ? 0.0f : anchor.getWidth());

    const auto bounds = getLocalBounds().toFloat().reduced (edgeMargin);
    const auto chrome = 2.0f * (Panel::border + Panel::padding);
    auto height = panel->getContentHeight() + chrome;

    if (menu.isScrollingList())
        height = juce::jmin (height, Panel::maxScrollRows * Panel::rowHeight + chrome);

    // The height and top first: below the target, or above it if there's more room there.
    auto y = 0.0f;

    if (asSubMenu)
    {
        // Top-aligned with its row.
        y = anchor.getY() - Panel::border - Panel::padding;
    }
    else if (position.has_value())
    {
        y = position->y;

        if (y + height > bounds.getBottom())
            y = juce::jmax (bounds.getY(), position->y - height);
    }
    else
    {
        const auto below = bounds.getBottom() - (anchor.getBottom() + targetGap);
        const auto above = anchor.getY() - targetGap - bounds.getY();

        if (height <= below || below >= above)
        {
            height = juce::jmin (height, below);
            y = anchor.getBottom() + targetGap;
        }
        else
        {
            height = juce::jmin (height, above);
            y = anchor.getY() - targetGap - height;
        }
    }

    height = juce::jmin (height, bounds.getHeight());

    // A panel that scrolls gets the scrollbar's width on top of its text.
    const auto scrolls = panel->getContentHeight() > height - chrome + 0.5f;
    const auto width = juce::jmin ((float) panel->getIdealWidth() + (scrolls ? Panel::scrollbarWidth : 0.0f), bounds.getWidth());

    auto x = 0.0f;

    if (asSubMenu)
    {
        // Overlapping the parent by 2 px, or to its left if there's no room on the right.
        const auto& parentPanel = *panels.back();
        x = (float) parentPanel.getRight() - subMenuOverlap;

        if (x + width > bounds.getRight())
            x = (float) parentPanel.getX() - width + subMenuOverlap;
    }
    else
    {
        x = position.has_value() ? position->x : anchor.getX();
    }

    x = juce::jlimit (bounds.getX(), bounds.getRight() - width, x);
    y = juce::jlimit (bounds.getY(), bounds.getBottom() - height, y);

    panel->setBounds (juce::Rectangle<float> (x, y, width, height).getSmallestIntegerContainer());
    panel->setVisibleHeight ((float) panel->getHeight() - chrome);

    addAndMakeVisible (*panel);
    panels.push_back (std::move (panel));
    repaint();
}

void MenuLayer::openSubMenu (Panel& parent, int row)
{
    const auto& items = parent.getMenu().getItems();

    if (row < 0 || row >= (int) items.size() || items[(size_t) row].subMenu == nullptr || ! items[(size_t) row].enabled)
        return;

    closePanelsAfter (parent);
    parent.setOpenSubMenuRow (row);

    const auto rowTop = (float) parent.getY() + Panel::border + Panel::padding + parent.getRowTop (row);
    openPanel (*items[(size_t) row].subMenu, { (float) parent.getX(), rowTop, (float) parent.getWidth(), Panel::rowHeight }, true, std::nullopt);
}

void MenuLayer::closePanelsAfter (const Panel& panel)
{
    for (size_t i = 0; i < panels.size(); ++i)
    {
        if (panels[i].get() != &panel)
            continue;

        while (panels.size() > i + 1)
        {
            removeChildComponent (panels.back().get());
            panels.pop_back();
            menus.pop_back();
        }

        panels[i]->setOpenSubMenuRow (-1);
        repaint();
        return;
    }
}

int MenuLayer::getHighlightedId() const
{
    if (panels.empty())
        return 0;

    const auto& panel = *panels.back();
    const auto row = panel.getHighlightedRow();
    const auto& items = panel.getMenu().getItems();

    return row >= 0 && row < (int) items.size() ? items[(size_t) row].id : 0;
}

void MenuLayer::finish (int chosenId)
{
    if (finished)
        return;

    finished = true;
    setVisible (false);

    if (auto* menuTarget = dynamic_cast<MenuTarget*> (targetComponent.getComponent()))
        menuTarget->setMenuOpen (false);

    if (previousFocus != nullptr && previousFocus->isShowing())
        previousFocus->grabKeyboardFocus();

    auto onChosen = std::move (callback);

    // Deleted later, because this can run inside one of the panels' own mouse callbacks.
    if (targetComponent != nullptr)
        if (auto* host = findPopupHost (*targetComponent))
            host->disposePopup (this);

    if (onChosen != nullptr)
        onChosen (chosenId);
}

void MenuLayer::mouseDown (const juce::MouseEvent&)
{
    // A click outside every panel closes the menu and isn't passed on.
    finish (0);
}

bool MenuLayer::keyPressed (const juce::KeyPress& key)
{
    if (panels.empty())
        return false;

    auto& panel = *panels.back();

    if (key == juce::KeyPress::upKey)
    {
        panel.moveHighlight (-1);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        panel.moveHighlight (1);
        return true;
    }

    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        panel.chooseHighlighted();
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        const auto row = panel.getHighlightedRow();
        const auto& items = panel.getMenu().getItems();

        if (row >= 0 && row < (int) items.size() && items[(size_t) row].subMenu != nullptr)
        {
            const auto before = panels.size();
            openSubMenu (panel, row);

            if (panels.size() > before)
                panels.back()->moveHighlight (1);
        }

        return true;
    }

    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::escapeKey)
    {
        if (panels.size() > 1)
            closePanelsAfter (*panels[panels.size() - 2]);
        else if (key == juce::KeyPress::escapeKey)
            finish (0);

        return true;
    }

    return false;
}

// MenuLayer::Panel ------------------------------------------------------------

MenuLayer::Panel::Panel (MenuLayer& owner, std::shared_ptr<Menu> menuToShow, float minimumWidth)
    : layer (owner), menu (std::move (menuToShow))
{
    auto hasIcon = false;

    for (const auto& item : menu->getItems())
        hasIcon = hasIcon || item.icon != nullptr;

    auto widest = 0.0f;

    for (const auto& item : menu->getItems())
    {
        rowTops.push_back (contentHeight);
        contentHeight += rowHeightFor (item);

        auto width = 0.0f;

        if (item.kind == Item::Kind::item)
        {
            width = textLeft + (hasIcon ? iconWidth + iconGap : 0.0f) + textWidth (itemFont(), item.text) + rightPadding;

            if (item.hint.isNotEmpty())
                width += 16.0f + textWidth (hintFont(), item.hint);

            if (item.subMenu != nullptr)
                width += 16.0f;
        }
        else if (item.kind != Item::Kind::separator)
        {
            width = (item.kind == Item::Kind::title ? 10.0f : textLeft) + textWidth (sectionFont(), item.text.toUpperCase()) + rightPadding;
        }

        widest = juce::jmax (widest, width);
    }

    const auto chrome = 2.0f * (border + padding);

    // Without a scrollbar: the layer adds its width if the panel ends up scrolling.
    idealWidth = (int) std::ceil (juce::jmax (minimumMenuWidth, minimumWidth, widest + chrome));

    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

float MenuLayer::Panel::rowHeightFor (const Menu::Item& item) const
{
    switch (item.kind)
    {
        case Item::Kind::item: return rowHeight;
        case Item::Kind::separator: return separatorHeight;
        case Item::Kind::section:
        case Item::Kind::title: return sectionHeight;
    }

    return rowHeight;
}

float MenuLayer::Panel::getRowTop (int row) const
{
    return row >= 0 && row < (int) rowTops.size() ? rowTops[(size_t) row] - scroll : 0.0f;
}

juce::Rectangle<float> MenuLayer::Panel::rowBounds (int row) const
{
    const auto& item = menu->getItems()[(size_t) row];
    const auto inset = border + padding;
    const auto right = (float) getWidth() - inset - (isScrolling() ? scrollbarWidth - padding : 0.0f);

    return { inset, inset + rowTops[(size_t) row] - scroll, right - inset, rowHeightFor (item) };
}

void MenuLayer::Panel::setVisibleHeight (float height)
{
    scrollRange = juce::jmax (0.0f, contentHeight - height);
    scroll = 0.0f;

    // Brings the ticked row into view, roughly centred.
    if (scrollRange > 0.0f)
    {
        const auto& items = menu->getItems();

        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].ticked)
                scrollTo (rowTops[i] - (height - rowHeight) * 0.5f);
    }

    repaint();
}

void MenuLayer::Panel::scrollTo (float offset)
{
    scroll = juce::jlimit (0.0f, scrollRange, offset);
    repaint();
}

juce::Rectangle<float> MenuLayer::Panel::thumbBounds() const
{
    const auto inset = border + padding;
    const auto track = juce::Rectangle<float> ((float) getWidth() - border - scrollbarWidth + 3.0f, inset + 2.0f,
                                               4.0f, (float) getHeight() - 2.0f * inset - 4.0f);
    const auto visible = contentHeight - scrollRange;
    const auto thumbHeight = juce::jmax (16.0f, track.getHeight() * visible / contentHeight);
    const auto travel = track.getHeight() - thumbHeight;

    return track.withHeight (thumbHeight).translated (0.0f, scrollRange > 0.0f ? travel * scroll / scrollRange : 0.0f);
}

void MenuLayer::Panel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::menuPanel);
    g.fillRoundedRectangle (bounds, panelRadius);
    g.setColour (colours::menuBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), panelRadius, border);

    const auto& items = menu->getItems();
    auto hasIcon = false;

    for (const auto& item : items)
        hasIcon = hasIcon || item.icon != nullptr;

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (bounds.reduced (border).toNearestInt());

        for (int row = 0; row < (int) items.size(); ++row)
        {
            const auto& item = items[(size_t) row];
            const auto area = rowBounds (row);

            if (area.getBottom() < 0.0f || area.getY() > bounds.getBottom())
                continue;

            switch (item.kind)
            {
                case Item::Kind::separator:
                    g.setColour (colours::menuSep);
                    g.fillRect (area.getX() + 6.0f, area.getY() + 4.0f, area.getWidth() - 12.0f, 1.0f);
                    break;

                case Item::Kind::section:
                case Item::Kind::title:
                    g.setColour (colours::menuHint);
                    g.setFont (sectionFont());
                    g.drawText (item.text.toUpperCase(),
                                area.withTrimmedLeft (item.kind == Item::Kind::title ? 10.0f : textLeft).withTrimmedTop (6.0f).withTrimmedBottom (3.0f),
                                juce::Justification::centredLeft, true);
                    break;

                case Item::Kind::item:
                {
                    const auto lit = item.enabled && (row == highlighted || row == openSubRow);

                    if (lit)
                    {
                        g.setColour (colours::menuHover);
                        g.fillRoundedRectangle (area, rowRadius);
                        g.setColour (layer.getAccent());
                        g.fillRoundedRectangle (area.getX(), area.getY() + 5.0f, 2.0f, area.getHeight() - 10.0f, 1.0f);
                    }

                    if (item.ticked)
                        paintCheck (g, area, layer.getAccent());

                    auto textArea = area.withTrimmedLeft (textLeft).withTrimmedRight (rightPadding);
                    const auto ink = item.enabled ? colours::menuText : colours::textDisabled;

                    if (hasIcon)
                    {
                        const auto iconArea = textArea.removeFromLeft (iconWidth).withSizeKeepingCentre (iconWidth, 9.0f);
                        textArea.removeFromLeft (iconGap);

                        if (item.icon != nullptr)
                            item.icon (g, iconArea, ink);
                    }

                    if (item.subMenu != nullptr)
                    {
                        paintSubMenuArrow (g, area);
                        textArea.removeFromRight (16.0f);
                    }

                    if (item.hint.isNotEmpty())
                    {
                        g.setFont (hintFont());
                        g.setColour (item.enabled ? colours::menuHint : colours::hintDisabled);
                        g.drawText (item.hint, textArea, juce::Justification::centredRight, false);
                        textArea.removeFromRight (textWidth (hintFont(), item.hint) + 16.0f);
                    }

                    g.setFont (itemFont());
                    g.setColour (ink);
                    g.drawText (item.text, textArea, juce::Justification::centredLeft, true);
                    break;
                }
            }
        }
    }

    if (isScrolling())
    {
        const auto thumb = thumbBounds();
        const auto inset = border + padding;
        g.setColour (colours::scrollTrack);
        g.fillRoundedRectangle (thumb.getX(), inset + 2.0f, 4.0f, (float) getHeight() - 2.0f * inset - 4.0f, 2.0f);
        g.setColour (colours::scrollThumb);
        g.fillRoundedRectangle (thumb, 2.0f);
    }
}

int MenuLayer::Panel::rowAt (juce::Point<float> position) const
{
    const auto inset = border + padding;

    if (position.y < inset || position.y > (float) getHeight() - inset)
        return -1;

    for (int row = 0; row < (int) rowTops.size(); ++row)
        if (rowBounds (row).contains (position))
            return row;

    return -1;
}

void MenuLayer::Panel::setHighlightedRow (int row)
{
    if (row != highlighted)
    {
        highlighted = row;
        repaint();
    }
}

void MenuLayer::Panel::mouseMove (const juce::MouseEvent& event)
{
    const auto row = rowAt (event.position);
    const auto& items = menu->getItems();

    setHighlightedRow (row >= 0 && isSelectable (items[(size_t) row]) ? row : -1);

    // Submenus open, or close, after the mouse has rested on a row for a moment.
    const auto wanted = highlighted >= 0 && items[(size_t) highlighted].subMenu != nullptr ? highlighted : -1;

    if (wanted != openSubRow)
    {
        pendingSubRow = wanted;
        startTimer (subMenuDelayMs);
    }
    else
    {
        stopTimer();
    }
}

void MenuLayer::Panel::timerCallback()
{
    stopTimer();

    if (pendingSubRow >= 0)
        layer.openSubMenu (*this, pendingSubRow);
    else
        layer.closePanelsAfter (*this);
}

void MenuLayer::Panel::mouseExit (const juce::MouseEvent&)
{
    stopTimer();

    if (openSubRow < 0)
        setHighlightedRow (-1);
}

void MenuLayer::Panel::mouseDown (const juce::MouseEvent& event)
{
    if (isScrolling() && thumbBounds().expanded (3.0f).contains (event.position))
    {
        draggingThumb = true;
        thumbGrab = event.position.y - thumbBounds().getY();
    }
}

void MenuLayer::Panel::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingThumb)
        return;

    const auto inset = border + padding;
    const auto trackHeight = (float) getHeight() - 2.0f * inset - 4.0f;
    const auto travel = trackHeight - thumbBounds().getHeight();

    if (travel > 0.0f)
        scrollTo ((event.position.y - thumbGrab - inset - 2.0f) / travel * scrollRange);
}

void MenuLayer::Panel::mouseUp (const juce::MouseEvent& event)
{
    if (draggingThumb)
    {
        draggingThumb = false;
        return;
    }

    const auto row = rowAt (event.position);

    if (row < 0 || ! isSelectable (menu->getItems()[(size_t) row]))
        return;

    highlighted = row;
    chooseHighlighted();
}

void MenuLayer::Panel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (isScrolling())
        scrollTo (scroll - wheel.deltaY * rowHeight * 6.0f);
}

void MenuLayer::Panel::moveHighlight (int direction)
{
    const auto& items = menu->getItems();
    const auto count = (int) items.size();

    if (count == 0)
        return;

    auto row = highlighted;

    for (int step = 0; step < count; ++step)
    {
        row = row < 0 ? (direction > 0 ? 0 : count - 1) : (row + direction + count) % count;

        if (isSelectable (items[(size_t) row]))
        {
            setHighlightedRow (row);

            // Keeps the highlighted row in view.
            const auto top = rowTops[(size_t) row];
            const auto visible = contentHeight - scrollRange;

            if (top < scroll)
                scrollTo (top);
            else if (top + rowHeight > scroll + visible)
                scrollTo (top + rowHeight - visible);

            return;
        }
    }
}

void MenuLayer::Panel::chooseHighlighted()
{
    const auto& items = menu->getItems();

    if (highlighted < 0 || highlighted >= (int) items.size() || ! isSelectable (items[(size_t) highlighted]))
        return;

    const auto& item = items[(size_t) highlighted];

    if (item.subMenu != nullptr)
    {
        stopTimer();
        layer.openSubMenu (*this, highlighted);
        return;
    }

    layer.finish (item.id);
}

// Tooltips --------------------------------------------------------------------

Tooltips::Tooltips (juce::Component& parentToWatch) : watched (parentToWatch)
{
    setInterceptsMouseClicks (false, false);
    setAlwaysOnTop (true);
    setVisible (false);
    startTimer (50);
}

void Tooltips::hide()
{
    shownFor = nullptr;
    shownText.clear();
    pinned = false;
    setVisible (false);
}

void Tooltips::showNow (juce::Component& near, const juce::String& text)
{
    showFor (near, text);
    pinned = true;
    hovered = &near;
}

void Tooltips::showFor (juce::Component& near, const juce::String& text)
{
    if (text.isEmpty() || getParentComponent() == nullptr)
        return;

    juce::AttributedString string;
    string.append (text, font (Weight::regular, hintFontSize), colours::menuText);
    string.setLineSpacing (2.5f);
    string.setWordWrap (juce::AttributedString::byWord);

    constexpr float paddingX = 10.0f, paddingY = 7.0f, margin = 20.0f;
    layout.createLayout (string, (float) maxWidth - 2.0f * paddingX);

    auto textWidthUsed = 0.0f;

    for (int i = 0; i < layout.getNumLines(); ++i)
        textWidthUsed = juce::jmax (textWidthUsed, layout.getLine (i).getLineBoundsX().getLength());

    const auto width = std::ceil (textWidthUsed + 2.0f * paddingX);
    const auto height = std::ceil (layout.getHeight() + 2.0f * paddingY);

    auto& parent = *getParentComponent();
    const auto area = parent.getLocalArea (&near, near.getLocalBounds()).toFloat();
    const auto limits = parent.getLocalBounds().toFloat().reduced (edgeMargin);

    auto x = area.getX();
    auto y = area.getBottom() + targetGap;

    if (y + height > limits.getBottom())
        y = area.getY() - targetGap - height;

    x = juce::jlimit (limits.getX(), juce::jmax (limits.getX(), limits.getRight() - width), x);
    y = juce::jlimit (limits.getY(), juce::jmax (limits.getY(), limits.getBottom() - height), y);

    setBounds (juce::Rectangle<float> (x, y, width, height).expanded (margin).getSmallestIntegerContainer());
    shownFor = &near;
    shownText = text;
    shownAt = juce::Time::getMillisecondCounterHiRes();
    toFront (false);
    setVisible (true);
    repaint();
}

void Tooltips::paint (juce::Graphics& g)
{
    const auto panel = getLocalBounds().toFloat().reduced (20.0f);

    paintShadow (g, panel, 5.0f, 6.0f, 18.0f, 0.5f);
    g.setColour (colours::tooltipFill);
    g.fillRoundedRectangle (panel, 5.0f);
    g.setColour (colours::menuBorder);
    g.drawRoundedRectangle (panel.reduced (0.5f), 5.0f, 1.0f);

    layout.draw (g, panel.reduced (10.0f, 7.0f));
}

void Tooltips::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (MenuLayer::findOpen (watched) != nullptr || DialogLayer::findOpen (watched) != nullptr)
    {
        hide();
        return;
    }

    auto source = juce::Desktop::getInstance().getMainMouseSource();
    auto* under = source.getComponentUnderMouse();
    const auto mouse = source.getScreenPosition();

    // The control under the mouse that has a tooltip, if it's in this plugin.
    juce::Component* client = nullptr;
    juce::String text;

    if (under != nullptr && (under == &watched || watched.isParentOf (under)))
    {
        for (auto* component = under; component != nullptr && component != &watched; component = component->getParentComponent())
        {
            if (auto* tooltipClient = dynamic_cast<juce::TooltipClient*> (component))
            {
                text = tooltipClient->getTooltip();

                if (text.isNotEmpty())
                {
                    client = component;
                    break;
                }
            }
        }
    }

    if (pinned)
    {
        const auto stillOver = shownFor != nullptr && under != nullptr
                               && (under == shownFor.getComponent() || shownFor->isParentOf (under));

        if (! stillOver || now - shownAt > 5000.0)
            hide();

        return;
    }

    // A click hides the tip, and it stays hidden until the mouse moves to another control.
    const auto mouseDown = source.getLastMouseDownTime().toMilliseconds();

    if (source.isDragging() || mouseDown != lastMouseDown)
    {
        lastMouseDown = mouseDown;
        hide();
        hovered = client;
        hoverStart = now + 1.0e9;
        return;
    }

    if (client != hovered.getComponent())
    {
        hide();
        hovered = client;
        hoverStart = now;
        lastMouse = mouse;
        return;
    }

    if (client == nullptr)
        return;

    if (isVisible())
    {
        if (text != shownText)
            showFor (*client, text);

        return;
    }

    // Restarts the wait while the mouse is still moving over the control.
    if (mouse.getDistanceFrom (lastMouse) > 2.0f)
    {
        lastMouse = mouse;

        if (hoverStart <= now)
            hoverStart = now;

        return;
    }

    auto* control = dynamic_cast<ParamControl*> (client);
    const auto delay = control != nullptr && control->isLocked() ? lockedDelayMs : normalDelayMs;

    if (now - hoverStart >= delay)
        showFor (*client, text);
}

// DialogLayer -----------------------------------------------------------------

class DialogLayer::Button final : public juce::Component
{
public:
    Button (const juce::String& text, juce::Colour fill, bool bold, std::function<void()> clicked)
        : buttonText (text), fillColour (fill), isBold (bold), onClick (std::move (clicked))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int getIdealWidth() const
    {
        return (int) std::ceil (textWidth (buttonFont(), buttonText) + 28.0f);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (isMouseOver() ? fillColour.brighter (0.08f) : fillColour);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (isBold ? juce::Colours::white : colours::menuText);
        g.setFont (buttonFont());
        g.drawText (buttonText, getLocalBounds(), juce::Justification::centred, false);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (contains (event.getPosition()) && onClick != nullptr)
            onClick();
    }

private:
    juce::Font buttonFont() const { return font (isBold ? Weight::semiBold : Weight::regular, 11.0f); }

    juce::String buttonText;
    juce::Colour fillColour;
    bool isBold;
    std::function<void()> onClick;
};

namespace
{
constexpr float dialogWidth = 300.0f, dialogPadding = 16.0f, titleHeight = 16.0f, fieldLabelHeight = 14.0f,
                fieldHeight = 26.0f, buttonHeight = 26.0f;

juce::TextLayout bodyLayout (const juce::String& text, juce::Colour colour, float width)
{
    juce::AttributedString string;
    string.append (text, font (Weight::regular, 11.0f), colour);
    string.setLineSpacing (3.2f);
    string.setWordWrap (juce::AttributedString::byWord);

    juce::TextLayout layout;
    layout.createLayout (string, width);
    return layout;
}
} // namespace

DialogLayer::DialogLayer (DialogSpec dialogSpec, juce::Colour accent, Callback onClosed)
    : spec (std::move (dialogSpec)), accentColour (accent), callback (std::move (onClosed))
{
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

    for (const auto& field : spec.fields)
    {
        auto editor = std::make_unique<juce::TextEditor>();
        styleField (*editor, accentColour);
        editor->setText (field.text, false);
        editor->setSelectAllWhenFocused (true);
        editor->onReturnKey = [this] { accept(); };
        editor->onEscapeKey = [this] { cancel(); };
        editor->addListener (this);
        addAndMakeVisible (*editor);
        fields.push_back (std::move (editor));
    }

    if (spec.showCancel)
    {
        cancelButton = std::make_unique<Button> ("Cancel", colours::menuHover, false, [this] { cancel(); });
        addAndMakeVisible (*cancelButton);
    }

    actionButton = std::make_unique<Button> (spec.action, spec.destructive ? colours::destructive : accentColour, true, [this] { accept(); });
    addAndMakeVisible (*actionButton);
}

DialogLayer::~DialogLayer() = default;

DialogLayer* DialogLayer::findOpen (juce::Component& parent)
{
    for (int i = parent.getNumChildComponents(); --i >= 0;)
        if (auto* layer = dynamic_cast<DialogLayer*> (parent.getChildComponent (i)))
            if (layer->isVisible() && ! layer->closed)
                return layer;

    return nullptr;
}

juce::TextEditor* DialogLayer::getField (int index) const
{
    return index >= 0 && index < (int) fields.size() ? fields[(size_t) index].get() : nullptr;
}

juce::StringArray DialogLayer::getValues() const
{
    juce::StringArray values;

    for (const auto& field : fields)
        values.add (field->getText());

    return values;
}

juce::String DialogLayer::getNote() const
{
    return spec.note != nullptr ? spec.note (getValues()) : juce::String();
}

float DialogLayer::bodyHeight() const
{
    return spec.body.isEmpty() ? 0.0f : bodyLayout (spec.body, colours::menuHint, dialogWidth - 2.0f * dialogPadding).getHeight();
}

juce::Rectangle<float> DialogLayer::panelBounds() const
{
    auto height = dialogPadding + titleHeight + 6.0f;

    if (spec.body.isNotEmpty())
        height += bodyHeight() + 14.0f;

    height += (float) fields.size() * (fieldLabelHeight + 4.0f + fieldHeight + 10.0f);

    if (spec.note != nullptr)
        height += 12.0f;

    if (error.isNotEmpty())
        height += bodyLayout (error, colours::destructive, dialogWidth - 2.0f * dialogPadding).getHeight() + 10.0f;

    height += buttonHeight + dialogPadding;

    const auto width = juce::jmin (dialogWidth, (float) getWidth() - 16.0f);
    return juce::Rectangle<float> (width, height).withCentre (getLocalBounds().toFloat().getCentre());
}

void DialogLayer::resized()
{
    const auto panel = panelBounds();
    auto y = panel.getY() + dialogPadding + titleHeight + 6.0f;

    if (spec.body.isNotEmpty())
        y += bodyHeight() + 14.0f;

    for (auto& field : fields)
    {
        y += fieldLabelHeight + 4.0f;
        field->setBounds (juce::Rectangle<float> (panel.getX() + dialogPadding, y, panel.getWidth() - 2.0f * dialogPadding, fieldHeight).toNearestInt());
        y += fieldHeight + 10.0f;
    }

    const auto buttonsTop = panel.getBottom() - dialogPadding - buttonHeight;
    auto right = panel.getRight() - dialogPadding;

    const auto actionWidth = (float) actionButton->getIdealWidth();
    actionButton->setBounds (juce::Rectangle<float> (right - actionWidth, buttonsTop, actionWidth, buttonHeight).toNearestInt());
    right -= actionWidth + 8.0f;

    if (cancelButton != nullptr)
    {
        const auto cancelWidth = (float) cancelButton->getIdealWidth();
        cancelButton->setBounds (juce::Rectangle<float> (right - cancelWidth, buttonsTop, cancelWidth, buttonHeight).toNearestInt());
    }
}

void DialogLayer::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.4f));

    const auto panel = panelBounds();
    paintShadow (g, panel, 8.0f, 14.0f, 36.0f, 0.6f);
    g.setColour (colours::menuPanel);
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (colours::menuBorder);
    g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);

    const auto contentWidth = panel.getWidth() - 2.0f * dialogPadding;
    auto y = panel.getY() + dialogPadding;

    g.setColour (colours::menuText);
    g.setFont (font (Weight::semiBold, 12.5f));
    g.drawText (spec.title, juce::Rectangle<float> (panel.getX() + dialogPadding, y, contentWidth, titleHeight), juce::Justification::centredLeft, true);
    y += titleHeight + 6.0f;

    if (spec.body.isNotEmpty())
    {
        const auto layout = bodyLayout (spec.body, colours::menuHint, contentWidth);
        layout.draw (g, { panel.getX() + dialogPadding, y, contentWidth, layout.getHeight() });
        y += layout.getHeight() + 14.0f;
    }

    g.setFont (font (Weight::regular, hintFontSize));

    for (size_t i = 0; i < fields.size(); ++i)
    {
        g.setColour (colours::menuHint);
        g.drawText (spec.fields[i].label, juce::Rectangle<float> (panel.getX() + dialogPadding, y, contentWidth, fieldLabelHeight), juce::Justification::centredLeft, true);
        y += fieldLabelHeight + 4.0f + fieldHeight + 10.0f;
    }

    if (spec.note != nullptr)
    {
        g.setColour (colours::menuHint);
        g.drawText (getNote(), juce::Rectangle<float> (panel.getX() + dialogPadding, y - 6.0f, contentWidth, 14.0f),
                    juce::Justification::centredLeft, true);
        y += 12.0f;
    }

    if (error.isNotEmpty())
    {
        const auto layout = bodyLayout (error, colours::destructive, contentWidth);
        layout.draw (g, { panel.getX() + dialogPadding, y, contentWidth, layout.getHeight() });
    }
}

void DialogLayer::mouseDown (const juce::MouseEvent& event)
{
    if (! panelBounds().contains (event.position))
        cancel();
}

bool DialogLayer::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        cancel();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        accept();
        return true;
    }

    return false;
}

void DialogLayer::accept()
{
    if (spec.validate != nullptr)
    {
        juce::StringArray values;

        for (auto& field : fields)
            values.add (field->getText());

        error = spec.validate (values);

        if (error.isNotEmpty())
        {
            resized();
            repaint();
            return;
        }
    }

    close (true);
}

void DialogLayer::cancel()
{
    close (false);
}

void DialogLayer::close (bool accepted)
{
    if (closed)
        return;

    closed = true;

    juce::StringArray values;

    for (auto& field : fields)
        values.add (field->getText());

    setVisible (false);
    auto onClosed = std::move (callback);

    if (auto* parent = getParentComponent())
        if (auto* host = findPopupHost (*parent))
            host->disposePopup (this);

    if (onClosed != nullptr)
        onClosed (accepted, values);
}

void showDialog (juce::Component& from, DialogSpec spec, DialogLayer::Callback callback)
{
    auto* host = findPopupHost (from);

    if (host == nullptr)
    {
        if (callback != nullptr)
            callback (false, {});

        return;
    }

    auto& parent = host->getPopupParent();

    if (auto* menu = MenuLayer::findOpen (parent))
        menu->finish (0);

    if (auto* open = DialogLayer::findOpen (parent))
        open->cancel();

    auto layer = std::make_unique<DialogLayer> (std::move (spec), host->getAccentColour(), std::move (callback));
    auto* raw = layer.get();
    layer->setBounds (parent.getLocalBounds());
    host->adoptPopup (std::move (layer));

    if (! raw->isShowing())
        return;

    if (auto* field = raw->getField (0))
        field->grabKeyboardFocus();
    else
        raw->grabKeyboardFocus();
}

} // namespace fancy::ui
