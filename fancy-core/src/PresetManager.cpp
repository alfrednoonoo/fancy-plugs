#include <fancy/plugin/FancyProcessor.h>

#include <algorithm>
#include <cmath>

namespace fancy
{

namespace
{
const juce::Identifier presetNameProperty ("presetName");
const juce::Identifier presetPathProperty ("presetPath");
const juce::Identifier presetUserProperty ("presetUser");
const juce::Identifier presetModifiedProperty ("presetModified");
const juce::String defaultName ("Default");
const juce::String presetExtension (".fancypreset");
const juce::String factoryKeyPrefix ("factory:"), userKeyPrefix ("user:");

// Smaller than any change a control can make, larger than float rounding through the host.
constexpr float changeTolerance = 1.0e-5f;

juce::File testRoot;

// Lower-case words, split at anything that isn't a letter or digit.
juce::StringArray wordsOf (const juce::String& text)
{
    juce::StringArray words;
    juce::String word;

    for (auto character : text.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit (character))
        {
            word += character;
        }
        else if (word.isNotEmpty())
        {
            words.add (word);
            word.clear();
        }
    }

    if (word.isNotEmpty())
        words.add (word);

    return words;
}

juce::String legalName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).trim();
}

// A user category's folder. "User" is the preset folder itself.
juce::String folderFor (const juce::String& category)
{
    const auto folder = legalName (category);
    return folder.equalsIgnoreCase ("User") ? juce::String() : folder;
}
} // namespace

void setCategories (std::vector<FactoryPreset>& presets, std::initializer_list<std::pair<const char*, const char*>> categories)
{
    for (auto& preset : presets)
        for (const auto& [name, category] : categories)
            if (preset.name == juce::String::fromUTF8 (name))
                preset.category = juce::String::fromUTF8 (category);
}

juce::String PresetManager::Entry::getKey() const
{
    return isUser ? userKeyPrefix + path : factoryKeyPrefix + name;
}

PresetManager::PresetManager (FancyProcessor& processor, std::vector<FactoryPreset> factoryPresets)
    : owner (processor), factory (std::move (factoryPresets))
{
    takeDefaultSnapshot();
}

void PresetManager::setUserRootForTests (const juce::File& root)
{
    testRoot = root;
}

juce::File PresetManager::getUserDirectory() const
{
    const auto root = testRoot != juce::File()
                          ? testRoot
                          : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Fancy Plugs");

    return root.getChildFile (owner.getName()).getChildFile ("Presets");
}

juce::File PresetManager::getFavouritesFile() const
{
    return getUserDirectory().getParentDirectory().getChildFile ("favourites.xml");
}

void PresetManager::openUserDirectory() const
{
    const auto directory = getUserDirectory();

    if (directory.createDirectory())
        directory.startAsProcess();
}

bool PresetManager::isValidPresetFile (const juce::File& file) const
{
    if (file.getSize() == 0)
        return false;

    const auto xml = juce::XmlDocument::parse (file);
    return xml != nullptr && xml->hasTagName (owner.getApvts().state.getType().toString());
}

std::vector<PresetManager::UserFile> PresetManager::getUserFiles() const
{
    std::vector<UserFile> result;
    const auto directory = getUserDirectory();

    if (! directory.isDirectory())
        return result;

    const auto addFrom = [&] (const juce::File& folder, const juce::String& category, const juce::String& prefix)
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
        files.sort();

        for (const auto& file : files)
            if (isValidPresetFile (file))
                result.push_back ({ file, prefix + file.getFileNameWithoutExtension(), category });
    };

    addFrom (directory, userCategory, {});

    // One level of folders, each a category.
    auto folders = directory.findChildFiles (juce::File::findDirectories, false);
    folders.sort();

    for (const auto& folder : folders)
        addFrom (folder, folder.getFileName(), folder.getFileName() + "/");

    return result;
}

juce::File PresetManager::getUserFile (const juce::String& path) const
{
    return getUserDirectory().getChildFile (path + presetExtension);
}

std::vector<PresetManager::Entry> PresetManager::getEntries() const
{
    std::vector<Entry> entries;

    for (int i = 0; i < (int) factory.size(); ++i)
    {
        Entry entry;
        entry.name = factory[(size_t) i].name;
        entry.category = factory[(size_t) i].category;
        entry.factoryIndex = i;
        entries.push_back (entry);
    }

    for (const auto& file : getUserFiles())
    {
        Entry entry;
        entry.name = file.file.getFileNameWithoutExtension();
        entry.category = file.category;
        entry.isUser = true;
        entry.path = file.path;
        entries.push_back (entry);
    }

    return entries;
}

