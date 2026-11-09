#include "Report.h"

#include "Analysis.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace bench
{

namespace
{

// The dark categorical palette, checked for colour-blind separation on this surface with the
// dataviz validator; slots after the third are also dashed, so identity never rests on colour.
const juce::Colour surface (0xff262626);
const juce::Colour grid (0xff383838);
const juce::Colour textPrimary (0xffffffff);
const juce::Colour textSecondary (0xff8d8d8d);
const juce::Colour palette[] { juce::Colour (0xff3987e5), juce::Colour (0xffd95926), juce::Colour (0xff199e70), juce::Colour (0xffc98500),
                               juce::Colour (0xffd55181), juce::Colour (0xff008300), juce::Colour (0xff9085e9), juce::Colour (0xffe66767) };

juce::Colour colourFor (int slot) { return palette[slot % 8]; }

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

float textWidth (const juce::Font& f, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (f, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox (0, -1, true).getWidth();
}

juce::String number (double value, int decimals)
{
    if (value <= floorDb + 1.0)
        return "-inf";

    return juce::String (value, decimals);
}

juce::String hzText (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 || std::fmod (hz, 1000.0) == 0.0 ? 0 : 1) + "k";

    return juce::String (juce::roundToInt (hz));
}

std::vector<double> linearTicks (double low, double high)
{
    const auto raw = (high - low) / 8.0;
    const auto magnitude = std::pow (10.0, std::floor (std::log10 (std::max (raw, 1.0e-9))));
    auto step = magnitude;

    for (const auto multiple : { 1.0, 2.0, 5.0, 10.0 })
    {
        step = multiple * magnitude;

        if (step >= raw)
            break;
    }

    std::vector<double> ticks;

    for (auto tick = std::ceil (low / step) * step; tick <= high + 1.0e-9; tick += step)
        ticks.push_back (std::abs (tick) < 1.0e-9 ? 0.0 : tick);

    return ticks;
}

// The smallest 1, 2, 2.5 or 5 times a power of ten at or above the value.
double niceCeiling (double value)
{
    if (value <= 0.0)
        return 1.0;

    const auto magnitude = std::pow (10.0, std::floor (std::log10 (value)));

    for (const auto multiple : { 1.0, 2.0, 2.5, 5.0, 10.0 })
        if (multiple * magnitude >= value)
            return multiple * magnitude;

    return 10.0 * magnitude;
}

std::vector<double> logTicks (double low, double high)
{
    std::vector<double> ticks;

    for (auto decade = 1.0; decade <= high; decade *= 10.0)
        for (const auto multiple : { 1.0, 2.0, 5.0 })
            if (const auto tick = decade * multiple; tick >= low - 1.0e-9 && tick <= high + 1.0e-9)
                ticks.push_back (tick);

    return ticks;
}

void strokeSeries (juce::Graphics& g, const juce::Path& path, int slot)
{
    g.setColour (colourFor (slot));

    if (slot < 3)
    {
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    const float dashes[] { 8.0f, 5.0f };
    juce::Path dashed;
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, path, dashes, 2);
    g.fillPath (dashed);
}

void drawLegend (juce::Graphics& g, const std::vector<juce::String>& names, float x, float y)
{
    if (names.size() < 2)
        return;

    const auto f = font (13.0f);
    g.setFont (f);

    for (size_t i = 0; i < names.size(); ++i)
    {
        juce::Path swatch;
        swatch.startNewSubPath (x, y + 8.0f);
        swatch.lineTo (x + 24.0f, y + 8.0f);
        strokeSeries (g, swatch, (int) i);

        g.setColour (textPrimary);
        const auto width = textWidth (f, names[i]);
        g.drawText (names[i], juce::Rectangle<float> (x + 30.0f, y, width + 4.0f, 16.0f), juce::Justification::centredLeft, false);
        x += 30.0f + width + 28.0f;
    }
}

// Report building ---------------------------------------------------------------------------

struct Writer
{
    juce::File folder;
    juce::String markdown;
    const std::vector<juce::var>& results;
    const juce::var& reference;

    void line (const juce::String& text = {}) { markdown << text << "\n"; }

    void csv (const juce::String& name, const juce::StringArray& rows)
    {
        folder.getChildFile (name).replaceWithText (rows.joinIntoString ("\n") + "\n");
    }

    void image (const juce::String& name, const juce::Image& picture, const juce::String& alt)
    {
        if (savePng (picture, folder.getChildFile (name)))
            line ("![" + alt + "](" + name + ")\n");
    }

    std::vector<std::pair<int, const juce::var*>> withTest (const char* test) const
    {
        std::vector<std::pair<int, const juce::var*>> list;

        for (size_t i = 0; i < results.size(); ++i)
            if (results[i].hasProperty (test))
                list.emplace_back ((int) i, &results[i]);

        return list;
    }

    static juce::String label (const juce::var& result) { return result["label"].toString(); }
};

void plugins (Writer& w)
{
    w.line ("## Plugins\n");
    w.line ("| Plugin | Status | Loaded as | Settings, as the plugin shows them |");
    w.line ("|---|---|---|---|");

    for (const auto& result : w.results)
    {
        const auto status = result["status"].toString();
        juce::String loaded, settings;

        if (result.hasProperty ("plugin"))
            loaded = result["plugin"].toString() + " " + result["version"].toString() + " (" + result["vendor"].toString() + ")";

        if (const auto* list = result["settings"].getArray())
        {
            juce::StringArray parts;

            for (const auto& setting : *list)
            {
                auto part = setting["param"].toString() + " = " + setting["shown"].toString();

                if (setting.hasProperty ("note"))
                    part << " (asked " << setting["requested"].toString() << ", " << setting["note"].toString() << ")";

                parts.add (part);
            }

            settings = parts.isEmpty() ? "defaults" : parts.joinIntoString ("; ");
        }

        auto statusText = status == "ok" ? juce::String ("measured") : status;

        if (result.hasProperty ("reason"))
            statusText << ": " << result["reason"].toString();

        w.line ("| " + Writer::label (result) + " | " + statusText.replace ("|", "/") + " | " + loaded + " | " + settings.replace ("|", "/") + " |");
    }

    w.line();
}

double referenceMean (const juce::var& reference, int block)
{
    if (const auto* rows = reference["cpu"].getArray())
        for (const auto& row : *rows)
            if ((int) row["block"] == block)
                return (double) row["mean"];

    return 0.0;
}

void cpu (Writer& w)
{
    const auto list = w.withTest ("cpu");

    if (list.empty())
        return;

    w.line ("## CPU\n");
    w.line ("Percent of one core: the time spent in the plugin's process call against the audio's duration, at time-critical priority. "
            "\"All threads\" is the whole process's CPU time over the same audio, which includes background threads. "
            "Mean and 99th percentile are the median over the runs; worst block is the worst in any run.\n");

    const auto hasReference = w.reference.isObject() && w.reference.hasProperty ("cpu");

    if (hasReference)
        w.line ("The last column is a ratio to Fancy Comp at its defaults, timed the same way in this session, which compares across PCs and with CI.\n");

    w.line (juce::String ("| Plugin | Block | Mean % | 99th percentile % | Worst block % | All threads % |") + (hasReference ? " × Fancy Comp |" : ""));
    w.line (juce::String ("|---|---|---|---|---|---|") + (hasReference ? "---|" : ""));

    juce::StringArray rows { "plugin,block,mean_percent,p99_percent,worst_percent,all_threads_percent,ratio_to_fancy_comp" };
    std::vector<std::pair<juce::String, std::pair<double, int>>> bars;

    const auto add = [&] (const juce::var& result, int slot)
    {
        for (const auto& row : *result["cpu"].getArray())
        {
            const auto block = (int) row["block"];
            const auto mean = (double) row["mean"];
            const auto ratio = hasReference && referenceMean (w.reference, block) > 0.0 ? mean / referenceMean (w.reference, block) : 0.0;

            w.line ("| " + Writer::label (result) + " | " + juce::String (block) + " | " + juce::String (mean, 3) + " | "
                    + juce::String ((double) row["p99"], 3) + " | " + juce::String ((double) row["worst"], 2) + " | "
                    + juce::String ((double) row["allThreads"], 3) + " |" + (hasReference ? " " + juce::String (ratio, 2) + " |" : ""));

            rows.add (Writer::label (result).quoted() + "," + juce::String (block) + "," + row["mean"].toString() + "," + row["p99"].toString() + ","
                      + row["worst"].toString() + "," + row["allThreads"].toString() + "," + juce::String (ratio, 3));

            bars.push_back ({ Writer::label (result) + ", " + juce::String (block), { mean, slot } });
        }
    };

    for (const auto& [slot, result] : list)
        add (*result, slot);

    if (hasReference)
        add (w.reference, (int) w.results.size());

    w.line();
    w.csv ("cpu.csv", rows);

    // Horizontal bars, one per plugin and block size.
    const auto rowHeight = 34;
    const auto width = 1200, height = 110 + rowHeight * (int) bars.size();
    juce::Image picture (juce::Image::ARGB, width, height, true);
    juce::Graphics g (picture);
    g.fillAll (surface);
    g.setColour (textPrimary);
    g.setFont (font (18.0f, true));
    g.drawText ("CPU, mean % of one core", 24, 16, width - 48, 26, juce::Justification::centredLeft);

    auto top = 0.0;

    for (const auto& bar : bars)
        top = std::max (top, bar.second.first);

    top = niceCeiling (top * 1.15);

    const juce::Rectangle<float> area (300.0f, 60.0f, width - 360.0f, (float) (rowHeight * (int) bars.size()));

    g.setFont (font (12.0f));

    for (const auto tick : linearTicks (0.0, top))
    {
        const auto x = area.getX() + (float) (tick / top) * area.getWidth();
        g.setColour (grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (textSecondary);
        g.drawText (juce::String (tick, tick < 1.0 ? 2 : 1), juce::Rectangle<float> (x - 40.0f, area.getBottom() + 6.0f, 80.0f, 16.0f), juce::Justification::centred);
    }

    for (size_t i = 0; i < bars.size(); ++i)
    {
        const auto y = area.getY() + (float) i * rowHeight;
        const auto value = bars[i].second.first;
        const auto length = (float) (value / top) * area.getWidth();

        g.setColour (textPrimary);
        g.setFont (font (13.0f));
        g.drawText (bars[i].first, juce::Rectangle<float> (24.0f, y, area.getX() - 36.0f, (float) rowHeight), juce::Justification::centredRight, true);

        g.setColour (colourFor (bars[i].second.second));
        g.fillRoundedRectangle (juce::Rectangle<float> (area.getX(), y + 8.0f, std::max (2.0f, length), rowHeight - 16.0f), 4.0f);
        g.fillRect (juce::Rectangle<float> (area.getX(), y + 8.0f, std::min (4.0f, std::max (2.0f, length)), rowHeight - 16.0f));

        g.setColour (textPrimary);
        g.drawText (juce::String (value, 3) + " %", juce::Rectangle<float> (area.getX() + length + 8.0f, y, 100.0f, (float) rowHeight),
                    juce::Justification::centredLeft);
    }

    g.setColour (textSecondary);
    g.drawText ("% of one core", juce::Rectangle<float> (area.getX(), area.getBottom() + 24.0f, area.getWidth(), 16.0f), juce::Justification::centred);

    w.image ("cpu.png", picture, "CPU, mean percent of one core");
}

void latency (Writer& w)
{
    const auto list = w.withTest ("latency");

    if (list.empty())
        return;

    w.line ("## Latency\n");
    w.line ("The latency the plugin reports, and the delay measured from the peak of the input-output cross-correlation, on white noise. "
            "Through minimum-phase filtering the peak can land a few samples after the reported latency; a large difference means the "
            "host would misalign the plugin.\n");
    w.line ("| Plugin | Reported (samples) | Measured (samples) | Difference |");
    w.line ("|---|---|---|---|");

    juce::StringArray rows { "plugin,reported,measured,difference" };

    for (const auto& [slot, result] : list)
    {
        const auto& row = (*result)["latency"];
        const auto silent = (bool) row.getProperty ("silent", false);
        const auto measured = silent ? juce::String ("no output") : row["measured"].toString();
        const auto difference = silent ? juce::String ("-") : row["difference"].toString();

        w.line ("| " + Writer::label (*result) + " | " + row["reported"].toString() + " | " + measured + " | " + difference + " |");
        rows.add (Writer::label (*result).quoted() + "," + row["reported"].toString() + "," + (silent ? "" : measured) + "," + (silent ? "" : difference));
    }

    w.line();
    w.csv ("latency.csv", rows);
}

void response (Writer& w, const TestSpec& spec)
{
    const auto list = w.withTest ("response");

    if (list.empty())
        return;

    const auto level = (double) list.front().second->getProperty ("response", {}).getProperty ("level", -20.0);
    w.line ("## Frequency response\n");
    w.line ("From a 10 s exponential sweep at " + juce::String (level, 1) + " dBFS, with the reported latency removed, at 1/24-octave points. "
            "A level-dependent plugin, such as a compressor, gives the response at that level.\n");

    std::vector<Series> magnitude, phase;
    std::vector<juce::String> names;
    std::map<double, juce::StringArray> table;
    juce::StringArray header { "hz" };

    double low = 0.0, high = 0.0;
    auto first = true;

    for (const auto& [slot, result] : list)
    {
        Series m { Writer::label (*result), {} }, p { Writer::label (*result), {} };
        header.add (Writer::label (*result).quoted() + " dB");
        header.add (Writer::label (*result).quoted() + " degrees");

        for (const auto& point : *(*result)["response"]["points"].getArray())
        {
            const auto hz = (double) point[0], db = (double) point[1], degrees = (double) point[2];
            m.points.emplace_back (hz, db);
            p.points.emplace_back (hz, degrees);
            table[hz].add (juce::String (db, 3));
            table[hz].add (juce::String (degrees, 2));

            if (db > floorDb + 1.0)
            {
                low = first ? db : std::min (low, db);
                high = first ? db : std::max (high, db);
                first = false;
            }
        }

        magnitude.push_back (m);
        phase.push_back (p);
        names.push_back (m.name);
    }

    juce::StringArray rows { header.joinIntoString (",") };

    for (const auto& [hz, values] : table)
        rows.add (juce::String (hz, 2) + "," + values.joinIntoString (","));

    w.csv ("response.csv", rows);

    const auto highHz = std::min (20000.0, 0.45 * spec.sampleRate);
    const auto bottom = std::floor (std::min (low, -3.0) / 3.0) * 3.0 - 3.0;
    const auto top = std::ceil (std::max (high, 3.0) / 3.0) * 3.0 + 3.0;

    w.image ("response.png", drawPlot ({ "Frequency response", "Frequency (Hz)", "Level (dB)", true, 20.0, highHz, bottom, top }, magnitude),
             "Frequency response");
    w.image ("response-phase.png", drawPlot ({ "Phase, with the reported latency removed", "Frequency (Hz)", "Phase (degrees)", true, 20.0, highHz, -180.0, 180.0 }, phase),
             "Phase response");

    // A short table at round frequencies.
    w.line ("| Plugin | 50 Hz | 100 Hz | 1 kHz | 5 kHz | 10 kHz | 20 kHz |");
    w.line ("|---|---|---|---|---|---|---|");

    for (const auto& series : magnitude)
    {
        juce::String row = "| " + series.name + " |";

        for (const auto hz : { 50.0, 100.0, 1000.0, 5000.0, 10000.0, 20000.0 })
        {
            const auto nearest = std::min_element (series.points.begin(), series.points.end(), [hz] (const auto& a, const auto& b)
                                                   { return std::abs (std::log (a.first / hz)) < std::abs (std::log (b.first / hz)); });
            row << " " << (nearest != series.points.end() && std::abs (std::log (nearest->first / hz)) < 0.03 ? number (nearest->second, 2) + " dB" : juce::String ("-")) << " |";
        }

        w.line (row);
    }

    w.line();
}

void thd (Writer& w)
{
    const auto list = w.withTest ("thd");

    if (list.empty())
        return;

    w.line ("## THD and THD+N\n");
    w.line ("A sine at each frequency and level, 1 s settled, then analysed with a Blackman-Harris window over about 1.4 s. "
            "THD counts harmonics 2 to 10 below Nyquist; THD+N everything but the fundamental from 20 Hz to 20 kHz. "
            "Frequencies are moved to the nearest analysis bin.\n");
    w.line ("| Plugin | Frequency | Level in | Level out | THD % | THD dB | THD+N % | THD+N dB |");
    w.line ("|---|---|---|---|---|---|---|---|");

    juce::StringArray rows { "plugin,hz,actual_hz,level_dbfs,output_dbfs,thd_percent,thd_db,thdn_percent,thdn_db" };
    std::map<double, std::vector<Series>> byFrequency;

    for (const auto& [slot, result] : list)
    {
        std::map<double, Series> series;

        for (const auto& row : *(*result)["thd"].getArray())
        {
            const auto hz = (double) row["hz"];
            w.line ("| " + Writer::label (*result) + " | " + hzText (hz) + " Hz | " + number ((double) row["level"], 1) + " dBFS | "
                    + number ((double) row["outputDbfs"], 1) + " dBFS | " + juce::String ((double) row["thdPercent"], 5) + " | "
                    + number ((double) row["thdDb"], 1) + " | " + juce::String ((double) row["thdnPercent"], 5) + " | " + number ((double) row["thdnDb"], 1) + " |");

            rows.add (Writer::label (*result).quoted() + "," + row["hz"].toString() + "," + row["actualHz"].toString() + "," + row["level"].toString() + ","
                      + row["outputDbfs"].toString() + "," + row["thdPercent"].toString() + "," + row["thdDb"].toString() + ","
                      + row["thdnPercent"].toString() + "," + row["thdnDb"].toString());

            series[hz].name = Writer::label (*result);
            series[hz].points.emplace_back ((double) row["level"], std::max (-200.0, (double) row["thdDb"]));
        }

        for (auto& [hz, s] : series)
            byFrequency[hz].push_back (s);
    }

    w.line();
    w.csv ("thd.csv", rows);

    for (const auto& [hz, series] : byFrequency)
    {
        double low = 0.0, high = -200.0, left = 0.0, right = -200.0;

        for (const auto& s : series)
            for (const auto& [x, y] : s.points)
            {
                low = std::min (low, y);
                high = std::max (high, y);
                left = std::min (left, x);
                right = std::max (right, x);
            }

        const auto bottom = std::floor (low / 20.0) * 20.0 - 10.0;
        const auto top = std::max (bottom + 40.0, std::ceil (high / 20.0) * 20.0 + 10.0);

        w.image ("thd-" + juce::String (juce::roundToInt (hz)) + ".png",
                 drawPlot ({ "THD at " + hzText (hz) + " Hz against input level", "Input level (dBFS)", "THD (dB)", false, left - 2.0, right + 2.0, bottom, top }, series),
                 "THD at " + hzText (hz) + " Hz");
    }
}

void aliasingSection (Writer& w, const TestSpec& spec)
{
    const auto list = w.withTest ("aliasing");

    if (list.empty())
        return;

    w.line ("## Aliasing\n");
    w.line ("A sine at each frequency. An alias is any component more than 30 dB over the noise floor that isn't a harmonic below Nyquist "
            "or near DC. Relative levels are against the output's fundamental.\n");
    w.line ("| Plugin | Frequency | Level | Aliases | Loudest alias | at | Relative | All aliases, relative |");
    w.line ("|---|---|---|---|---|---|---|---|");

    juce::StringArray rows { "plugin,hz,level_dbfs,aliases,worst_hz,worst_dbfs,worst_relative_db,total_relative_db" };
    std::map<double, std::vector<Series>> byFrequency;

    for (const auto& [slot, result] : list)
    {
        for (const auto& row : *(*result)["aliasing"].getArray())
        {
            const auto hz = (double) row["hz"];
            const auto count = (int) row["aliases"];

            w.line ("| " + Writer::label (*result) + " | " + hzText (hz) + " Hz | " + number ((double) row["level"], 1) + " dBFS | " + juce::String (count) + " | "
                    + (count > 0 ? number ((double) row["worstDbfs"], 1) + " dBFS | " + hzText ((double) row["worstHz"]) + " Hz | "
                                       + number ((double) row["worstRelativeDb"], 1) + " dB | " + number ((double) row["totalRelativeDb"], 1) + " dB |"
                                 : juce::String ("none | - | - | - |")));

            rows.add (Writer::label (*result).quoted() + "," + row["hz"].toString() + "," + row["level"].toString() + "," + juce::String (count) + ","
                      + row["worstHz"].toString() + "," + row["worstDbfs"].toString() + "," + row["worstRelativeDb"].toString() + ","
                      + row["totalRelativeDb"].toString());

            Series s { Writer::label (*result), {} };
            const auto step = (double) row["spectrumStepHz"];
            int index = 0;

            for (const auto& value : *row["spectrum"].getArray())
                s.points.emplace_back (step * index++, std::max (-200.0, (double) value));

            byFrequency[hz].push_back (s);
        }
    }

    w.line();
    w.csv ("aliasing.csv", rows);

    for (const auto& [hz, series] : byFrequency)
        w.image ("aliasing-" + juce::String (juce::roundToInt (hz)) + ".png",
                 drawPlot ({ "Spectrum of a " + hzText (hz) + " Hz sine", "Frequency (Hz)", "Level (dBFS)", false, 0.0, spec.sampleRate / 2.0, -180.0, 10.0 }, series),
                 "Spectrum at " + hzText (hz) + " Hz");
}

void nullSection (Writer& w)
{
    const auto list = w.withTest ("null");

    if (list.empty())
        return;

    w.line ("## Null test\n");
    w.line ("The output minus the input delayed by the reported latency, after 1 s. For plugins set to be neutral; below -120 dBFS reads as a null.\n");
    w.line ("| Plugin | Signal | Residual RMS | Residual peak | Relative to the input | Null? |");
    w.line ("|---|---|---|---|---|---|");

    juce::StringArray rows { "plugin,signal,rms_dbfs,peak_dbfs,relative_db" };

    for (const auto& [slot, result] : list)
    {
        for (const auto& row : *(*result)["null"].getArray())
        {
            const auto rms = (double) row["rmsDbfs"];
            w.line ("| " + Writer::label (*result) + " | " + row["signal"].toString() + " | " + number (rms, 1) + " dBFS | "
                    + number ((double) row["peakDbfs"], 1) + " dBFS | " + number ((double) row["relativeDb"], 1) + " dB | " + (rms < -120.0 ? "yes" : "no") + " |");
            rows.add (Writer::label (*result).quoted() + "," + row["signal"].toString().quoted() + "," + row["rmsDbfs"].toString() + ","
                      + row["peakDbfs"].toString() + "," + row["relativeDb"].toString());
        }
    }

    w.line();
    w.csv ("null.csv", rows);
}

void clicksSection (Writer& w)
{
    const auto list = w.withTest ("clicks");

    if (list.empty())
        return;

    w.line ("## Clicks under automation\n");
    w.line ("A 1 kHz sine at -12 dBFS while one parameter jumps between two values, in 32-sample blocks. The output has the sine notched out and "
            "is high-passed at 4 kHz; a burst is anything 6 dB over the same plugin's figure without automation (and over -60 dBFS). "
            "A click-free plugin reads about the same as its static figure.\n");
    w.line ("| Plugin | Parameter | From | To | Jumps | Static peak | Automated peak | Bursts |");
    w.line ("|---|---|---|---|---|---|---|---|");

    juce::StringArray rows { "plugin,param,from,to,jumps,static_peak_dbfs,peak_dbfs,bursts" };

    for (const auto& [slot, result] : list)
    {
        const auto& row = (*result)["clicks"];
        w.line ("| " + Writer::label (*result) + " | " + row["param"].toString() + " | " + row["from"].toString() + " | " + row["to"].toString() + " | "
                + row["jumps"].toString() + " | " + number ((double) row["staticPeakDbfs"], 1) + " dBFS | " + number ((double) row["peakDbfs"], 1)
                + " dBFS | " + row["count"].toString() + " |");
        rows.add (Writer::label (*result).quoted() + "," + row["param"].toString().quoted() + "," + row["from"].toString().quoted() + ","
                  + row["to"].toString().quoted() + "," + row["jumps"].toString() + "," + row["staticPeakDbfs"].toString() + ","
                  + row["peakDbfs"].toString() + "," + row["count"].toString());
    }

    w.line();
    w.csv ("clicks.csv", rows);
}

} // namespace

Machine thisMachine()
{
    Machine machine;
    machine.cpu = juce::SystemStats::getCpuModel().trim() + ", " + juce::String (juce::SystemStats::getNumCpus()) + " threads";
    machine.os = juce::SystemStats::getOperatingSystemName();
    machine.date = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M");
    return machine;
}

juce::Image drawPlot (const PlotSpec& spec, const std::vector<Series>& series)
{
    constexpr int width = 1200, height = 640;
    juce::Image picture (juce::Image::ARGB, width, height, true);
    juce::Graphics g (picture);
    g.fillAll (surface);

    g.setColour (textPrimary);
    g.setFont (font (18.0f, true));
    g.drawText (spec.title, 24, 14, width - 48, 26, juce::Justification::centredLeft);

    std::vector<juce::String> names;

    for (const auto& s : series)
        names.push_back (s.name);

    drawLegend (g, names, 24.0f, 46.0f);

    const juce::Rectangle<float> area (84.0f, 80.0f, width - 112.0f, height - 140.0f);

    const auto toX = [&] (double x)
    {
        const auto t = spec.logX ? std::log (std::max (x, spec.xMin * 0.5) / spec.xMin) / std::log (spec.xMax / spec.xMin)
                                 : (x - spec.xMin) / (spec.xMax - spec.xMin);
        return area.getX() + (float) t * area.getWidth();
    };

    const auto toY = [&] (double y)
    {
        const auto clamped = juce::jlimit (spec.yMin, spec.yMax, y);
        return area.getBottom() - (float) ((clamped - spec.yMin) / (spec.yMax - spec.yMin)) * area.getHeight();
    };

    g.setFont (font (12.0f));

    for (const auto tick : linearTicks (spec.yMin, spec.yMax))
    {
        const auto y = toY (tick);
        g.setColour (grid);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (textSecondary);
        g.drawText (juce::String (tick, std::abs (tick - std::round (tick)) < 1.0e-6 ? 0 : 1), juce::Rectangle<float> (area.getX() - 60.0f, y - 8.0f, 52.0f, 16.0f),
                    juce::Justification::centredRight);
    }

    for (const auto tick : spec.logX ? logTicks (spec.xMin, spec.xMax) : linearTicks (spec.xMin, spec.xMax))
    {
        const auto x = toX (tick);
        g.setColour (grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (textSecondary);
        g.drawText (spec.logX || spec.xMax >= 1000.0 ? hzText (tick) : juce::String (tick, std::abs (tick - std::round (tick)) < 1.0e-6 ? 0 : 1),
                    juce::Rectangle<float> (x - 40.0f, area.getBottom() + 6.0f, 80.0f, 16.0f), juce::Justification::centred);
    }

    g.setColour (textSecondary);
    g.drawText (spec.xLabel, juce::Rectangle<float> (area.getX(), area.getBottom() + 26.0f, area.getWidth(), 16.0f), juce::Justification::centred);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, 22.0f, area.getCentreY()));
        g.drawText (spec.yLabel, juce::Rectangle<float> (22.0f - 150.0f, area.getCentreY() - 8.0f, 300.0f, 16.0f), juce::Justification::centred);
    }

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (area.expanded (1.0f).toNearestInt());

        for (size_t i = 0; i < series.size(); ++i)
        {
            juce::Path path;
            auto started = false;

            for (const auto& [x, y] : series[i].points)
            {
                if (x < spec.xMin || x > spec.xMax || (spec.logX && x <= 0.0))
                    continue;

                const juce::Point<float> point (toX (x), toY (y));

                if (! started)
                    path.startNewSubPath (point);
                else
                    path.lineTo (point);

                started = true;
            }

            if (started)
                strokeSeries (g, path, (int) i);
        }
    }

    return picture;
}

