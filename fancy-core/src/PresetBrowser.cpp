#include <fancy/ui/PresetBrowser.h>

#include <fancy/ui/Controls.h>

#include <algorithm>
#include <cmath>

namespace fancy::ui
{

namespace
{
// Menu values from §S.2, so the browser reads as one of the styled pop-ups.
constexpr float itemFontSize = 11.5f, hintFontSize = 10.5f, sectionFontSize = 9.5f;
constexpr float rowHeight = 26.0f, sectionHeight = 21.0f, textLeft = 26.0f;
constexpr float panelRadius = 6.0f, rowRadius = 4.0f;
constexpr int border = 1, padding = 4;
constexpr int inner = border + padding;
constexpr int controlHeight = 24, topGap = 4, belowTop = 6, separatorGap = 4;
constexpr int listTop = inner + topGap + controlHeight + belowTop + 1 + separatorGap;          // 44
constexpr int footerHeight = separatorGap + 1 + separatorGap + topGap + controlHeight + 2;   // 39
constexpr int chrome = listTop + footerHeight + inner;                                       // 88
constexpr int emptyHeight = 70;
constexpr int maxWidth = 420, windowMargin = 8, anchorGap = 4;
constexpr float starArea = 22.0f, scrollbarWidth = 10.0f;

const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
const juce::String starText (juce::CharPointer_UTF8 ("\xe2\x98\x85 "));
const juce::String hollowStar (juce::CharPointer_UTF8 ("\xe2\x98\x86"));

juce::String quoted (const juce::String& text)
{
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d"));
}

juce::Font itemFont (Weight weight = Weight::regular) { return font (weight, itemFontSize); }
juce::Font hintFont (Weight weight = Weight::regular) { return font (weight, hintFontSize); }
juce::Font sectionFont() { return font (Weight::semiBold, sectionFontSize).withExtraKerningFactor (0.08f); }

void paintShadow (juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Path shape;
    shape.addRoundedRectangle (area, panelRadius);
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 28, { 0, 10 }).drawForPath (g, shape);
}

void paintCheck (juce::Graphics& g, juce::Rectangle<float> row, juce::Colour colour)
{
    const auto x = row.getX() + 10.0f, y = row.getCentreY();
    juce::Path tick;
    tick.startNewSubPath (x + 0.5f, y + 0.5f);
    tick.lineTo (x + 3.5f, y + 3.5f);
    tick.lineTo (x + 8.5f, y - 4.0f);
    g.setColour (colour);
    g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

// The mockup's five-point star, in a 16 px box.
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

void paintStar (juce::Graphics& g, juce::Rectangle<float> box, bool on, bool lit, juce::Colour accent)
{
    const auto path = starPath (box);

    if (on)
    {
        g.setColour (accent);
        g.fillPath (path);
        g.strokePath (path, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
        return;
    }

    g.setColour (lit ? colours::menuHint : colours::textDisabled);
    g.strokePath (path, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
}

juce::Path folderPath (juce::Rectangle<float> box)
{
    // 14 x 12, as the mockup draws it.
    const auto sx = box.getWidth() / 14.0f, sy = box.getHeight() / 12.0f;
    const auto p = [&] (float x, float y) { return juce::Point<float> (box.getX() + x * sx, box.getY() + y * sy); };

    juce::Path path;
    path.startNewSubPath (p (1.0f, 2.2f));
    path.lineTo (p (1.0f, 10.0f));
    path.lineTo (p (13.0f, 10.0f));
    path.lineTo (p (13.0f, 3.7f));
    path.lineTo (p (6.7f, 3.7f));
    path.lineTo (p (5.3f, 1.2f));
    path.lineTo (p (1.0f, 1.2f));
    path.closeSubPath();
    return path;
}

void paintMagnifier (juce::Graphics& g, juce::Point<float> centre)
{
    g.setColour (colours::menuHint);
    g.drawEllipse (centre.x - 3.6f, centre.y - 3.6f, 7.2f, 7.2f, 1.2f);
    g.drawLine (centre.x + 2.7f, centre.y + 2.7f, centre.x + 5.8f, centre.y + 5.8f, 1.3f);
}

// The text with each searched word's start in semibold, as the mockup marks matches.
juce::AttributedString markMatches (const juce::String& text, const juce::String& search, juce::Font plain, juce::Font bold,
                                    juce::Colour colour, juce::Colour boldColour, juce::Justification justification)
{
    juce::StringArray wanted;

    for (const auto& word : juce::StringArray::fromTokens (search.toLowerCase(), " \t-/_.,", {}))
        if (word.isNotEmpty())
            wanted.add (word);

    juce::AttributedString result;
    result.setJustification (justification);
    result.setWordWrap (juce::AttributedString::none);

    int i = 0;
    const auto length = text.length();

    while (i < length)
    {
        if (! juce::CharacterFunctions::isLetterOrDigit (text[i]))
        {
            result.append (text.substring (i, i + 1), plain, colour);
            ++i;
            continue;
        }

        auto end = i;

        while (end < length && juce::CharacterFunctions::isLetterOrDigit (text[end]))
            ++end;

        const auto word = text.substring (i, end);
        int matched = 0;

        for (const auto& part : wanted)
            if (word.toLowerCase().startsWith (part))
                matched = juce::jmax (matched, part.length());

        if (matched > 0)
            result.append (word.substring (0, matched), bold, boldColour);

        if (matched < word.length())
            result.append (word.substring (matched), plain, colour);

        i = end;
    }

    return result;
}
} // namespace

// PresetPill --------------------------------------------------------------------

class PresetPill::Arrow final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Arrow (bool pointsLeft) : left (pointsLeft) {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto lit = isMouseOver();

        if (lit)
        {
            g.setColour (colours::grey5);
            g.fillRoundedRectangle (bounds, 3.0f);
        }

        const auto c = bounds.getCentre();
        juce::Path arrow;
        const auto dx = left ? 1.75f : -1.75f;
        arrow.startNewSubPath (c.x + dx, c.y - 4.0f);
        arrow.lineTo (c.x - dx, c.y);
        arrow.lineTo (c.x + dx, c.y + 4.0f);
        g.setColour (lit ? juce::Colours::white : juce::Colours::white.withAlpha (0.4f));
        g.strokePath (arrow, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (onClick != nullptr && contains (event.getPosition()))
            onClick();
    }

    std::function<void()> onClick;

private:
    bool left;
};

PresetPill::PresetPill()
    : previous (std::make_unique<Arrow> (true)), next (std::make_unique<Arrow> (false))
{
    previous->setTooltip ("Previous preset");
    next->setTooltip ("Next preset");
    previous->onClick = [this] { if (onPrevious != nullptr) onPrevious(); };
    next->onClick = [this] { if (onNext != nullptr) onNext(); };
    addAndMakeVisible (*previous);
    addAndMakeVisible (*next);
}

PresetPill::~PresetPill() = default;

void PresetPill::setText (const juce::String& text)
{
    if (text != pillText)
    {
        pillText = text;
        repaint();
    }
}

void PresetPill::resized()
{
    previous->setBounds (0, 0, arrowWidth, getHeight());
    next->setBounds (getWidth() - arrowWidth, 0, arrowWidth, getHeight());
}

void PresetPill::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (isMenuOpen() ? colours::menuHover : colours::bgBody);
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setFont (font (Weight::regular, 14.0f));
    g.setColour (colours::textPrimary);
    g.drawText (pillText, bounds.reduced ((float) arrowWidth + 2.0f, 0.0f), juce::Justification::centred, true);
}

void PresetPill::mouseUp (const juce::MouseEvent& event)
{
    if (onOpen != nullptr && contains (event.getPosition()))
        onOpen();
}

// TitleStrip --------------------------------------------------------------------

void TitleStrip::setTitle (const juce::String& pluginName, juce::Colour colour)
{
    if (colour == restColour && pluginName.toUpperCase() == (first + " " + rest).trim())
        return;

    first = pluginName.upToFirstOccurrenceOf (" ", false, false).toUpperCase();
    rest = pluginName.fromFirstOccurrenceOf (" ", false, false).toUpperCase();
    restColour = colour;
    repaint();
}

void TitleStrip::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgWell);
    g.setColour (colours::bgSection);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    const auto titleFont = font (Weight::semiBold, 10.0f).withExtraKerningFactor (0.08f);
    g.setFont (titleFont);

