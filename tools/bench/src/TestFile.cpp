#include "TestFile.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace bench
{

namespace
{

const juce::StringArray knownTests { "cpu", "latency", "response", "thd", "aliasing", "null", "clicks" };

juce::Result fail (const juce::String& message) { return juce::Result::fail (message); }

int editDistance (const juce::String& a, const juce::String& b)
{
    std::vector<int> row ((size_t) b.length() + 1);

    for (int j = 0; j <= b.length(); ++j)
        row[(size_t) j] = j;

    for (int i = 1; i <= a.length(); ++i)
    {
        auto previous = row[0];
        row[0] = i;

        for (int j = 1; j <= b.length(); ++j)
        {
            const auto current = row[(size_t) j];
            const auto same = juce::CharacterFunctions::toLowerCase (a[i - 1]) == juce::CharacterFunctions::toLowerCase (b[j - 1]);
            row[(size_t) j] = std::min ({ row[(size_t) j] + 1, row[(size_t) j - 1] + 1, previous + (same ? 0 : 1) });
            previous = current;
        }
    }

    return row[(size_t) b.length()];
}

juce::String nameOf (juce::AudioProcessorParameter& parameter)
{
    return parameter.getName (512).trim();
}

bool closeEnough (double requested, double shown)
{
    if (std::isinf (requested) || std::isinf (shown))
        return requested == shown;

    return std::abs (requested - shown) <= std::max (1.0e-6, 0.005 * std::abs (requested));
}

int samplePoints (juce::AudioProcessorParameter& parameter)
{
    const auto steps = parameter.getNumSteps();
    return steps >= 2 && steps < 1001 ? steps : 1001;
}

float pointValue (int index, int points)
{
    return points > 1 ? (float) index / (float) (points - 1) : 0.0f;
}

} // namespace

juce::Result parseTestFile (const juce::String& text, TestSpec& spec)
{
    juce::var root;
    const auto parsed = juce::JSON::parse (text, root);

    if (parsed.failed())
        return fail ("not valid JSON: " + parsed.getErrorMessage());

    if (! root.isObject())
        return fail ("the file must hold one JSON object, with \"plugins\" and \"tests\"");

    spec = {};
    spec.source = root;
    spec.name = root.getProperty ("name", "Untitled").toString();

    if (root.hasProperty ("sampleRate"))
    {
        spec.sampleRate = (double) root["sampleRate"];

        if (spec.sampleRate < 8000.0 || spec.sampleRate > 384000.0)
            return fail ("sampleRate must be between 8000 and 384000");
    }

    if (root.hasProperty ("blockSizes"))
    {
        const auto* sizes = root["blockSizes"].getArray();

        if (sizes == nullptr || sizes->isEmpty())
            return fail ("blockSizes must be a list of numbers, such as [128]");

        spec.blockSizes.clear();

        for (const auto& size : *sizes)
        {
            const auto value = (int) size;

            if (value < 1 || value > 8192)
                return fail ("each block size must be between 1 and 8192");

            spec.blockSizes.push_back (value);
        }
    }

    if (root.hasProperty ("tempo"))
    {
        spec.tempo = (double) root["tempo"];

        if (spec.tempo < 20.0 || spec.tempo > 999.0)
            return fail ("tempo must be between 20 and 999");
    }

    const auto* plugins = root["plugins"].getArray();

    if (plugins == nullptr || plugins->isEmpty())
        return fail ("\"plugins\" must be a list with at least one plugin");

    for (const auto& item : *plugins)
    {
        if (! item.isObject())
            return fail ("each entry in \"plugins\" must be an object with a \"path\"");

        if ((bool) item.getProperty ("disabled", false))
            continue;

        PluginEntry entry;
        entry.path = item["path"].toString();

        if (entry.path.isEmpty())
            return fail ("a plugin has no \"path\"");

        const auto fileName = expandPath (entry.path).fromLastOccurrenceOf ("\\", false, false);
        entry.label = item.getProperty ("label", fileName.containsChar ('.') ? fileName.upToLastOccurrenceOf (".", false, false) : fileName).toString();
        entry.name = item["name"].toString();
        entry.preset = item["preset"].toString();
        entry.todo = item["todo"].toString();
        entry.params = item["params"];

        if (! entry.params.isVoid() && ! entry.params.isObject())
            return fail (entry.label + ": \"params\" must be an object, such as { \"Ratio\": \"4\" }");

        spec.plugins.push_back (entry);
    }

    if (spec.plugins.empty())
        return fail ("every plugin is disabled");

    spec.tests = root["tests"];

    if (auto* tests = spec.tests.getDynamicObject())
    {
        for (const auto& property : tests->getProperties())
            if (! knownTests.contains (property.name.toString()))
                return fail ("unknown test \"" + property.name.toString() + "\"; the tests are " + knownTests.joinIntoString (", "));

        if (tests->getProperties().isEmpty())
            return fail ("\"tests\" is empty");
    }
    else
    {
        return fail ("\"tests\" must be an object, such as { \"cpu\": {}, \"latency\": {} }");
    }

    if (spec.has ("clicks"))
    {
        const auto clicks = spec.options ("clicks");

        if (clicks["param"].toString().isEmpty() || clicks["from"].isVoid() || clicks["to"].isVoid())
            return fail ("the clicks test needs \"param\", \"from\" and \"to\"");
    }

    return juce::Result::ok();
}

juce::Result loadTestFile (const juce::File& file, TestSpec& spec)
{
    if (! file.existsAsFile())
        return fail ("can't find " + file.getFullPathName());

    return parseTestFile (file.loadFileAsString(), spec);
}

juce::String expandPath (const juce::String& path)
{
    juce::String result;
    auto rest = path;

    while (rest.isNotEmpty())
    {
        const auto start = rest.indexOfChar ('%');
        const auto end = start < 0 ? -1 : rest.indexOfChar (start + 1, '%');

        if (start < 0 || end < 0)
        {
            result << rest;
            break;
        }

        const auto name = rest.substring (start + 1, end);
        auto value = juce::SystemStats::getEnvironmentVariable (name, {});

        if (value.isEmpty() && name.equalsIgnoreCase ("FANCY_VST3"))
            value = juce::SystemStats::getEnvironmentVariable ("USERPROFILE", {}) + "\\VST3-Dev";

        result << rest.substring (0, start) << (value.isNotEmpty() ? value : rest.substring (start, end + 1));
        rest = rest.substring (end + 1);
    }

    return result.replaceCharacter ('/', '\\');
}

bool parseDisplayNumber (const juce::String& text, double& value)
{
    // Normalise the minus sign the suite and some plugins display.
    auto t = text.trim().toLowerCase().replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-");

    if (t.startsWith ("-inf") || t.startsWith ("-" + juce::String::fromUTF8 ("\xe2\x88\x9e")))
    {
        value = -std::numeric_limits<double>::infinity();
        return true;
    }

    if (t.startsWith ("inf") || t.startsWith ("+inf"))
    {
        value = std::numeric_limits<double>::infinity();
        return true;
    }

    int end = 0;

    if (end < t.length() && (t[end] == '-' || t[end] == '+'))
        ++end;

    const auto digitsStart = end;
    auto seenDigit = false;

    while (end < t.length() && (juce::CharacterFunctions::isDigit (t[end]) || t[end] == '.'))
    {
        seenDigit = seenDigit || juce::CharacterFunctions::isDigit (t[end]);
        ++end;
    }

    if (! seenDigit || end == digitsStart)
        return false;

    value = t.substring (0, end).getDoubleValue();

    const auto suffix = t.substring (end).trimStart();

    if (suffix.startsWith ("k") && ! suffix.startsWith ("kn")) // "2.5k", "2.50 khz", not "knee"
        value *= 1000.0;

    return true;
}

juce::AudioProcessorParameter* findParameter (juce::AudioProcessor& processor, const juce::String& nameOrIndex, juce::String& error)
{
    const auto& parameters = processor.getParameters();
    const auto wanted = nameOrIndex.trim();

    if (wanted.startsWithChar ('#'))
    {
        const auto index = wanted.substring (1).getIntValue();

        if (wanted.length() > 1 && wanted.substring (1).containsOnly ("0123456789") && juce::isPositiveAndBelow (index, parameters.size()))
            return parameters[index];

        error = "no parameter " + wanted + "; this plugin has #0 to #" + juce::String (parameters.size() - 1);
        return nullptr;
    }

    for (auto* parameter : parameters)
        if (nameOf (*parameter).equalsIgnoreCase (wanted))
            return parameter;

    // Suggestions: names containing the text first, then the nearest by edit distance.
    juce::StringArray containing;
    std::vector<std::pair<int, juce::String>> nearest;

    for (auto* parameter : parameters)
    {
        const auto name = nameOf (*parameter);

        if (name.containsIgnoreCase (wanted) || wanted.containsIgnoreCase (name))
            containing.addIfNotAlreadyThere (name);

        nearest.emplace_back (editDistance (name, wanted), name);
    }

    std::stable_sort (nearest.begin(), nearest.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& [distance, name] : nearest)
    {
        if (containing.size() >= 5)
            break;

        containing.addIfNotAlreadyThere (name);
    }

    containing.removeRange (5, containing.size());
    error = "no parameter called \"" + wanted + "\". Closest: " + containing.joinIntoString (", ")
          + ". Run FancyBench params to list them all";
    return nullptr;
}

juce::Result resolveValue (juce::AudioProcessorParameter& parameter, const juce::String& requestedText, ParamSetting& setting)
{
    const auto requested = requestedText.trim();
    setting = {};
    setting.parameter = &parameter;
    setting.name = nameOf (parameter);
    setting.requested = requested;

    const auto show = [&parameter] (float value) { return parameter.getText (value, 64).trim(); };

    if (requested.startsWithChar ('='))
    {
        setting.normalised = juce::jlimit (0.0f, 1.0f, requested.substring (1).getFloatValue());
        setting.shown = show (setting.normalised);
        return juce::Result::ok();
    }

    setting.normalised = juce::jlimit (0.0f, 1.0f, parameter.getValueForText (requested));
    setting.shown = show (setting.normalised);

    double target = 0.0, shownValue = 0.0;
    const auto numeric = parseDisplayNumber (requested, target);

    if (numeric && parseDisplayNumber (setting.shown, shownValue) && closeEnough (target, shownValue))
        return juce::Result::ok();

    if (! numeric && setting.shown.equalsIgnoreCase (requested))
        return juce::Result::ok();

    // The plugin didn't parse it: search its range for the display that matches.
    const auto points = samplePoints (parameter);

    if (! numeric)
    {
        for (const auto exact : { true, false })
        {
            for (int i = 0; i < points; ++i)
            {
                const auto value = pointValue (i, points);
                const auto text = show (value);

                if (exact ? text.equalsIgnoreCase (requested) : text.startsWithIgnoreCase (requested))
                {
                    setting.normalised = value;
                    setting.shown = text;
                    setting.note = "found by search";
                    return juce::Result::ok();
                }
            }
        }

        return fail (setting.name + " doesn't take \"" + requested + "\" (it shows \"" + setting.shown + "\")");
    }

    auto bestIndex = -1;
    auto bestDistance = std::numeric_limits<double>::max();
    std::vector<double> values ((size_t) points, std::numeric_limits<double>::quiet_NaN());

    for (int i = 0; i < points; ++i)
    {
        double value = 0.0;

        if (! parseDisplayNumber (show (pointValue (i, points)), value))
            continue;

        values[(size_t) i] = value;
        const auto distance = std::isinf (target) || std::isinf (value) ? (target == value ? 0.0 : std::numeric_limits<double>::max() / 2)
                                                                       : std::abs (value - target);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    if (bestIndex < 0)
        return fail (setting.name + " doesn't show numbers, so \"" + requested + "\" can't be matched (it shows \"" + setting.shown + "\")");

    auto best = pointValue (bestIndex, points);

    // Refine between the neighbours that bracket the target, for continuous parameters.
    if (points == 1001 && ! std::isinf (target) && bestDistance > 0.0)
    {
        for (const auto other : { bestIndex - 1, bestIndex + 1 })
        {
            if (! juce::isPositiveAndBelow (other, points) || std::isnan (values[(size_t) other]))
                continue;

            auto low = pointValue (std::min (bestIndex, other), points);
            auto high = pointValue (std::max (bestIndex, other), points);
            double lowValue = 0.0, highValue = 0.0;
            parseDisplayNumber (show (low), lowValue);
            parseDisplayNumber (show (high), highValue);

            if ((lowValue - target) * (highValue - target) > 0.0)
                continue;

            for (int i = 0; i < 30; ++i)
            {
                const auto middle = 0.5f * (low + high);
                double middleValue = 0.0;
                parseDisplayNumber (show (middle), middleValue);

                if ((middleValue - target) * (lowValue - target) > 0.0)
                {
                    low = middle;
                    lowValue = middleValue;
                }
                else
                {
                    high = middle;
                    highValue = middleValue;
                }
            }

            best = std::abs (lowValue - target) <= std::abs (highValue - target) ? low : high;
            break;
        }
    }

    setting.normalised = best;
    setting.shown = show (best);

    double finalValue = 0.0;
    parseDisplayNumber (setting.shown, finalValue);
    setting.note = closeEnough (target, finalValue) ? "found by search" : "the closest this plugin offers";
    return juce::Result::ok();
}

juce::Result applyParams (juce::AudioProcessor& processor, const juce::var& params, std::vector<ParamSetting>& settings)
{
    auto* object = params.getDynamicObject();

    if (object == nullptr)
        return juce::Result::ok();

    for (const auto& property : object->getProperties())
    {
        juce::String error;
        auto* parameter = findParameter (processor, property.name.toString(), error);

        if (parameter == nullptr)
            return fail (error);

        ParamSetting setting;
        const auto resolved = resolveValue (*parameter, property.value.toString(), setting);

        if (resolved.failed())
            return resolved;

        parameter->setValue (setting.normalised);
        settings.push_back (setting);
    }

    return juce::Result::ok();
}

} // namespace bench