bool PresetManager::matchesSearch (const Entry& entry, const juce::String& search)
{
    const auto wanted = wordsOf (search);
    auto words = wordsOf (entry.name);
    words.addArray (wordsOf (entry.category));

    for (const auto& part : wanted)
    {
        const auto found = std::any_of (words.begin(), words.end(), [&part] (const juce::String& word) { return word.startsWith (part); });

        if (! found)
            return false;
    }

    return true;
}

std::vector<PresetManager::Entry> PresetManager::getFiltered (const Filter& filter) const
{
    std::vector<Entry> result;

    for (auto& entry : getEntries())
    {
        switch (filter.kind)
        {
            case Filter::Kind::all: break;
            case Filter::Kind::favourites: if (! isFavourite (entry)) continue; break;
            case Filter::Kind::factoryCategory: if (entry.isUser || entry.category != filter.category) continue; break;
            case Filter::Kind::userCategory: if (! entry.isUser || entry.category != filter.category) continue; break;
        }

        if (matchesSearch (entry, filter.search))
            result.push_back (std::move (entry));
    }

    return result;
}

juce::StringArray PresetManager::getFactoryCategories() const
{
    juce::StringArray categories;

    for (const auto& preset : factory)
        if (preset.category.isNotEmpty())
            categories.addIfNotAlreadyThere (preset.category);

    return categories;
}

juce::StringArray PresetManager::getUserCategories() const
{
    juce::StringArray categories;

    for (const auto& file : getUserFiles())
        categories.addIfNotAlreadyThere (file.category);

    return categories;
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;

    for (const auto& preset : factory)
        names.add (preset.name);

    return names;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;

    for (const auto& file : getUserFiles())
        names.add (file.file.getFileNameWithoutExtension());

    return names;
}

juce::StringArray PresetManager::getNames() const
{
    auto names = getFactoryNames();
    names.addArray (getUserNames());
    return names;
}

juce::String PresetManager::getCurrentName() const
{
    const auto name = owner.getApvts().state.getProperty (presetNameProperty).toString();
    return name.isEmpty() ? defaultName : name;
}

juce::String PresetManager::getCurrentPath() const
{
    const auto& state = owner.getApvts().state;

    // Projects saved before Phase 3 only have the name, of a preset directly in the folder.
    if (state.hasProperty (presetPathProperty))
        return state.getProperty (presetPathProperty).toString();

    return state.getProperty (presetNameProperty).toString();
}

juce::String PresetManager::getCurrentCategory() const
{
    if (! isUserPresetLoaded())
        return {};

    const auto path = getCurrentPath();
    return path.containsChar ('/') ? path.upToFirstOccurrenceOf ("/", false, false) : juce::String();
}

bool PresetManager::isUserPresetLoaded() const
{
    const auto& state = owner.getApvts().state;
    const auto path = getCurrentPath();

    if (path.isEmpty() || ! getUserFile (path).existsAsFile())
        return false;

    // Projects saved before v1.2 don't say which kind it was.
    if (state.hasProperty (presetUserProperty))
        return (bool) state.getProperty (presetUserProperty);

    return ! getFactoryNames().contains (state.getProperty (presetNameProperty).toString());
}

bool PresetManager::isCurrent (const Entry& entry) const
{
    const auto userLoaded = isUserPresetLoaded();

    if (entry.isUser)
        return userLoaded && getCurrentPath() == entry.path;

    return ! userLoaded && getCurrentName() == entry.name;
}

bool PresetManager::nameExists (const juce::String& name) const
{
    const auto trimmed = name.trim();
    return getFactoryNames().contains (trimmed, true) || getUserNames().contains (trimmed, true);
}

bool PresetManager::userPresetExists (const juce::String& name, const juce::String& category) const
{
    const auto file = legalName (name);
    const auto folder = folderFor (category);

    if (file.isEmpty())
        return false;

    return getUserFile (folder.isEmpty() ? file : folder + "/" + file).existsAsFile();
}