    auto x = 16.0f;
    const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), (float) getHeight() - 1.0f);

    g.setColour (colours::textSecondary);
    g.drawText (first, area.withX (x), juce::Justification::centredLeft, false);
    x += juce::GlyphArrangement::getStringWidth (titleFont, first) + 5.0f;

    g.setColour (restColour);
    g.drawText (rest, area.withX (x), juce::Justification::centredLeft, false);
}

// The browser's parts -------------------------------------------------------------

class PresetBrowser::SearchField final : public juce::TextEditor
{
public:
    explicit SearchField (PresetBrowser& owner) : browser (owner) {}

    bool keyPressed (const juce::KeyPress& key) override;

    void paintOverChildren (juce::Graphics& g) override
    {
        // The outline, from the look and feel.
        juce::TextEditor::paintOverChildren (g);

        paintMagnifier (g, { 12.0f, (float) getHeight() * 0.5f - 1.0f });

        // The placeholder stays while the field has focus, as the mockup shows it.
        if (getText().isEmpty())
        {
            g.setColour (colours::textDisabled);
            g.setFont (itemFont());
            g.drawText ("Search presets", getLocalBounds().toFloat().withTrimmedLeft (25.0f), juce::Justification::centredLeft, false);
        }
        else
        {
            const auto x = (float) getWidth() - 13.0f, y = (float) getHeight() * 0.5f;
            g.setColour (colours::menuHint);
            g.drawLine (x - 3.5f, y - 3.5f, x + 3.5f, y + 3.5f, 1.1f);
            g.drawLine (x - 3.5f, y + 3.5f, x + 3.5f, y - 3.5f, 1.1f);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        // The cross at the right clears the search.
        if (! getText().isEmpty() && event.x > getWidth() - 22)
        {
            setText ({}, true);
            return;
        }

        juce::TextEditor::mouseUp (event);
    }

private:
    PresetBrowser& browser;
};

class PresetBrowser::FooterButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    FooterButton (const juce::String& text, bool isFolder = false) : label (text), folder (isFolder) {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto on = isEnabled();
        g.setColour (on && isMouseOver() ? colours::menuHover : colours::bgBody);
        g.fillRoundedRectangle (bounds, 3.0f);