bool savePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (! stream.openedOk())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream);
}

void writeReport (const juce::File& folder, const TestSpec& spec, const std::vector<juce::var>& results,
                  const juce::var& reference, const Machine& machine)
{
    folder.createDirectory();

    Writer w { folder, {}, results, reference };

    juce::StringArray blocks;

    for (const auto block : spec.blockSizes)
        blocks.add (juce::String (block));

    w.line ("# " + spec.name + "\n");
    w.line ("FancyBench, " + machine.date + ", on " + machine.cpu + ", " + machine.os + ". " + juce::String (spec.sampleRate / 1000.0, 1)
            + " kHz, block sizes " + blocks.joinIntoString (", ") + ", stereo, " + juce::String (spec.tempo, 0) + " BPM. The analysed output is the left channel. "
            "The test file as run is `bench.json`.\n");

    plugins (w);
    cpu (w);
    latency (w);
    response (w, spec);
    thd (w);
    aliasingSection (w, spec);
    nullSection (w);
    clicksSection (w);

    folder.getChildFile ("report.md").replaceWithText (w.markdown);

    // The test file as run, with each path as found and the machine it ran on.
    auto run = spec.source.clone();

    if (auto* object = run.getDynamicObject())
    {
        object->setProperty ("machine", machine.cpu + "; " + machine.os);
        object->setProperty ("date", machine.date);

        juce::Array<juce::var> paths;

        for (const auto& result : results)
            paths.add (result["path"]);

        object->setProperty ("resolvedPaths", paths);
    }

    folder.getChildFile ("bench.json").replaceWithText (juce::JSON::toString (run));
}

} // namespace bench