void PresetManager::setCurrent (const juce::String& name, const juce::String& path, bool isUser)
{
    auto& state = owner.getApvts().state;
    state.setProperty (presetNameProperty, name, nullptr);
    state.setProperty (presetUserProperty, isUser, nullptr);

    if (isUser)
        state.setProperty (presetPathProperty, path, nullptr);
    else
        state.removeProperty (presetPathProperty, nullptr);

    state.removeProperty (presetModifiedProperty, nullptr);
}

void PresetManager::takeSnapshot()
{
    reference.clear();

    for (auto* parameter : owner.getParameters())
        reference.push_back (parameter == owner.getBypassParameter() ? std::nanf ("") : parameter->getValue());

    referenceEdits = owner.getEditCount();
    modifiedWhenSaved = false;
}

void PresetManager::takeDefaultSnapshot()
{
    reference.clear();

    for (auto* parameter : owner.getParameters())
        reference.push_back (parameter == owner.getBypassParameter() ? std::nanf ("") : parameter->getDefaultValue());

    referenceEdits = owner.getEditCount();
    modifiedWhenSaved = false;
}

bool PresetManager::isModified() const
{
    if (modifiedWhenSaved || owner.getEditCount() != referenceEdits)
        return true;

    const auto& parameters = owner.getParameters();

    for (int i = 0; i < parameters.size() && i < (int) reference.size(); ++i)
        if (! std::isnan (reference[(size_t) i]) && std::abs (parameters[i]->getValue() - reference[(size_t) i]) > changeTolerance)
            return true;

    return false;
}

void PresetManager::loadFactory (const FactoryPreset& preset)
{
    for (auto* parameter : owner.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

        if (ranged == nullptr || parameter == owner.getBypassParameter())
            continue;

        auto normalised = ranged->getDefaultValue();

        for (const auto& [id, value] : preset.values)
            if (id == ranged->getParameterID())
                normalised = ranged->convertTo0to1 (value);

        ranged->setValueNotifyingHost (normalised);
    }
}

void PresetManager::load (int index)
{
    if (index < 0)
        return;

    // Factory presets don't need the user files read.
    if (index < (int) factory.size())
    {
        Entry entry;
        entry.name = factory[(size_t) index].name;
        entry.factoryIndex = index;
        load (entry);
        return;
    }

    const auto entries = getEntries();

    if (index < (int) entries.size())
        load (entries[(size_t) index]);
}

void PresetManager::load (const Entry& entry)
{
    const FancyProcessor::UserEdit edit (owner, "Load preset");

    // A listen or preview switch is always off after a preset loads (§0.3).
    owner.setListening (false);

    if (! entry.isUser)
    {
        if (entry.factoryIndex < 0 || entry.factoryIndex >= (int) factory.size())
            return;

        const auto& preset = factory[(size_t) entry.factoryIndex];
        loadFactory (preset);
        owner.factoryPresetLoaded (preset.data);
        setCurrent (preset.name, {}, false);
        takeSnapshot();
        return;
    }

    const auto file = getUserFile (entry.path);

    if (! isValidPresetFile (file))
        return;

    if (const auto xml = juce::XmlDocument::parse (file))
    {
        if (owner.applyState (juce::ValueTree::fromXml (*xml)))
        {
            setCurrent (file.getFileNameWithoutExtension(), entry.path, true);
            takeSnapshot();
        }
    }
}

bool PresetManager::step (int direction, const Filter& filter)
{
    const auto list = getFiltered (filter);

    if (list.empty())
        return false;

    const auto count = (int) list.size();
    int index = -1;

    for (int i = 0; i < count; ++i)
        if (isCurrent (list[(size_t) i]))
            index = i;

    if (index < 0)
        index = direction > 0 ? 0 : count - 1;
    else
        index = ((index + (direction > 0 ? 1 : -1)) % count + count) % count;

    load (list[(size_t) index]);
    return true;
}

void PresetManager::reload()
{
    if (isUserPresetLoaded())
    {
        Entry entry;
        entry.name = getCurrentName();
        entry.isUser = true;
        entry.path = getCurrentPath();
        load (entry);
        return;
    }

    load (juce::jmax (0, getFactoryNames().indexOf (getCurrentName())));
}