        if (folder)
        {
            g.setColour (juce::Colour (0xffc8c8c8));
            g.strokePath (folderPath (bounds.withSizeKeepingCentre (14.0f, 12.0f)), juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
            return;
        }

        g.setFont (itemFont());
        g.setColour (on ? colours::textPrimary : colours::textDisabled);
        g.drawText (label, bounds, juce::Justification::centred, false);
    }

    int getIdealWidth() const
    {
        return folder ? 28 : (int) std::ceil (juce::GlyphArrangement::getStringWidth (itemFont(), label)) + 18;
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (isEnabled() && onClick != nullptr && contains (event.getPosition()))
            onClick();
    }

    const juce::String& getLabel() const noexcept { return label; }

    std::function<void()> onClick;

private:
    juce::String label;
    bool folder;
};

class PresetBrowser::List final : public juce::Component
{
public:
    explicit List (PresetBrowser& owner) : browser (owner) {}

    struct Row
    {
        bool section = false;
        juce::String text;
        int entry = -1;
    };

    void setEntries (std::vector<PresetManager::Entry> newEntries)
    {
        entries = std::move (newEntries);
        rows.clear();

        const auto addSection = [this] (bool user)
        {
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                if (entries[(size_t) i].isUser != user)
                    continue;

                if (rows.empty() || rows.back().entry < 0 || entries[(size_t) rows.back().entry].isUser != user)
                    rows.push_back ({ true, user ? "User" : "Factory", -1 });

                rows.push_back ({ false, entries[(size_t) i].name, i });
            }
        };

        addSection (false);
        addSection (true);

        if (highlighted >= (int) entries.size())
            highlighted = -1;

        scrollTo (scroll);
        repaint();
    }

    float getContentHeight() const
    {
        if (entries.empty())
            return (float) emptyHeight;

        auto height = 0.0f;

        for (const auto& row : rows)
            height += row.section ? sectionHeight : rowHeight;

        return height;
    }

    int getNumEntries() const noexcept { return (int) entries.size(); }
    const PresetManager::Entry& getEntry (int index) const { return entries[(size_t) index]; }
    int getHighlighted() const noexcept { return highlighted; }

