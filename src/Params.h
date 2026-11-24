// Every control of the faceplate as a host-automatable parameter. IDs, ranges, knob laws and defaults are the mockup's.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "purrvc/VoiceDsp.h"

namespace pvc {
using APVTS = juce::AudioProcessorValueTreeState;

inline juce::NormalisableRange<float> linR (float a, float b, float step = 0.0f) { return { a, b, step }; }
inline juce::NormalisableRange<float> logR (float a, float b)
{
    return { a, b, [] (float s, float e, float n) { return s * std::pow (e / s, n); },
                   [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                   [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}
// Output knob: piecewise law through (value, position) points, like the mockup (-inf at the left, 0 dB in the centre)
inline juce::NormalisableRange<float> outR()
{
    static const float P[7][2] = { { -60, 0 }, { -10, 0.25f }, { -2, 0.4f }, { 0, 0.5f }, { 2, 0.6f }, { 10, 0.75f }, { 20, 1 } };
    return { -60.0f, 20.0f,
        [] (float, float, float n) { for (int i = 1; i < 7; ++i) if (n <= P[i][1]) return P[i - 1][0] + (n - P[i - 1][1]) / (P[i][1] - P[i - 1][1]) * (P[i][0] - P[i - 1][0]); return 20.0f; },
        [] (float, float, float v) { for (int i = 1; i < 7; ++i) if (v <= P[i][0]) return P[i - 1][1] + (v - P[i - 1][0]) / (P[i][0] - P[i - 1][0]) * (P[i][1] - P[i - 1][1]); return 1.0f; },
        [] (float, float, float v) { return juce::jlimit (-60.0f, 20.0f, std::round (v * 10.0f) / 10.0f); } };
}

inline juce::String fmtDb (float v) { return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB"; }
inline juce::String fmtHz (float v) { return v >= 1000 ? juce::String (v / 1000, v >= 10000 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
inline juce::String fmtMs (float v) { return v >= 1000 ? juce::String (v / 1000, 2) + " s" : v < 1 ? juce::String (std::round (v * 100) / 100) + " ms" : (v < 9.95f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms"; }
inline juce::String fmtPct (float v) { return juce::String (juce::roundToInt (v)) + " %"; }

// value text exactly as the faceplate shows it
inline juce::String formatValue (const juce::String& id, float v)
{
    if (id == "gpos") return "+" + juce::String (0.6f * v, 1) + " dB";
    if (id == "imp") { const double z = purrvc::VoiceDsp::toneZ (v); return (v >= 1000 ? juce::String (v / 1000, 1) + "k" : juce::String (juce::roundToInt (v))) + " " + (z >= 0 ? "+" : "") + juce::String (3 * z, 1) + "dB"; }
    if (id == "tcol" || id == "tvoice" || id == "tvar" || id == "mix") return fmtPct (v);
    if (id == "thr" || id == "itrim" || id == "lg" || id == "m1g" || id == "m2g" || id == "hg") return fmtDb (v);
    if (id == "ratio") return v < 1.05f ? "OFF" : v >= 19.9f ? "20:1 limit" : juce::String (v, 1) + ":1";
    if (id == "att" || id == "rel" || id == "erel") return fmtMs (v);
    if (id == "makeup") return "+" + juce::String (v, 1) + " dB";
    if (id == "ethr") return v <= -79.5f ? "OFF" : fmtDb (v);
    if (id == "erange") return "-" + juce::String (juce::roundToInt (v)) + " dB";
    if (id == "ds") { const float d = v / 100, cp = d <= 0.25f ? 24 * d : d <= 0.75f ? 6 + 16 * (d - 0.25f) : 14 + 24 * (d - 0.75f); return v < 1 ? juce::String ("OFF") : "-" + juce::String (cp, 1) + " dB"; }
    if (id == "dsf" || id == "m1f" || id == "m2f") return fmtHz (v);
    if (id == "lq" || id == "m1q" || id == "m2q" || id == "hq") return "Q " + juce::String (v, 2);
    if (id == "out") return v <= -59.5f ? juce::String ("-inf") : fmtDb (v);
    return juce::String (v, 2);
}

struct KnobDef { const char* id; const char* name; juce::NormalisableRange<float> range; float def; const char* tip; };

inline std::vector<KnobDef> knobDefs()
{
    return {
        { "gpos", "Gain", linR (0, 100, 0.5f), 0, "Mic preamp gain, 0 to +60 dB. It drives the tube: more gain, more tube character, like the hardware." },
        { "imp", "Impedance", logR (150, 3400), 1200, "Input impedance as a tone control (our own modelling): low = darker and fuller, high = brighter and leaner." },
        { "tvoice", "Voicing", linR (0, 300, 1), 100, "How strong each tube's own character is. 100 % = the tube's base voicing, 0 % = neutral, above 100 % adds low-mid body and presence bite, 300 % = extreme." },
        { "tcol", "Colour", linR (0, 100, 1), 50, "Tube colour, from angelic to nasty, at any level. Up to about 50 % it adds warm even harmonics; above that more and more grit, and 100 % is heavy, fuzzy and aggressive." },
        { "tvar", "Variation", linR (0, 100, 1), 30, "How different the tubes sound from each other. 0 % = almost alike, 30 % = a clear difference, 100 % = five times the voicing difference (extreme)." },
        { "thr", "Threshold", linR (-64, -4, 0.5f), -20, "Compressor threshold." },
        { "ratio", "Ratio", logR (1, 20), 1, "Compression ratio. Fully left is off; 20:1 is a limiter." },
        { "ds", "De-ess amt", linR (0, 100, 1), 0, "De-esser amount. It follows the level of the S band automatically; depth stops OFF, -6, -14 and -20 dB as on the unit." },
        { "att", "Attack", logR (0.05f, 100), 10, "Compressor attack. The hardware range starts at 0.25 ms (dot on the scale)." },
        { "rel", "Release", logR (0.1f, 3000), 400, "Compressor release. HW range limits it to the hardware's 100 ms to 3 s." },
        { "dsf", "De-ess Hz", logR (2000, 15000), 6000, "De-esser frequency." },
        { "ethr", "Exp./gate thresh.", linR (-80, -10, 0.5f), -80, "Expander or gate threshold. Fully left is OFF, like the unit." },
        { "erange", "Range", linR (0, 80, 1), 60, "How far the expander or gate turns the signal down." },
        { "erel", "Release", logR (0.1f, 2000), 40, "Expander or gate release." },
        { "itrim", "Input trim", linR (-18, 18, 0.1f), 0, "Level into the dynamics block (plugin extra)." },
        { "makeup", "Makeup", linR (0, 24, 0.1f), 0, "Gain after the compressor (plugin extra). Auto makeup adds the measured amount." },
        { "mix", "Mix", linR (0, 100, 1), 100, "Parallel compression: blend of the dry and compressed signal (plugin extra)." },
        { "m1f", "Mid-1 Hz", logR (100, 3000), 400, "Mid-1 bell frequency." },
        { "m2f", "Mid-2 Hz", logR (500, 15000), 3500, "Mid-2 bell frequency." },
        { "lg", "Low", linR (-15, 15, 0.1f), 0, "Low shelf gain." },
        { "lq", "Low slope", logR (0.3f, 2), 0.7f, "Low shelf slope (plugin extra)." },
        { "m1g", "Mid-1", linR (-15, 15, 0.1f), 0, "Mid-1 bell gain." },
        { "m1q", "Mid-1 Q", logR (0.3f, 5), 0.9f, "Mid-1 bandwidth (plugin extra)." },
        { "m2g", "Mid-2", linR (-15, 15, 0.1f), 0, "Mid-2 bell gain." },
        { "m2q", "Mid-2 Q", logR (0.3f, 5), 0.9f, "Mid-2 bandwidth (plugin extra)." },
        { "hg", "High", linR (-15, 15, 0.1f), 0, "High shelf gain." },
        { "hq", "High slope", logR (0.3f, 2), 0.7f, "High shelf slope (plugin extra)." },
        { "out", "Output level", outR(), 0, "Gain after the processing. Fully left is off (-inf), the centre is 0 dB, fully right is +20 dB." },
    };
}
struct BoolDef { const char* id; const char* name; bool def; const char* tip; };
inline std::vector<BoolDef> boolDefs()
{
    return {
        { "pad", "Pad -20", false, "-20 dB before the preamp for loud sources." },
        { "ph", "Invert", false, "Polarity invert." },
        { "lc", "Low cut", false, "100 Hz low cut, 6 dB per octave." },
        { "tnoise", "Vintage noise", false, "Adds the tube's own noise floor (a rough guide from retailer ratings)." },
        { "arel", "Auto release", false, "Program dependent release: fast for short peaks, slow for sustained sound (plugin extra)." },
        { "hwr", "HW range", false, "Limits the release to the hardware's 100 ms to 3 s." },
        { "amk", "Auto makeup", false, "Adds makeup gain to match the compression (plugin extra)." },
        { "lim", "TP limiter", true, "True-peak safety limiter, ceiling -1.2 dBFS (plugin extra)." },
        { "power", "Power", true, "Off = bypass (the dry signal, latency matched)." },
        { "tvolt", "Tube voltage high", false, "NORMAL adds warmth with more tube saturation. HIGH gives more gain, headroom and bandwidth. The change takes about 10 to 20 seconds, like the hardware." },
        { "emode", "Gate mode", false, "Expand: a gentle downward expander, 1:1.5. Gate: fast open, auto hold, release." },
        { "eqpre", "EQ pre comp", true, "EQ before (pre comp) or after (post comp) the expander and compressor." },
        { "eqbyp", "EQ bypass", false, "Bypasses the EQ." },
        { "lf", "Low freq 150 Hz", true, "Low shelf at 50 or 150 Hz." },
        { "hf", "High freq 15 kHz", true, "High shelf at 5 or 15 kHz." },
    };
}

inline APVTS::ParameterLayout makeLayout()
{
    APVTS::ParameterLayout L;
    for (auto& k : knobDefs())
    {
        const juce::String id (k.id);
        L.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, k.name, k.range, k.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([id] (float v, int) { return formatValue (id, v); })));
    }
    for (auto& b : boolDefs()) L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { b.id, 1 }, b.name, b.def));
    juce::StringArray tubes; for (auto* n : purrvc::kTubeNames) tubes.add (n);
    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "tube", 1 }, "Tube", tubes, purrvc::kTubeAx7));
    return L;
}

// copies the parameter values into the engine (fixed values of the faceplate's trimmed controls included)
inline void toEngine (const APVTS& s, purrvc::Params& T)
{
    auto f = [&] (const char* id) { return (double) s.getRawParameterValue (id)->load(); };
    auto b = [&] (const char* id) { return s.getRawParameterValue (id)->load() > 0.5f; };
    T.gpos = f ("gpos"); T.pad = b ("pad"); T.imp = f ("imp"); T.ph = b ("ph"); T.lc = b ("lc"); T.tvoltHigh = b ("tvolt");
    T.tcol = f ("tcol"); T.tvoice = f ("tvoice"); T.tvar = f ("tvar"); T.tnoise = b ("tnoise"); T.tube = (int) f ("tube");
    T.eqPost = ! b ("eqpre"); T.eqbyp = b ("eqbyp");
    T.lg = f ("lg"); T.lf = b ("lf") ? 150 : 50; T.lq = f ("lq"); T.m1g = f ("m1g"); T.m1f = f ("m1f"); T.m1q = f ("m1q");
    T.m2g = f ("m2g"); T.m2f = f ("m2f"); T.m2q = f ("m2q"); T.hg = f ("hg"); T.hf = b ("hf") ? 15000 : 5000; T.hq = f ("hq");
    T.thr = f ("thr"); T.ratio = f ("ratio"); T.att = f ("att"); T.rel = b ("hwr") ? juce::jlimit (100.0, 3000.0, f ("rel")) : juce::jmin (1000.0, f ("rel"));
    T.arel = b ("arel"); T.itrim = f ("itrim"); T.makeup = f ("makeup"); T.amk = b ("amk"); T.mix = f ("mix"); T.lim = b ("lim");
    T.ds = f ("ds"); T.dsf = f ("dsf"); T.ethr = f ("ethr"); T.eon = T.ethr > -79.5; T.gate = b ("emode"); T.erange = f ("erange"); T.erel = f ("erel");
    T.out = f ("out"); T.byp = ! b ("power"); T.delta = false; T.match = false;
    // fixed values (the faceplate trims these controls, same as the mockup's FIXED list)
    T.erat = 1.5; T.eatt = 1; T.ehold = 0; T.ehys = 4; T.ekhp = 80; T.eklp = 20000; T.elook = false;
    T.knee = 6; T.schp = 80; T.link = 100; T.dsthr = -32; T.dsw = false; T.dsl = false;
}
} // namespace pvc