bool PresetManager::saveUser (const juce::String& name, const juce::String& category)
{
    const auto fileName = legalName (name);
    const auto folderName = folderFor (category);

    if (fileName.isEmpty())
        return false;

    const auto path = folderName.isEmpty() ? fileName : folderName + "/" + fileName;
    const auto file = getUserFile (path);

    if (! file.getParentDirectory().createDirectory())
        return false;

    setCurrent (fileName, path, true);

    auto state = owner.getApvts().copyState();
    state.setProperty ("stateVersion", FancyProcessor::stateVersion, nullptr);
    owner.addToState (state);

    const auto xml = state.createXml();

    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    takeSnapshot();
    return true;
}

bool PresetManager::renameUser (const juce::String& newName)
{
    if (! isUserPresetLoaded())
        return false;

    const auto fileName = legalName (newName);
    const auto currentPath = getCurrentPath();
    const auto folder = currentPath.containsChar ('/') ? currentPath.upToLastOccurrenceOf ("/", true, false) : juce::String();
    const auto newPath = folder + fileName;

    if (fileName.isEmpty())
        return false;

    if (newPath == currentPath)
        return true;

    if (getUserFile (newPath).existsAsFile() || ! getUserFile (currentPath).moveFileTo (getUserFile (newPath)))
        return false;

    renameFavourite (userKeyPrefix + currentPath, userKeyPrefix + newPath);

    auto& state = owner.getApvts().state;
    state.setProperty (presetNameProperty, fileName, nullptr);
    state.setProperty (presetPathProperty, newPath, nullptr);
    return true;
}

bool PresetManager::deleteUser()
{
    if (! isUserPresetLoaded())
        return false;

    const auto path = getCurrentPath();

    if (! getUserFile (path).deleteFile())
        return false;

    renameFavourite (userKeyPrefix + path, {});

    // The settings stay as they are, now measured against the defaults.
    setCurrent ({}, {}, false);
    takeDefaultSnapshot();
    return true;
}

// Favourites --------------------------------------------------------------------

void PresetManager::readFavourites() const
{
    if (favourites.has_value())
        return;

    favourites = juce::StringArray();

    // A file that can't be read counts as empty, and is only replaced when a star changes.
    if (const auto xml = juce::XmlDocument::parse (getFavouritesFile()))
        if (xml->hasTagName ("favourites"))
            for (const auto* item : xml->getChildWithTagNameIterator ("item"))
                if (const auto key = item->getStringAttribute ("key"); key.isNotEmpty())
                    favourites->addIfNotAlreadyThere (key);
}

void PresetManager::writeFavourites() const
{
    readFavourites();

    // Keys whose preset is gone are dropped.
    juce::StringArray existing;

    for (const auto& entry : getEntries())
        existing.add (entry.getKey());

    juce::XmlElement xml ("favourites");

    for (const auto& key : *favourites)
        if (existing.contains (key))
            xml.createNewChildElement ("item")->setAttribute ("key", key);

    const auto file = getFavouritesFile();

    if (file.getParentDirectory().createDirectory())
        xml.writeTo (file);
}

bool PresetManager::isFavourite (const Entry& entry) const
{
    readFavourites();
    return favourites->contains (entry.getKey());
}

int PresetManager::countFavourites() const
{
    int count = 0;

    for (const auto& entry : getEntries())
        if (isFavourite (entry))
            ++count;

    return count;
}

void PresetManager::setFavourite (const Entry& entry, bool shouldBeFavourite)
{
    readFavourites();

    if (shouldBeFavourite)
        favourites->addIfNotAlreadyThere (entry.getKey());
    else
        favourites->removeString (entry.getKey());

    writeFavourites();
}

void PresetManager::renameFavourite (const juce::String& oldKey, const juce::String& newKey)
{
    readFavourites();

    if (! favourites->contains (oldKey))
        return;

    favourites->removeString (oldKey);

    if (newKey.isNotEmpty())
        favourites->addIfNotAlreadyThere (newKey);

    writeFavourites();
}

// Projects ----------------------------------------------------------------------

void PresetManager::addToState (juce::ValueTree& state) const
{
    state.setProperty (presetModifiedProperty, isModified(), nullptr);
}

void PresetManager::stateRestored()
{
    auto& state = owner.getApvts().state;
    const auto wasModified = (bool) state.getProperty (presetModifiedProperty, false);
    state.removeProperty (presetModifiedProperty, nullptr);

    if (state.getProperty (presetNameProperty).toString().isEmpty())
        takeDefaultSnapshot();
    else
        takeSnapshot();

    modifiedWhenSaved = wasModified;
}

} // namespace fancy