    void setHighlighted (int entry)
    {
        highlighted = juce::jlimit (-1, (int) entries.size() - 1, entry);
        scrollIntoView (highlighted);
        repaint();
    }

    void moveHighlight (int direction)
    {
        if (entries.empty())
            return;

        if (highlighted < 0)
            setHighlighted (direction > 0 ? 0 : (int) entries.size() - 1);
        else
            setHighlighted (juce::jlimit (0, (int) entries.size() - 1, highlighted + direction));
    }

    bool isScrolling() const { return getContentHeight() > (float) getHeight() + 0.5f; }

    void paint (juce::Graphics& g) override
    {
        const auto& presets = browser.presets;
        const auto showCategory = browser.filter.kind == PresetManager::Filter::Kind::all
                                  || browser.filter.kind == PresetManager::Filter::Kind::favourites;

        if (entries.empty())
        {
            paintEmpty (g);
            return;
        }

        const auto right = (float) getWidth() - (isScrolling() ? scrollbarWidth : 0.0f);
        auto y = -scroll;

        for (const auto& row : rows)
        {
            const auto height = row.section ? sectionHeight : rowHeight;
            const auto area = juce::Rectangle<float> (0.0f, y, right, height);
            y += height;

            if (area.getBottom() < 0.0f || area.getY() > (float) getHeight())
                continue;

            if (row.section)
            {
                g.setColour (colours::menuHint);
                g.setFont (sectionFont());
                g.drawText (row.text.toUpperCase(), area.withTrimmedLeft (textLeft).withTrimmedTop (6.0f).withTrimmedBottom (3.0f),
                            juce::Justification::centredLeft, true);
                continue;
            }

            const auto& entry = entries[(size_t) row.entry];
            const auto lit = row.entry == highlighted;

            if (lit)
            {
                g.setColour (colours::menuHover);
                g.fillRoundedRectangle (area, rowRadius);
                g.setColour (browser.accent);
                g.fillRoundedRectangle (area.getX(), area.getY() + 5.0f, 2.0f, area.getHeight() - 10.0f, 1.0f);
            }

            if (presets.isCurrent (entry))
                paintCheck (g, area, browser.accent);

            auto textArea = area.withTrimmedLeft (textLeft).withTrimmedRight (6.0f);
            const auto star = textArea.removeFromRight (16.0f).withSizeKeepingCentre (13.0f, 13.0f);
            paintStar (g, star, presets.isFavourite (entry), lit, browser.accent);
            textArea.removeFromRight (8.0f);

            const auto& search = browser.filter.search;

            if (showCategory && entry.category.isNotEmpty())
            {
                const auto categoryWidth = juce::GlyphArrangement::getStringWidth (hintFont (Weight::semiBold), entry.category) + 2.0f;
                const auto categoryArea = textArea.removeFromRight (categoryWidth);
                markMatches (entry.category, search, hintFont(), hintFont (Weight::semiBold), colours::menuHint, colours::menuText,
                             juce::Justification::centredRight).draw (g, categoryArea);
                textArea.removeFromRight (10.0f);
            }

            markMatches (entry.name, search, itemFont(), itemFont (Weight::semiBold), colours::menuText, colours::textPrimary,
                         juce::Justification::centredLeft).draw (g, textArea);
        }

        if (isScrolling())
        {
            const auto track = juce::Rectangle<float> ((float) getWidth() - scrollbarWidth + 3.0f, 2.0f, 4.0f, (float) getHeight() - 4.0f);
            const auto content = getContentHeight();
            const auto range = content - (float) getHeight();
            const auto thumbHeight = juce::jmax (16.0f, track.getHeight() * (float) getHeight() / content);
            const auto thumb = track.withHeight (thumbHeight).translated (0.0f, range > 0.0f ? (track.getHeight() - thumbHeight) * scroll / range : 0.0f);

            g.setColour (colours::scrollTrack);
            g.fillRoundedRectangle (track, 2.0f);
            g.setColour (colours::scrollThumb);
            g.fillRoundedRectangle (thumb, 2.0f);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto entry = entryAt (event.position);

        if (entry != highlighted)
        {
            highlighted = entry;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        highlighted = -1;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (entries.empty())
        {
            if (linkArea.contains (event.position))
                browser.setCategory (PresetManager::Filter::Kind::all);

            return;
        }

        if (event.mouseWasDraggedSinceMouseDown() || event.getNumberOfClicks() > 1)
            return;

        const auto entry = entryAt (event.position);

        if (entry < 0)
            return;

        const auto starLeft = (float) getWidth() - (isScrolling() ? scrollbarWidth : 0.0f) - 6.0f - starArea;

        if (event.position.x >= starLeft)
            browser.toggleFavourite (entry);
        else
            browser.chooseEntry (entry, false);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto entry = entryAt (event.position);

        if (entry >= 0 && event.position.x < (float) getWidth() - starArea - 16.0f)
            browser.chooseEntry (entry, true);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        scrollTo (scroll - wheel.deltaY * 120.0f);
    }

    void resized() override { scrollTo (scroll); }

    juce::String getEmptyText() const
    {
        const auto& filter = browser.filter;

        if (filter.search.trim().isEmpty())
            return filter.kind == PresetManager::Filter::Kind::favourites
                       ? "No favourites yet.\nClick " + hollowStar + " beside a preset to add it here."
                       : juce::String ("No presets here yet.");

        if (filter.kind == PresetManager::Filter::Kind::all)
            return "No presets match " + quoted (filter.search.trim()) + ".";

        return "No presets in " + browser.categoryLabel().replace (starText, {}) + " match " + quoted (filter.search.trim()) + ".";
    }

private:
    void paintEmpty (juce::Graphics& g)
    {
        const auto text = getEmptyText();
        const auto lines = juce::StringArray::fromLines (text);
        const auto hasLink = browser.filter.kind != PresetManager::Filter::Kind::all && browser.filter.search.trim().isNotEmpty();
        const auto lineHeight = 16.0f;
        const auto count = (float) lines.size() + (hasLink ? 1.0f : 0.0f);
        auto y = ((float) getHeight() - count * lineHeight) * 0.5f;

        g.setFont (hintFont());
        g.setColour (colours::menuHint);

        for (const auto& line : lines)
        {
            g.drawText (line, juce::Rectangle<float> (8.0f, y, (float) getWidth() - 16.0f, lineHeight), juce::Justification::centred, true);
            y += lineHeight;
        }

        linkArea = {};

        if (hasLink)
        {
            const juce::String link ("Search all categories");
            const auto width = juce::GlyphArrangement::getStringWidth (hintFont(), link);
            linkArea = { ((float) getWidth() - width) * 0.5f, y, width, lineHeight };
            g.setColour (browser.accent);
            g.drawText (link, linkArea, juce::Justification::centred, false);
        }
    }

    int entryAt (juce::Point<float> position) const
    {
        auto y = -scroll;

        for (const auto& row : rows)
        {
            const auto height = row.section ? sectionHeight : rowHeight;

            if (! row.section && position.y >= y && position.y < y + height)
                return row.entry;

            y += height;
        }

        return -1;
    }

    void scrollIntoView (int entry)
    {
        auto y = 0.0f;

        for (const auto& row : rows)
        {
            const auto height = row.section ? sectionHeight : rowHeight;

            if (! row.section && row.entry == entry)
            {
                if (y < scroll)
                    scrollTo (y);
                else if (y + height > scroll + (float) getHeight())
                    scrollTo (y + height - (float) getHeight());

                return;
            }

            y += height;
        }
    }

    void scrollTo (float offset)
    {
        scroll = juce::jlimit (0.0f, juce::jmax (0.0f, getContentHeight() - (float) getHeight()), offset);
        repaint();
    }

    PresetBrowser& browser;
    std::vector<PresetManager::Entry> entries;
    std::vector<Row> rows;
    int highlighted = -1;
    float scroll = 0.0f;
    juce::Rectangle<float> linkArea;
};

class PresetBrowser::Panel final : public juce::Component
{
public:
    class CategoryBox final : public juce::Component, public MenuTarget
    {
    public:
        void paint (juce::Graphics& g) override
        {
            paintDropdownBox (g, getLocalBounds().toFloat(), text, itemFontSize, isMenuOpen());
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            if (onClick != nullptr && contains (event.getPosition()))
                onClick();
        }

        juce::String text;
        std::function<void()> onClick;
    };

    explicit Panel (PresetBrowser& owner)
        : search (owner), list (owner),
          save ("Save as new" + ellipsis), reload ("Reload"), rename ("Rename" + ellipsis), remove ("Delete" + ellipsis),
          folder ({}, true)
    {
        styleField (search, owner.accent);
        search.setColour (juce::TextEditor::outlineColourId, colours::menuBorder);
        search.setColour (juce::TextEditor::focusedOutlineColourId, owner.accent);
        search.setFont (itemFont());
        search.setIndents (24, 0);
        search.setEscapeAndReturnKeysConsumed (false);

        save.setTooltip ("Save as new preset (Ctrl+S)");
        folder.setTooltip ("Open preset folder");

        for (juce::Component* child : std::initializer_list<juce::Component*> { &search, &category, &list, &save, &reload, &rename, &remove, &folder })
            addAndMakeVisible (child);

        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (colours::menuPanel);
        g.fillRoundedRectangle (bounds, panelRadius);
        g.setColour (colours::menuBorder);
        g.drawRoundedRectangle (bounds.reduced (0.5f), panelRadius, (float) border);

        g.setColour (colours::menuSep);
        g.fillRect ((float) inner + 6.0f, (float) (listTop - separatorGap - 1), (float) getWidth() - 2.0f * inner - 12.0f, 1.0f);
        g.fillRect ((float) inner + 6.0f, (float) (getHeight() - inner - footerHeight + separatorGap), (float) getWidth() - 2.0f * inner - 12.0f, 1.0f);
    }

    void resized() override
    {
        const auto width = getWidth();
        const auto narrow = width < 400;
        const auto categoryWidth = narrow ? 118 : 130;
        const auto top = inner + topGap;

        category.setBounds (width - inner - 4 - categoryWidth, top, categoryWidth, controlHeight);
        search.setBounds (inner + 4, top, category.getX() - 6 - (inner + 4), controlHeight);
        list.setBounds (inner, listTop, width - 2 * inner, getHeight() - chrome);

        const auto footerTop = getHeight() - inner - 2 - controlHeight;
        auto x = inner + 4;

        for (auto* button : { &save, &reload, &rename, &remove })
        {
            if (! button->isVisible())
                continue;

            button->setBounds (x, footerTop, button->getIdealWidth(), controlHeight);
            x += button->getWidth() + 6;
        }

        folder.setBounds (width - inner - 4 - folder.getIdealWidth(), footerTop, folder.getIdealWidth(), controlHeight);
    }

    SearchField search;
    CategoryBox category;
    List list;
    FooterButton save, reload, rename, remove, folder;
};

bool PresetBrowser::SearchField::keyPressed (const juce::KeyPress& key)
{
    auto& list = browser.panel->list;

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        list.moveHighlight (key == juce::KeyPress::upKey ? -1 : 1);
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        if (list.getHighlighted() >= 0)
            browser.chooseEntry (list.getHighlighted(), true);
        else if (list.getNumEntries() == 1)
            browser.chooseEntry (0, true);

        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        browser.close();
        return true;
    }

    return juce::TextEditor::keyPressed (key);
}

// PresetBrowser -------------------------------------------------------------------

PresetBrowser::PresetBrowser (PresetManager& presetManager, PresetManager::Filter& browserFilter, juce::Component& anchorComponent,
                              Actions browserActions)
    : presets (presetManager), filter (browserFilter), anchor (&anchorComponent),
      previousFocus (juce::Component::getCurrentlyFocusedComponent()), actions (std::move (browserActions)),
      accent (accentFor (anchorComponent))
{
    auto* host = findPopupHost (anchorComponent);
    jassert (host != nullptr);
    setBounds (host->getPopupParent().getLocalBounds());

    panel = std::make_unique<Panel> (*this);
    addAndMakeVisible (*panel);

    panel->search.setText (filter.search, false);
    panel->search.onTextChange = [this] { searchChanged(); };

    panel->category.onClick = [this] { showCategoryMenu(); };

    const auto run = [this] (std::function<void()>& action, bool closeFirst)
    {
        return [this, &action, closeFirst]
        {
            auto toRun = action;

            if (closeFirst)
                close();

            if (toRun != nullptr)
                toRun();

            if (! closeFirst)
            {
                refresh();

                if (actions.changed != nullptr)
                    actions.changed();
            }
        };
    };

    panel->save.onClick = run (actions.save, true);
    panel->reload.onClick = run (actions.reload, false);
    panel->rename.onClick = run (actions.rename, true);
    panel->remove.onClick = run (actions.remove, true);
    panel->folder.onClick = run (actions.openFolder, false);

    if (auto* target = dynamic_cast<MenuTarget*> (&anchorComponent))
        target->setMenuOpen (true);

    refresh();
}

PresetBrowser::~PresetBrowser()
{
    if (auto* target = dynamic_cast<MenuTarget*> (anchor.getComponent()))
        target->setMenuOpen (false);
}

PresetBrowser* PresetBrowser::findOpen (juce::Component& parent)
{
    for (int i = parent.getNumChildComponents(); --i >= 0;)
        if (auto* browser = dynamic_cast<PresetBrowser*> (parent.getChildComponent (i)))
            if (browser->isVisible() && ! browser->closed)
                return browser;

    return nullptr;
}

void PresetBrowser::paint (juce::Graphics& g)
{
    paintShadow (g, panel->getBounds().toFloat());
}

void PresetBrowser::parentHierarchyChanged()
{
    if (getParentComponent() != nullptr && ! closed)
        layoutPanel();
}

void PresetBrowser::mouseDown (const juce::MouseEvent&)
{
    // A click outside the panel closes the browser and isn't passed on.
    close();
}

void PresetBrowser::close()
{
    if (closed)
        return;

    closed = true;

    if (auto* parent = getParentComponent())
        if (auto* menu = MenuLayer::findOpen (*parent))
            menu->finish (0);

    panel->search.giveAwayKeyboardFocus();
    setVisible (false);

    if (auto* target = dynamic_cast<MenuTarget*> (anchor.getComponent()))
        target->setMenuOpen (false);

    if (previousFocus != nullptr && previousFocus->isShowing())
        previousFocus->grabKeyboardFocus();

    if (anchor != nullptr)
        if (auto* host = findPopupHost (*anchor))
            host->disposePopup (this);
}

juce::String PresetBrowser::categoryLabel() const
{
    switch (filter.kind)
    {
        case PresetManager::Filter::Kind::all: return "All";
        case PresetManager::Filter::Kind::favourites: return starText + "Favourites";
        case PresetManager::Filter::Kind::factoryCategory:
        case PresetManager::Filter::Kind::userCategory: return filter.category;
    }

    return "All";
}

void PresetBrowser::refresh()
{
    // A category that no longer has presets falls back to All.
    if (filter.kind == PresetManager::Filter::Kind::factoryCategory && ! presets.getFactoryCategories().contains (filter.category))
        filter.kind = PresetManager::Filter::Kind::all;

    if (filter.kind == PresetManager::Filter::Kind::userCategory && ! presets.getUserCategories().contains (filter.category))
        filter.kind = PresetManager::Filter::Kind::all;

    panel->category.text = categoryLabel();
    panel->category.repaint();
    panel->list.setEntries (presets.getFiltered (filter));
    refreshFooter();
    layoutPanel();
}

void PresetBrowser::refreshFooter()
{
    const auto userLoaded = presets.isUserPresetLoaded();
    const auto reloadWanted = presets.isModified();
    const auto changed = panel->reload.isVisible() != reloadWanted || panel->rename.isEnabled() != userLoaded;

    panel->reload.setVisible (reloadWanted);
    panel->reload.setTooltip ("Reload " + quoted (presets.getCurrentName()) + ", discarding changes");
    panel->rename.setEnabled (userLoaded);
    panel->remove.setEnabled (userLoaded);

    if (changed)
        panel->resized();

    panel->list.repaint();
}

void PresetBrowser::layoutPanel()
{
    auto* parent = getParentComponent();
    const auto area = parent != nullptr ? parent->getLocalBounds() : getLocalBounds();

    if (parent != nullptr)
        setBounds (area);

    auto anchorBounds = juce::Rectangle<int> (16, 13, 110, 24);

    if (anchor != nullptr && parent != nullptr)
        anchorBounds = parent->getLocalArea (anchor, anchor->getLocalBounds());

    const auto width = juce::jmin (maxWidth, area.getWidth() - 2 * windowMargin);
    auto x = anchorBounds.getX();

    if (x + width > area.getRight() - windowMargin)
        x = area.getRight() - windowMargin - width;

    x = juce::jmax (area.getX() + windowMargin, x);

    const auto y = anchorBounds.getBottom() + anchorGap;
    const auto available = area.getBottom() - windowMargin - y;
    const auto height = juce::jmin (available, chrome + (int) std::ceil (panel->list.getContentHeight()));

    panel->setBounds (x, y, width, juce::jmax (chrome + 26, height));
    repaint();
}

void PresetBrowser::setSearch (const juce::String& text)
{
    // The field reports its own changes later, from the message loop; this applies at once.
    panel->search.setText (text, false);
    searchChanged();
}

void PresetBrowser::searchChanged()
{
    filter.search = panel->search.getText();
    refresh();
    panel->list.setHighlighted (filter.search.trim().isEmpty() ? -1 : 0);
}

void PresetBrowser::setCategory (PresetManager::Filter::Kind kind, const juce::String& category)
{
    filter.kind = kind;
    filter.category = category;
    refresh();
}

void PresetBrowser::showCategoryMenu()
{
    constexpr int allItem = 1, favouritesItem = 2, factoryBase = 100, userBase = 1000;
    using Kind = PresetManager::Filter::Kind;

    const auto factoryCategories = presets.getFactoryCategories();
    const auto userCategories = presets.getUserCategories();
    const auto entries = presets.getEntries();

    Menu menu;
    menu.addItem (allItem, "All", true, filter.kind == Kind::all);
    menu.addItem (favouritesItem, starText + "Favourites", true, filter.kind == Kind::favourites, juce::String (presets.countFavourites()));

    if (! factoryCategories.isEmpty())
    {
        menu.addSection ("Factory");

        for (int i = 0; i < factoryCategories.size(); ++i)
            menu.addItem (factoryBase + i, factoryCategories[i], true, filter.kind == Kind::factoryCategory && filter.category == factoryCategories[i]);
    }

    if (! userCategories.isEmpty())
    {
        menu.addSection ("User");

        for (int i = 0; i < userCategories.size(); ++i)
        {
            const auto count = std::count_if (entries.begin(), entries.end(), [&] (const PresetManager::Entry& entry)
                                              { return entry.isUser && entry.category == userCategories[i]; });

            menu.addItem (userBase + i, userCategories[i], true, filter.kind == Kind::userCategory && filter.category == userCategories[i],
                          juce::String ((int) count));
        }
    }

    menu.show (panel->category, [safeThis = juce::Component::SafePointer<PresetBrowser> (this), factoryCategories, userCategories] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        if (result == allItem)
            safeThis->setCategory (Kind::all);
        else if (result == favouritesItem)
            safeThis->setCategory (Kind::favourites);
        else if (result >= userBase)
            safeThis->setCategory (Kind::userCategory, userCategories[result - userBase]);
        else
            safeThis->setCategory (Kind::factoryCategory, factoryCategories[result - factoryBase]);
    });
}

int PresetBrowser::getNumEntries() const
{
    return panel->list.getNumEntries();
}

const PresetManager::Entry& PresetBrowser::getEntry (int index) const
{
    return panel->list.getEntry (index);
}

void PresetBrowser::chooseEntry (int index, bool andClose)
{
    if (index < 0 || index >= getNumEntries())
        return;

    const auto entry = getEntry (index);
    presets.load (entry);

    if (andClose)
        close();
    else
        refresh();

    if (actions.changed != nullptr)
        actions.changed();
}

void PresetBrowser::toggleFavourite (int index)
{
    if (index < 0 || index >= getNumEntries())
        return;

    const auto entry = getEntry (index);
    presets.setFavourite (entry, ! presets.isFavourite (entry));
    refresh();
}

void PresetBrowser::setHighlightedEntry (int index)
{
    panel->list.setHighlighted (index);
}

juce::Rectangle<int> PresetBrowser::getPanelBounds() const
{
    return panel->getBounds();
}

juce::TextEditor& PresetBrowser::getSearchField()
{
    return panel->search;
}

juce::String PresetBrowser::getEmptyText() const
{
    return getNumEntries() == 0 ? panel->list.getEmptyText() : juce::String();
}

juce::StringArray PresetBrowser::getFooterLabels() const
{
    juce::StringArray labels;

    for (auto* button : { &panel->save, &panel->reload, &panel->rename, &panel->remove })
        if (button->isVisible() && button->isEnabled())
            labels.add (button->getLabel());

    return labels;
}

} // namespace fancy::ui
