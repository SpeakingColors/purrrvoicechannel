#include "PluginEditor.h"
#include "ui/LayoutGen.h"
#include "BinaryData.h"

using namespace juce;
namespace L = pvc::layout;

namespace {
namespace col {
    const Colour disp { 0xff06080a }, grid { 0xff1b2523 }, ink { 0xfff4f5f7 }, inkDim { 0xffa9adb8 }, cL { 0xff02EBBA }, cAcc { 0xffFFD100 },
                 cR { 0xffFF1383 }, cM { 0xffA43DFF }, panel { 0xff15161a }, line { 0xff2b2d34 }, track { 0xff33353d };
}
Rectangle<float> rf (const L::R& r) { return { (float) r.x, (float) r.y, (float) r.w, (float) r.h }; }
Rectangle<int> ri (const L::R& r) { return { r.x, r.y, r.w, r.h }; }

struct Fonts
{
    Typeface::Ptr label, labelBold, mono;
    Fonts()
    {
        label = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
        labelBold = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
        mono = Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    }
    static Fonts& get() { static Fonts f; return f; }
};
Font labelFont (float h, bool bold = false, float kern = 0.05f) { return Font (FontOptions (bold ? Fonts::get().labelBold : Fonts::get().label).withHeight (h).withKerningFactor (kern)); }
Font monoFont (float h) { return Font (FontOptions (Fonts::get().mono).withHeight (h)); }

void text (Graphics& g, const String& s, float x, float y, Colour c, Justification j, float size, bool mono = true)
{
    g.setColour (c); g.setFont (mono ? monoFont (size) : labelFont (size));
    const float w = 900.0f;
    const float x0 = j.testFlags (Justification::horizontallyCentred) ? x - w / 2 : j.testFlags (Justification::right) ? x - w : x;
    g.drawText (s, Rectangle<float> (x0, y - size, w, size * 1.3f), j.getOnlyHorizontalFlags() | Justification::top, false);
}

struct Tube { const char* name; const char* eu; const char* rate; int lvl; };
const Tube kTubes[purrvc::kNumTubes] = {
    { "Kit Cat", "12AU7 / ECC82", "Clean", 1 }, { "Tabby Tom", "12AU7 / ECC82, aged", "Warm", 2 }, { "Tuxedo", "12AT7 / ECC81", "Warm", 2 },
    { "Old Tux", "12AT7 / ECC81, aged", "Rich", 3 }, { "Gold Cat", "12AX7 / ECC83", "Rich", 3 }, { "Meowlard", "12AX7 / ECC83", "Rich", 3 },
    { "Bengal", "12AX7 / ECC83", "Punchy", 3 }, { "Black Cat", "12AX7 / ECC83", "Dark", 3 }, { "Snowy", "12AX7 / ECC83", "Neutral", 2 },
    { "Alley Cat", "12AX7 / ECC83, aged", "Heavy", 4 } };

double magDb (const purrvc::Coef& c, double f, double sr)
{
    const double w = 2 * MathConstants<double>::pi * f / sr, cw = std::cos (w), sw = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
    const double nr = c[0] + c[1] * cw + c[2] * c2, ni = -(c[1] * sw + c[2] * s2), dr = 1 + c[3] * cw + c[4] * c2, di = -(c[3] * sw + c[4] * s2);
    return 10 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di + 1e-30));
}

String tipFor (const String& id)
{
    for (auto& k : pvc::knobDefs()) if (id == k.id) return String (k.name) + "\n" + k.tip + " Double-click resets.";
    for (auto& b : pvc::boolDefs()) if (id == b.id) return String (b.name) + "\n" + b.tip;
    return {};
}
} // namespace

//==============================================================================
struct Bubbles { bool* on; };

class KnobW : public Component, public SettableTooltipClient
{
public:
    KnobW (RangedAudioParameter& p, const L::Knob& d, bool& bub) : param (p), def (d), bubbles (bub), attach (p, [this] (float) { repaint(); })
    {
        setBounds (ri (d.r).getUnion (ri (d.v)).expanded (2)); attach.sendInitialUpdate(); setTooltip (tipFor (d.id)); setWantsKeyboardFocus (true);
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        mid = String (d.id) == "itrim" || String (d.id) == "out";
    }
    String getTooltip() override { return bubbles ? SettableTooltipClient::getTooltip() : String(); }
    void paint (Graphics& g) override
    {
        const auto kr = rf (def.r).translated ((float) -getX(), (float) -getY());
        const float s = kr.getWidth() / 54.0f, cx = kr.getCentreX(), cy = kr.getCentreY(), r = 23.0f * s, n = param.getValue();
        const float a0 = degreesToRadians (-135.0f), a1 = degreesToRadians (135.0f), av = a0 + n * (a1 - a0);
        auto arc = [&] (float from, float to, Colour c) { Path p; p.addCentredArc (cx, cy, r, r, 0.0f, from, to, true); g.setColour (c); g.strokePath (p, PathStrokeType (3.0f * s, PathStrokeType::curved, PathStrokeType::rounded)); };
        arc (a0, a1, col::track);
        if (mid) { const float am = a0 + param.convertTo0to1 (0.0f) * (a1 - a0); if (std::abs (av - am) > 0.01f) arc (jmin (am, av), jmax (am, av), Colour (def.col)); }
        else if (n > 0.002f) arc (a0, av, Colour (def.col));
        const float br = 19.0f * s; const Rectangle<float> body (cx - br, cy - br, 2 * br, 2 * br);
        g.setGradientFill (ColourGradient (Colour (0xff3a3d47), cx - br * 0.3f, cy - br * 0.45f, Colour (0xff121317), cx + br, cy + br, true)); g.fillEllipse (body);
        g.setColour (Colours::black); g.drawEllipse (body, 1.0f); g.setColour (Colours::white.withAlpha (0.08f)); g.drawEllipse (body.reduced (1.5f), 1.0f);
        Path t; t.addRoundedRectangle (-1.0f * s, -19.0f * s, 2.0f * s, 15.0f * s, 1.0f * s);
        g.setColour (col::ink); g.fillPath (t, AffineTransform::rotation (av).translated (cx, cy));
        if (hasKeyboardFocus (false)) { g.setColour (col::cM.withAlpha (0.6f)); g.drawEllipse (body.expanded (3.0f), 1.5f); }
        const auto vr = rf (def.v).translated ((float) -getX(), (float) -getY());
        g.setColour (col::cL); g.setFont (monoFont (vr.getHeight() >= 20 ? 13.0f : vr.getHeight() >= 15 ? 12.0f : 9.5f));
        g.drawText (pvc::formatValue (def.id, param.convertFrom0to1 (n)), vr.expanded (30, 0).withHeight (jmin (vr.getHeight(), 17.0f)), Justification::centredTop, false);
    }
    void mouseDown (const MouseEvent& e) override { grabKeyboardFocus(); start = param.getValue(); y0 = e.position.y; attach.beginGesture(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const float dy = (y0 - e.position.y) * jmax (0.3f, getParentComponent()->getTransform().getScaleFactor());   // screen pixels
        attach.setValueAsPartOfGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, start + dy / (e.mods.isShiftDown() ? 700.0f : 160.0f))));
    }
    void mouseUp (const MouseEvent&) override { attach.endGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    { if (w.deltaY != 0) attach.setValueAsCompleteGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, param.getValue() + (w.deltaY > 0 ? 0.02f : -0.02f) * (w.isReversed ? -1.0f : 1.0f)))); }
    bool keyPressed (const KeyPress& k) override
    {
        float d = 0; const int c = k.getKeyCode();
        if (c == KeyPress::upKey || c == KeyPress::rightKey) d = 0.02f; else if (c == KeyPress::downKey || c == KeyPress::leftKey) d = -0.02f;
        else if (c == KeyPress::homeKey) { attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); return true; } else return false;
        attach.setValueAsCompleteGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, param.getValue() + d * (k.getModifiers().isShiftDown() ? 0.25f : 1.0f)))); return true;
    }
    bool hitTest (int x, int y) override { return ri (def.r).expanded (6).contains (x + getX(), y + getY()) || ri (def.v).contains (x + getX(), y + getY()); }
private:
    RangedAudioParameter& param; const L::Knob& def; bool& bubbles; ParameterAttachment attach; float start = 0, y0 = 0; bool mid = false;
};

//==============================================================================
void drawLed (Graphics& g, Rectangle<float> r, bool on, bool blue = false)
{
    const auto c = r.getCentre(); const float rad = r.getHeight() * 0.5f;
    if (on)
    {
        const Colour hi = blue ? Colour (0xffbcd8ff) : Colour (0xffffb09a), mid = blue ? Colour (0xff3b86ff) : Colour (0xffe8402a), lo = blue ? Colour (0xff1240a0) : Colour (0xff8a1208);
        g.setColour (mid.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (4), rad + 4);
        ColourGradient cg (hi, c.x, c.y, lo, c.x + r.getWidth() * 0.55f, c.y, true); cg.addColour (0.55, mid); g.setGradientFill (cg);
    }
    else g.setGradientFill (ColourGradient (Colour (0xff6a2620), c.x, c.y, Colour (0xff2e0c0c), c.x + r.getWidth() * 0.55f, c.y, true));
    g.fillRoundedRectangle (r, rad); g.setColour (Colour (0xff140606)); g.drawRoundedRectangle (r, rad, 1.0f);
}

class LedW : public Component, public SettableTooltipClient
{
public:
    LedW (RangedAudioParameter& p, const L::Led& d, bool& bub) : param (p), def (d), bubbles (bub), attach (p, [this] (float) { repaint(); })
    {
        setBounds (ri (d.r)); attach.sendInitialUpdate(); setTooltip (tipFor (id())); setMouseCursor (MouseCursor::PointingHandCursor);
        const String i (d.id);
        label = i == "tnoise" ? "VINTAGE\nNOISE" : i == "amk" ? "AUTO\nMAKEUP" : i == "lim" ? "TP\nLIMITER" : String (d.txt).toUpperCase();
    }
    String id() const { return String (def.id) == "byp" ? "power" : String (def.id); }
    String getTooltip() override { return bubbles ? SettableTooltipClient::getTooltip() : String(); }
    void paint (Graphics& g) override
    {
        const bool on = param.getValue() > 0.5f; const auto b = getLocalBounds().toFloat();
        if (String (def.id) == "byp")
        {
            drawLed (g, Rectangle<float> (b.getX() + 4, b.getCentreY() - 9, 34, 18), on);
            g.setColour (col::cL); g.setFont (labelFont (14.0f, true, 0.05f)); g.drawText ("POWER", b.withTrimmedLeft (46), Justification::centredLeft, false); return;
        }
        const float lw = def.mini ? 22.0f : 34.0f, lh = def.mini ? 12.0f : 18.0f;
        g.setColour (def.mini ? col::cAcc : col::ink); g.setFont (labelFont (def.mini ? 9.0f : 11.5f, true, 0.05f));
        g.drawFittedText (label, b.withTrimmedBottom (lh + 2).toNearestInt(), Justification::centredBottom, 2, 0.8f);
        drawLed (g, Rectangle<float> (b.getCentreX() - lw / 2, b.getBottom() - lh - 1, lw, lh), on);
    }
    void mouseUp (const MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition())) attach.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.0f : 1.0f); }
private:
    RangedAudioParameter& param; const L::Led& def; bool& bubbles; ParameterAttachment attach; String label;
};

class SwitchW : public Component, public SettableTooltipClient
{
public:
    SwitchW (RangedAudioParameter& p, int index, bool& bub) : param (p), i (index), bubbles (bub), attach (p, [this] (float) { repaint(); })
    {
        const auto& s = L::sws[i]; auto all = ri (s.r).getUnion (ri (s.lu)).getUnion (ri (s.ru)).getUnion (ri (L::swLabels[i][0])).getUnion (ri (L::swLabels[i][1]));
        setBounds (all.expanded (2)); attach.sendInitialUpdate(); setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip (tipFor (p.paramID));
    }
    String getTooltip() override { return bubbles ? SettableTooltipClient::getTooltip() : String(); }
    void paint (Graphics& g) override
    {
        const auto& s = L::sws[i]; const auto o = Point<float> ((float) -getX(), (float) -getY()); const bool right = param.getValue() > 0.5f;
        const bool blue = String (s.id) == "tvolt";
        drawLed (g, rf (s.r).translated (o.x, o.y).withSizeKeepingCentre (34, 18), true, blue);
        const Colour ledc = blue ? Colour (0xff3b86ff) : Colour (0xffff4a36);
        for (int k = 0; k < 2; ++k)
        {
            const bool act = (k == 1) == right; const auto u = rf (k ? s.ru : s.lu).translated (o.x, o.y);
            g.setColour (act ? ledc : Colour (0xff2a2c34)); g.fillRect (u);
            const auto tr = rf (L::swLabels[i][k]).translated (o.x, o.y); g.setColour (act ? col::ink : col::inkDim); g.setFont (monoFont (tr.getHeight() + 1.0f)); g.drawText (L::swText[i][k], tr.expanded (8, 2), Justification::centred, false);
        }
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        const auto& s = L::sws[i]; const float x = (float) e.x + (float) getX();
        bool right = param.getValue() > 0.5f;
        if (x < (float) s.r.x) right = false; else if (x > (float) (s.r.x + s.r.w)) right = true; else right = ! right;   // click a label to pick it, the LED to flip
        attach.setValueAsCompleteGesture (right ? 1.0f : 0.0f);
    }
private:
    RangedAudioParameter& param; int i; bool& bubbles; ParameterAttachment attach;
};

//==============================================================================
// The big display: EQ, Dynamics, Tube, GR history, Signal flow, Recall notes (same drawings as the mockup's canvases)
class Display : public Component, public SettableTooltipClient
{
public:
    Display (PurrVoiceProcessor& p) : proc (p), fft (13), win ((size_t) PurrVoiceProcessor::specN, dsp::WindowingFunction<float>::hann, false)
    {
        setBounds (ri (L::disp)); spec.assign (PurrVoiceProcessor::specN / 2, -140.0f);
        notes.setMultiLine (true); notes.setReturnKeyStartsNewLine (true); notes.setFont (monoFont (14.0f));
        notes.setColour (TextEditor::backgroundColourId, col::disp); notes.setColour (TextEditor::textColourId, col::ink); notes.setColour (TextEditor::outlineColourId, col::line);
        notes.setTextToShowWhenEmpty ("Recall notes: mic, distance, singer, song, anything you want to find again. Saved with the project.", col::inkDim);
        notes.setText (proc.notes, false); notes.onTextChange = [this] { proc.notes = notes.getText(); };
        addChildComponent (notes); notes.setBounds (getLocalBounds().reduced (14));
        for (auto* a : { &gh, &eh, &dh }) a->assign (240, 0.0f);
    }
    int view = 0;   // 0 eq, 1 dyn, 2 tube, 3 gr, 4 flow, 5 notes
    void setView (int v) { view = v; notes.setVisible (v == 5); repaint(); }

    float f (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    bool b (const char* id) const { return f (id) > 0.5f; }
    void set (const char* id, float v) { if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } }
    double sr() const { return proc.sampleRateNow() > 0 ? proc.sampleRateNow() : 48000.0; }
    float totalGain() const { return 0.6f * f ("gpos") - (b ("pad") ? 20.0f : 0.0f) + 1.5f * proc.live.tubeK.load(); }
    bool playing() const { return proc.live.lvIn.load() > -100.0f; }

    void tick()
    {
        // spectrum (EQ view) and gain reduction history
        if (view == 0)
        {
            const int w = proc.specW.load (std::memory_order_acquire), N = PurrVoiceProcessor::specN;
            for (int i = 0; i < N; ++i) buf[(size_t) i] = proc.specBuf[(size_t) ((w + i) & (N - 1))];
            win.multiplyWithWindowingTable (buf.data(), (size_t) N); fft.performFrequencyOnlyForwardTransform (buf.data());
            for (int i = 1; i < N / 2; ++i) { const float db = 20.0f * std::log10 (buf[(size_t) i] * 4.0f / (float) N + 1e-9f); spec[(size_t) i] = 0.8f * spec[(size_t) i] + 0.2f * db; }
        }
        auto push = [] (std::vector<float>& a, float v) { a.erase (a.begin()); a.push_back (v); };
        push (gh, proc.live.gr.load()); push (eh, proc.live.eg.load()); push (dh, proc.live.dgr.load());
        if (view != 5) repaint();
    }

    // ---- EQ ----
    static constexpr float pad = 36.0f;
    float fx (double fr) const { return pad + (float) (std::log (fr / 20.0) / std::log (1000.0)) * ((float) getWidth() - 2 * pad); }
    double xf (float x) const { return 20.0 * std::pow (1000.0, (x - pad) / ((float) getWidth() - 2 * pad)); }
    float dy (double d) const { return 14.0f + (float) (18.0 - d) / 36.0f * ((float) getHeight() - 34.0f); }
    double yd (float y) const { return 18.0 - (y - 14.0) / ((float) getHeight() - 34.0) * 36.0; }
    struct Node { float x, y; const char* g; const char* fr; const char* q; Colour c; };
    std::array<Node, 4> nodes() const
    {
        return { Node { fx (b ("lf") ? 150 : 50), dy (f ("lg")), "lg", "lf", "lq", col::cL }, Node { fx (f ("m1f")), dy (f ("m1g")), "m1g", "m1f", "m1q", col::cAcc },
                 Node { fx (f ("m2f")), dy (f ("m2g")), "m2g", "m2f", "m2q", col::cR }, Node { fx (b ("hf") ? 15000 : 5000), dy (f ("hg")), "hg", "hf", "hq", col::cM } };
    }
    void paintEQ (Graphics& g)
    {
        const float w = (float) getWidth(), h = (float) getHeight(); const double s = sr();
        g.setColour (col::grid);
        for (double fr : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 }) { g.drawVerticalLine ((int) fx (fr), 14.0f, h - 20); text (g, fr >= 1000 ? String (fr / 1000) + "k" : String ((int) fr), fx (fr), h - 6, col::inkDim, Justification::horizontallyCentred, 10.5f); g.setColour (col::grid); }
        for (int d = -18; d <= 18; d += 6) { g.setColour (col::grid); g.drawHorizontalLine ((int) dy (d), pad, w - pad); text (g, (d > 0 ? "+" : "") + String (d), pad - 6, dy (d) + 3, col::inkDim, Justification::right, 10.5f); }
        g.setColour (col::inkDim.withAlpha (0.5f)); g.drawHorizontalLine ((int) dy (0), pad, w - pad);
        if (playing())
        {   // output spectrum
            Path p; p.startNewSubPath (pad, h - 20); const double bw = s / PurrVoiceProcessor::specN;
            for (int i = 1; i < PurrVoiceProcessor::specN / 2; ++i) { const double fr = i * bw; if (fr < 20) continue; if (fr > 20000) break;
                const float db = jlimit (-100.0f, 0.0f, spec[(size_t) i]); p.lineTo (fx (fr), h - 20 - (db + 100) / 100 * (h - 40) * 0.8f); }
            p.lineTo (w - pad, h - 20); p.closeSubPath(); g.setColour (col::cL.withAlpha (0.22f)); g.fillPath (p);
        }
        const bool off = b ("eqbyp");
        purrvc::Coef cs[4] = { purrvc::VoiceDsp::design (1, b ("lf") ? 150 : 50, f ("lg"), f ("lq"), s), purrvc::VoiceDsp::design (0, f ("m1f"), f ("m1g"), f ("m1q"), s),
                               purrvc::VoiceDsp::design (0, f ("m2f"), f ("m2g"), f ("m2q"), s), purrvc::VoiceDsp::design (2, b ("hf") ? 15000 : 5000, f ("hg"), f ("hq"), s) };
        Path c; for (int i = 0; i <= 300; ++i) { const double fr = 20 * std::pow (1000.0, i / 300.0); double d = 0; if (! off) for (auto& k : cs) d += magDb (k, fr, s);
            const float px = fx (fr), py = dy (jlimit (-18.0, 18.0, d)); if (i) c.lineTo (px, py); else c.startNewSubPath (px, py); }
        g.setColour (off ? col::inkDim.withAlpha (0.5f) : col::ink); g.strokePath (c, PathStrokeType (2.4f));
        const double z = purrvc::VoiceDsp::toneZ (f ("imp")); const purrvc::Coef t0 = purrvc::VoiceDsp::design (2, 3500, 3 * z, 0.6, s), t1 = purrvc::VoiceDsp::design (0, 220, -1.5 * z, 0.5, s);
        Path tp; for (int i = 0; i <= 120; ++i) { const double fr = 20 * std::pow (1000.0, i / 120.0), d = magDb (t0, fr, s) + magDb (t1, fr, s); const float px = fx (fr), py = dy (jlimit (-18.0, 18.0, d)); if (i) tp.lineTo (px, py); else tp.startNewSubPath (px, py); }
        Path dashed; const float dl[] = { 2.0f, 4.0f }; PathStrokeType (1.6f).createDashedStroke (dashed, tp, dl, 2); g.setColour (col::cAcc); g.fillPath (dashed);
        text (g, "dotted: impedance tone " + String (z >= 0 ? "+" : "") + String (3 * z, 1) + " dB top (our modelling)", pad + 6, 12, col::cAcc, Justification::left, 11.5f);
        if (b ("lc"))
        {
            Path lp; for (int i = 0; i <= 120; ++i) { const double fr = 20 * std::pow (1000.0, i / 120.0), m = fr / std::sqrt (fr * fr + 1e4); const float py = dy (jlimit (-18.0, 18.0, 20 * std::log10 (m))); if (i) lp.lineTo (fx (fr), py); else lp.startNewSubPath (fx (fr), py); }
            Path d2; const float dl2[] = { 4.0f, 4.0f }; PathStrokeType (1.4f).createDashedStroke (d2, lp, dl2, 2); g.setColour (col::cR); g.fillPath (d2);
            text (g, "low cut 100 Hz", fx (30), dy (-12), col::cR, Justification::left, 10.5f);
        }
        int k = 0; for (auto& n : nodes()) { const float r = k == drag ? 9.0f : 7.0f; g.setColour (n.c.withAlpha (off ? 0.4f : 1.0f)); g.fillEllipse (n.x - r, n.y - r, 2 * r, 2 * r); g.setColour (Colours::black); g.drawEllipse (n.x - r, n.y - r, 2 * r, 2 * r, 2.0f); ++k; }
        text (g, off ? String ("EQ bypassed") : String ("EQ ") + (b ("eqpre") ? "before" : "after") + " dynamics", w - pad, 12, col::inkDim, Justification::right, 11.5f);
    }

    // ---- Dynamics ----
    float transfer (float inp) const
    {
        float out = inp; const float ratio = f ("ratio"), thr = f ("thr"), ethr = f ("ethr");
        if (ratio > 1.05f) { const float o = inp - thr, W = 6, half = W / 2, sl = 1 - 1 / ratio; if (o > half) out = inp - o * sl; else if (o > -half) out = inp - sl * (o + half) * (o + half) / (2 * W); out += b ("amk") ? proc.live.mk.load() : f ("makeup"); }
        if (ethr > -79.5f) { if (b ("emode")) { if (inp < ethr) out -= f ("erange"); } else if (inp < ethr) out -= jmin (f ("erange"), (ethr - inp) * 0.5f); }
        return out;
    }
    Point<float> dyxy (float in, float out, float& s, float& ox) const
    {
        const float m = 34, w = (float) getWidth(), h = (float) getHeight(); s = jmin (w, h) - 2 * m; ox = (w - s) / 2;
        return { ox + (in + 70) / 70 * s, h - m - (out + 70) / 70 * s };
    }
    void paintDyn (Graphics& g)
    {
        float s, ox; const float h = (float) getHeight(); dyxy (-70, -70, s, ox);
        for (int v = -70; v <= 0; v += 10)
        { const auto a = dyxy ((float) v, -70, s, ox), bb = dyxy (-70, (float) v, s, ox); g.setColour (col::grid); g.drawLine (a.x, a.y, a.x, a.y - s); g.drawLine (bb.x, bb.y, bb.x + s, bb.y);
          text (g, String (v), a.x, a.y + 14, col::inkDim, Justification::horizontallyCentred, 10.5f); text (g, String (v), bb.x - 6, bb.y + 3, col::inkDim, Justification::right, 10.5f); }
        text (g, "input dB", ox + s / 2, h - 6, col::inkDim, Justification::horizontallyCentred, 11.5f);
        { Graphics::ScopedSaveState ss (g); g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, ox - 26, h / 2)); text (g, "output dB", ox - 26, h / 2, col::inkDim, Justification::horizontallyCentred, 11.5f); }
        { const auto u0 = dyxy (-70, -70, s, ox), u1 = dyxy (0, 0, s, ox); Path p; p.startNewSubPath (u0); p.lineTo (u1); Path d; const float dl[] = { 4.0f, 4.0f }; PathStrokeType (1.0f).createDashedStroke (d, p, dl, 2); g.setColour (col::inkDim); g.fillPath (d); }
        Path c; for (int i = 0; i <= 240; ++i) { const float iv = -70 + i / 240.0f * 70; const auto pp = dyxy (iv, jlimit (-70.0f, 0.0f, transfer (iv)), s, ox); if (i) c.lineTo (pp); else c.startNewSubPath (pp); }
        g.setColour (col::cAcc); g.strokePath (c, PathStrokeType (2.6f));
        const auto th = dyxy (f ("thr"), f ("thr"), s, ox), re = dyxy (0, transfer (0), s, ox), ex = dyxy (f ("ethr"), f ("ethr"), s, ox);
        auto dot = [&] (Point<float> p, float r, Colour c2, float a = 1.0f) { g.setColour (c2.withAlpha (a)); g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r); if (a >= 1) { g.setColour (Colours::black); g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, 2.0f); } };
        if (f ("ratio") > 1.05f) { dot (th, 8, col::cAcc); dot (re, 7, col::cR); text (g, "thr " + String (f ("thr"), 1), th.x + 12, th.y - 8, col::cAcc, Justification::left, 11.5f);
            text (g, f ("ratio") >= 19.9f ? String ("20:1") : String (f ("ratio"), 1) + ":1", re.x - 10, re.y - 10, col::cR, Justification::right, 11.5f); }
        else { dot (th, 8, col::inkDim, 0.55f); text (g, "compressor off (ratio fully left)", th.x + 12, th.y - 8, col::inkDim, Justification::left, 11.5f); }
        if (f ("ethr") > -79.5f) { dot (ex, 8, col::cM); text (g, String (b ("emode") ? "gate " : "expander ") + String (f ("ethr"), 1), ex.x + 12, ex.y - 8, col::cM, Justification::left, 11.5f); }
        else { dot (ex, 8, col::inkDim, 0.55f); text (g, "expander / gate off", ex.x + 12, ex.y - 8, col::inkDim, Justification::left, 11.5f); }
        if (playing()) { const float li = jlimit (-70.0f, 0.0f, proc.live.lvIn.load() + totalGain() + f ("itrim")), lo = jlimit (-70.0f, 0.0f, li - proc.live.gr.load() - proc.live.eg.load());
            const auto lp = dyxy (li, lo, s, ox); g.setColour (col::ink); g.fillEllipse (lp.x - 5, lp.y - 5, 10, 10); }
        if (f ("ds") > 1) text (g, "de-esser " + String (roundToInt (f ("ds"))) + " % at " + pvc::fmtHz (f ("dsf")), ox + 6, 16, col::cR, Justification::left, 11.5f);
    }

    // ---- Tube ----
    struct TubeCfg { int id; double k, col, fc, ap; };
    TubeCfg tubeCfg() const
    {
        const int id = jlimit (0, purrvc::kNumTubes - 1, (int) f ("tube")); const auto sp = purrvc::VoiceDsp::spec (id); const double k = proc.live.tubeK.load(), tv = f ("tvar");
        const double ex = tv <= 0 ? 0.2 : 0.2 + 4.8 * std::pow (tv / 100, 1.2), vm = jlimit (0.0, 3.0, (double) f ("tvoice") / 100);
        return { id, k, f ("tcol") / 100.0, 24000 + jmax (-20500.0, (sp.fc * (1 + 0.3 * k) - 24000) * ex * (vm <= 1 ? vm : 1 + 0.2 * (vm - 1))), 0.1 };
    }
    static double tubeOut (double x, const TubeCfg& T)
    {
        double y = purrvc::VoiceDsp::xfer (T.id, T.k, x); const double cc = 0.7 * T.col * T.col + 0.1 * T.col, g2 = jlimit (0.0, 1.0, (T.col - 0.4) / 0.6), ce = jmax (T.ap, 1e-6);
        y += cc * y * y / (std::abs (y) + 0.02); if (g2 > 0.001) y += g2 * (ce * std::tanh (y / (0.35 * ce)) - y); return y;
    }
    void paintTube (Graphics& g)
    {
        const float w = (float) getWidth(), h = (float) getHeight(); auto T = tubeCfg(); const auto& tb = kTubes[T.id];
        const float cw = jmin (h - 44, w * 0.3f), cx = 44, cy = 22;
        const double amp = jmin (1.5, std::pow (10.0, (playing() ? jmax (-60.0f, proc.live.lvIn.load()) + totalGain() : -12 + totalGain()) / 20.0)); T.ap = amp;
        g.setColour (col::grid); g.drawRect (cx, cy, cw, cw); g.drawVerticalLine ((int) (cx + cw / 2), cy, cy + cw); g.drawHorizontalLine ((int) (cy + cw / 2), cx, cx + cw);
        text (g, "Transfer curve", cx, 14, col::inkDim, Justification::left, 11.5f);
        const double R = 1.5; auto mx = [&] (double v) { return cx + (float) ((v + R) / (2 * R)) * cw; }; auto my = [&] (double v) { return cy + cw - (float) ((v + R) / (2 * R)) * cw; };
        { Path p; p.startNewSubPath (mx (-R), my (-R)); p.lineTo (mx (R), my (R)); Path d; const float dl[] = { 4.0f, 4.0f }; PathStrokeType (1.0f).createDashedStroke (d, p, dl, 2); g.setColour (col::inkDim); g.fillPath (d); }
        Path c; for (int i = 0; i <= 200; ++i) { const double xv = -R + i / 200.0 * 2 * R, yv = jlimit (-R, R, tubeOut (xv, T)); if (i) c.lineTo (mx (xv), my (yv)); else c.startNewSubPath (mx (xv), my (yv)); }
        g.setColour (col::cAcc); g.strokePath (c, PathStrokeType (2.6f));
        g.setColour (col::cR); for (double a : { amp, -amp }) g.fillEllipse (mx (a) - 5, my (jlimit (-R, R, tubeOut (a, T))) - 5, 10, 10);
        text (g, "drive now: " + String (20 * std::log10 (amp), 1) + " dBFS peak", cx, cy + cw + 16, col::cR, Justification::left, 11.5f);
        const float hx = cx + cw + 50, hw = w - hx - 20, base = cy + cw * 0.58f; text (g, "Harmonics added at this level (dB below the fundamental)", hx, 14, col::inkDim, Justification::left, 11.5f);
        double hm[8] {}; const int N = 512;
        for (int m = 1; m <= 8; ++m) { double a = 0, bb = 0; for (int i = 0; i < N; ++i) { const double ph = 2 * MathConstants<double>::pi * i / N, y = tubeOut (amp * std::sin (ph), T); a += y * std::sin (m * ph); bb += y * std::cos (m * ph); } hm[m - 1] = 2 * std::sqrt (a * a + bb * bb) / N; }
        const float bw = jmin (46.0f, (hw - 10) / 7); double thd = 0;
        for (int i = 1; i < 8; ++i)
        {
            const double r = hm[i] / (hm[0] + 1e-12), dbc = 20 * std::log10 (r + 1e-6); thd += r * r;
            const float bh = (float) jlimit (0.0, 1.0, (dbc + 90) / 90) * (cw * 0.5f), bx = hx + (float) (i - 1) * (bw + 4), by = base - bh;
            g.setColour (i % 2 == 1 ? col::cM : col::cL); g.fillRect (bx, by, bw, bh);
            text (g, "H" + String (i + 1), bx + bw / 2, base + 14, col::inkDim, Justification::horizontallyCentred, 11.5f);
            text (g, dbc < -89 ? String ("-90") : String (roundToInt (dbc)), bx + bw / 2, by - 4, col::ink, Justification::horizontallyCentred, 10.5f);
        }
        text (g, "even (teal) sound warm, odd (blue) sound edgy. THD " + String (std::sqrt (thd) * 100, 2) + " %", hx, base + 32, col::inkDim, Justification::left, 11.5f);
        const float fy = base + 54, fh = h - 30 - fy - 8;
        if (fh > 14)
        {
            text (g, "Bandwidth", hx, fy, col::inkDim, Justification::left, 11.5f);
            Path bp; for (int i = 0; i <= 100; ++i) { const double fr = 20 * std::pow (1000.0, i / 100.0), m = 20 * std::log10 (1 / std::sqrt (1 + std::pow (fr / T.fc, 2))); const float px = hx + i / 100.0f * hw, py = fy + 8 + (float) jlimit (0.0, 1.0, -m / 12) * fh; if (i) bp.lineTo (px, py); else bp.startNewSubPath (px, py); }
            g.setColour (col::cAcc); g.strokePath (bp, PathStrokeType (2.0f)); text (g, "-3 dB at " + pvc::fmtHz ((float) T.fc), hx + hw, fy, col::cAcc, Justification::right, 11.5f);
        }
        const float k = proc.live.tubeK.load();
        text (g, "Active tube: " + String (tb.name) + " (" + tb.eu + ")  ·  " + tb.rate + "  ·  Voicing " + String (roundToInt (f ("tvoice"))) + " %  ·  Colour " + String (roundToInt (f ("tcol"))) + " %  ·  voltage "
                 + (b ("tvolt") ? "HIGH" : "NORMAL") + (k > 0.02f && k < 0.98f ? " (settling)" : ""), w - 14, h - 8, col::cAcc, Justification::right, 12.5f);
    }

    // ---- GR history ----
    void paintGR (Graphics& g)
    {
        const float w = (float) getWidth(), h = (float) getHeight(), m = 40, top = 16, bot = h - 20;
        for (int d = 0; d <= 24; d += 6) { const float y = top + d / 24.0f * (bot - top); g.setColour (col::grid); g.drawHorizontalLine ((int) y, m, w - 14); text (g, d ? "-" + String (d) : String ("0"), m - 6, y + 3, col::inkDim, Justification::right, 10.5f); }
        text (g, "last 8 seconds, dB of gain reduction", m, 11, col::inkDim, Justification::left, 11.5f);
        const std::vector<float>* ser[3] = { &gh, &eh, &dh }; const Colour cc[3] = { col::cAcc, col::cM, col::cR }; const char* nm[3] = { "compressor", "expander / gate", "de-esser" };
        for (int k = 0; k < 3; ++k)
        {
            const auto& a = *ser[k]; Path p; p.startNewSubPath (m, top); const float n = (float) a.size() - 1;
            for (size_t i = 0; i < a.size(); ++i) p.lineTo (m + (float) i / n * (w - m - 14), top + jlimit (0.0f, 1.0f, a[i] / 24) * (bot - top));
            p.lineTo (w - 14, top); p.closeSubPath();
            g.setColour (cc[k].withAlpha (0.35f)); g.fillPath (p); g.setColour (cc[k]); g.strokePath (p, PathStrokeType (2.0f));
            text (g, nm[k], w - 20 - k * 110, bot + 14, cc[k], Justification::right, 11.5f);
        }
    }

    // ---- Signal flow ----
    std::vector<std::pair<Rectangle<float>, int>> flowBoxes;
    void paintFlow (Graphics& g)
    {
        flowBoxes.clear();
        const bool pre = b ("eqpre"), eqOff = b ("eqbyp");
        struct B { String t, s; bool on; int act; };
        std::vector<B> bs { { "Input", "", true, 0 }, { "Preamp", "gain " + pvc::fmtDb (totalGain()) + (b ("pad") ? ", pad" : ""), true, 0 },
            { "Tube", String (kTubes[jlimit (0, 9, (int) f ("tube"))].name) + ", " + (b ("tvolt") ? "high" : "normal"), true, 0 } };
        const B eqb { "EQ", eqOff ? "bypassed" : "4 band", ! eqOff, 1 };
        if (pre) bs.push_back (eqb);
        bs.push_back ({ "Exp / gate", f ("ethr") > -79.5f ? (b ("emode") ? "gate" : "1:1.5") : "off", f ("ethr") > -79.5f, 0 });
        bs.push_back ({ "Comp + de-ess", f ("ratio") > 1.05f ? String (f ("ratio"), 1) + ":1" + (f ("ds") > 1 ? ", de-ess" : "") : (f ("ds") > 1 ? "de-ess" : "off"), f ("ratio") > 1.05f || f ("ds") > 1, 0 });
        if (! pre) bs.push_back (eqb);
        bs.push_back ({ "Output", pvc::formatValue ("out", f ("out")), true, 0 }); bs.push_back ({ "TP limiter", b ("lim") ? "-1.2 dBFS" : "off", b ("lim"), 2 });
        const float w = (float) getWidth(), h = (float) getHeight(), n = (float) bs.size(), gap = 26, bw = (w - 60 - gap * (n - 1)) / n, y = h / 2 - 50;
        for (size_t i = 0; i < bs.size(); ++i)
        {
            const Rectangle<float> r (30 + (float) i * (bw + gap), y, bw, 100);
            g.setColour (bs[i].on ? Colour (0xff12201c) : col::panel); g.fillRoundedRectangle (r, 6); g.setColour (bs[i].on ? col::cL : col::line); g.drawRoundedRectangle (r, 6, 1.5f);
            g.setColour (bs[i].on ? col::ink : col::inkDim); g.setFont (labelFont (18.0f, true)); g.drawText (bs[i].t.toUpperCase(), r.withHeight (60), Justification::centred);
            g.setColour (col::cL); g.setFont (monoFont (12.0f)); g.drawText (bs[i].s, r.withTrimmedTop (56).withHeight (24), Justification::centred);
            if (i + 1 < bs.size()) { Path a; a.addArrow ({ r.getRight() + 3, r.getCentreY(), r.getRight() + gap - 3, r.getCentreY() }, 2.0f, 10, 8); g.setColour (col::inkDim); g.fillPath (a); }
            if (bs[i].act) flowBoxes.push_back ({ r, bs[i].act });
        }
        text (g, "Click EQ to move it before or after the dynamics; click the limiter to switch it.", 30, h - 14, col::inkDim, Justification::left, 11.5f);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (col::disp);
        switch (view) { case 0: paintEQ (g); break; case 1: paintDyn (g); break; case 2: paintTube (g); break; case 3: paintGR (g); break; case 4: paintFlow (g); break; default: break; }
    }
    int nearest (Point<float> p) const { int best = -1; float bd = 2500; int k = 0; for (auto& n : nodes()) { const float d = p.getDistanceSquaredFrom ({ n.x, n.y }); if (d < bd) { bd = d; best = k; } ++k; } return best; }
    void mouseDown (const MouseEvent& e) override
    {
        if (view == 0) { drag = nearest (e.position); repaint(); }
        else if (view == 1)
        {
            float s, ox; const auto th = dyxy (f ("thr"), f ("thr"), s, ox), re = dyxy (0, transfer (0), s, ox), ex = dyxy (f ("ethr"), f ("ethr"), s, ox);
            float bd = 900; dragDyn = 0; int k = 1; for (auto p : { th, re, ex }) { const float d = e.position.getDistanceSquaredFrom (p); if (d < bd) { bd = d; dragDyn = k; } ++k; }
        }
        else if (view == 4) for (auto& [r, a] : flowBoxes) if (r.contains (e.position)) { if (a == 1) set ("eqpre", b ("eqpre") ? 0.0f : 1.0f); else set ("lim", b ("lim") ? 0.0f : 1.0f); }
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (view == 0 && drag >= 0)
        {
            const auto n = nodes()[(size_t) drag]; const double fr = xf (e.position.x);
            set (n.g, (float) jlimit (-15.0, 15.0, std::round (yd (e.position.y) * 10) / 10));
            if (drag == 0) set ("lf", fr < 87 ? 0.0f : 1.0f); else if (drag == 3) set ("hf", fr < 8660 ? 0.0f : 1.0f);
            else set (n.fr, (float) fr);
        }
        else if (view == 1 && dragDyn)
        {
            float s, ox; const auto a = dyxy (-70, -70, s, ox); const float inv = -70 + (e.position.x - ox) / s * 70, outv = -70 + (a.y - e.position.y) / s * 70;
            if (dragDyn == 1) set ("thr", jlimit (-64.0f, -4.0f, std::round (inv * 2) / 2));
            else if (dragDyn == 3) set ("ethr", jlimit (-80.0f, -10.0f, std::round (inv * 2) / 2));
            else { const float d = outv - f ("thr"); set ("ratio", d < 0.4f ? 20.0f : jlimit (1.0f, 20.0f, std::round ((0 - f ("thr")) / jmax (0.3f, d) * 10) / 10)); }
        }
    }
    void mouseUp (const MouseEvent&) override { drag = -1; dragDyn = 0; repaint(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (view != 0) return; const int k = nearest (e.position); if (k < 0) return;
        const auto n = nodes()[(size_t) k]; if (auto* p = proc.apvts.getParameter (n.q)) p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + (w.deltaY > 0 ? 0.03f : -0.03f)));
    }
    String getTooltip() override
    {
        static const char* t[] = { "EQ: drag a dot to set gain and frequency (low and high snap to their two frequencies), the wheel sets Q. Shaded: the output spectrum.",
            "Dynamics: drag the yellow dot (threshold), the pink dot (ratio) or the purple dot (expander or gate threshold).",
            "Tube: the transfer curve of the active tube at its voltage, the harmonics it adds at the current drive, and its bandwidth.",
            "Gain reduction of the last 8 seconds: compressor, expander or gate, de-esser.", "Signal flow. Click EQ to move it before or after the dynamics.", "" };
        return proc.bubbles ? String (t[jlimit (0, 5, view)]) : String();
    }
private:
    PurrVoiceProcessor& proc; dsp::FFT fft; dsp::WindowingFunction<float> win;
    std::array<float, 2 * PurrVoiceProcessor::specN> buf {}; std::vector<float> spec, gh, eh, dh; TextEditor notes; int drag = -1, dragDyn = 0;
};

//==============================================================================
class HitButton : public Component, public SettableTooltipClient
{
public:
    HitButton (const L::R& r, String tip, std::function<void()> fn) : onClick (std::move (fn)) { setBounds (ri (r)); setTooltip (tip); setMouseCursor (MouseCursor::PointingHandCursor); }
    void paint (Graphics& g) override { if (isMouseOver()) { g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f); } }
    void mouseEnter (const MouseEvent&) override { repaint(); }
    void mouseExit (const MouseEvent&) override { repaint(); }
    void mouseUp (const MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
};

class LaF : public LookAndFeel_V4
{
public:
    LaF()
    {
        setColour (PopupMenu::backgroundColourId, col::panel); setColour (PopupMenu::textColourId, col::ink); setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff241238));
        setColour (PopupMenu::highlightedTextColourId, Colour (0xffE1CBF4)); setColour (PopupMenu::headerTextColourId, col::cL);
        setColour (ComboBox::backgroundColourId, Colour (0xff101115)); setColour (ComboBox::textColourId, col::ink); setColour (ComboBox::outlineColourId, col::line); setColour (ComboBox::arrowColourId, col::ink);
        setColour (TooltipWindow::backgroundColourId, Colour (0xff1c1d22)); setColour (TooltipWindow::textColourId, col::ink); setColour (TooltipWindow::outlineColourId, col::line);
        setColour (AlertWindow::backgroundColourId, col::panel); setColour (AlertWindow::textColourId, col::ink); setColour (AlertWindow::outlineColourId, col::line);
        setColour (TextButton::buttonColourId, Colour (0xff1c1d22)); setColour (TextButton::textColourOffId, col::ink); setColour (TextEditor::backgroundColourId, Colour (0xff101115));
    }
    Font getComboBoxFont (ComboBox&) override { return labelFont (13.0f, true); }
    Font getPopupMenuFont() override { return labelFont (16.0f); }
    Font getAlertWindowMessageFont() override { return labelFont (15.0f); }
    Font getAlertWindowTitleFont() override { return labelFont (20.0f, true); }
    Font getTextButtonFont (TextButton&, int) override { return labelFont (15.0f, true); }
    Rectangle<int> getTooltipBounds (const String& t, Point<int> pos, Rectangle<int> parent) override
    {
        const TextLayout tl = layoutTip (t); const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 14;
        return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12, pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 18, w, h).constrainedWithin (parent);
    }
    void drawTooltip (Graphics& g, const String& t, int w, int h) override
    {
        g.fillAll (Colour (0xff1c1d22)); g.setColour (col::cL.withAlpha (0.6f)); g.drawRect (0, 0, w, h);
        layoutTip (t).draw (g, Rectangle<float> (9.0f, 7.0f, (float) w - 18.0f, (float) h - 14.0f));
    }
    static TextLayout layoutTip (const String& t)
    {
        AttributedString s; const auto head = t.upToFirstOccurrenceOf ("\n", false, false), body = t.fromFirstOccurrenceOf ("\n", false, false);
        s.append (head + (body.isNotEmpty() ? "\n" : ""), labelFont (15.0f, true), col::cL); if (body.isNotEmpty()) s.append (body, labelFont (14.0f), col::ink);
        TextLayout tl; tl.createLayout (s, 340.0f); return tl;
    }
};

//==============================================================================
struct PurrVoiceEditor::Content : public Component
{
    Content (PurrVoiceEditor& o, PurrVoiceProcessor& p) : owner (o), proc (p), display (p)
    {
        setLookAndFeel (&laf);
        face = ImageCache::getFromMemory (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);
        for (auto& k : L::knobs) if (auto* prm = proc.apvts.getParameter (k.id)) addAndMakeVisible (knobs.add (new KnobW (*prm, k, proc.bubbles)));
        for (auto& l : L::leds) if (auto* prm = proc.apvts.getParameter (String (l.id) == "byp" ? "power" : l.id)) addAndMakeVisible (leds.add (new LedW (*prm, l, proc.bubbles)));
        for (int i = 0; i < (int) std::size (L::sws); ++i)
        { const String id (L::sws[i].id); if (auto* prm = proc.apvts.getParameter (id == "eqpos" ? "eqpre" : id)) addAndMakeVisible (sws.add (new SwitchW (*prm, i, proc.bubbles))); }
        addAndMakeVisible (display);
        // tube menu
        tubeBox.setBounds (ri (L::tubeSel)); for (int i = 0; i < purrvc::kNumTubes; ++i) tubeBox.addItem (String (kTubes[i].name) + " · " + String (kTubes[i].eu).upToFirstOccurrenceOf (" /", false, false) + (String (kTubes[i].eu).contains ("aged") ? " aged" : ""), i + 1);
        tubeAttach = std::make_unique<ComboBoxParameterAttachment> (*proc.apvts.getParameter ("tube"), tubeBox); addAndMakeVisible (tubeBox);
        tubeBox.setTooltip ("Tube\nPick the tube type: cat names, the real type in small text. Each has its own curve, voicing, bandwidth, sag and noise.");
        // header and tabs
        auto add = [this] (const L::R& r, String tip, std::function<void()> fn) { addAndMakeVisible (hits.add (new HitButton (r, tip, std::move (fn)))); };
        add (L::pPrev, "Previous preset", [this] { step (-1); }); add (L::pNext, "Next preset", [this] { step (1); });
        add (L::pName, "Presets\nClick to pick a preset. Double-click to rename your own.", [this] { presetMenu(); });
        add (L::pSave, "Save\nSave the sound as your own preset.", [this] { savePreset(); }); add (L::pMore, "More\nRename, delete, export, import, init.", [this] { moreMenu(); });
        add (L::bHelp, "Guide", [this] { guide(); }); add (L::bSet, "Settings", [this] { settings(); }); add (L::bAbout, "About & Support", [this] { about(); });
        for (int i = 0; i < (int) std::size (L::tabs); ++i) add (L::tabs[i].r, {}, [this, i] { setView (i); });
        setWantsKeyboardFocus (true);
    }
    ~Content() override { setLookAndFeel (nullptr); }

    void setView (int v) { view = v; display.setView (v); repaint(); }
    float f (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (face, Rectangle<float> (0, 0, (float) L::W, (float) L::H));
        // active tab
        static const char* tn[] = { "EQ", "DYNAMICS", "TUBE", "GR HISTORY", "SIGNAL FLOW", "RECALL NOTES" };
        { const auto r = rf (L::tabs[view].r); g.setColour (Colour (0xff0d2622)); g.fillRoundedRectangle (r, 2); g.setColour (col::cL); g.drawRoundedRectangle (r.reduced (0.5f), 2, 1.0f);
          g.setFont (labelFont (14.0f, true, 0.12f)); g.drawText (tn[view], r, Justification::centred); }
        // preset name
        g.setColour (col::ink); g.setFont (monoFont (16.0f)); g.drawText (proc.presetName, rf (L::pName).withTrimmedLeft (12).withTrimmedRight (40), Justification::centredLeft, true);
        // preamp readouts
        const float tk = proc.live.tubeK.load(), gain = 0.6f * f ("gpos") - (f ("pad") > 0.5f ? 20.0f : 0.0f) + 1.5f * tk;
        g.setColour (col::inkDim); g.setFont (monoFont (12.0f)); g.drawText ("gain " + pvc::fmtDb (gain), rf (L::gTot).expanded (30, 2), Justification::centredRight, false);
        const auto& tb = kTubes[jlimit (0, 9, (int) f ("tube"))];
        g.setColour (col::cAcc); g.setFont (labelFont (14.0f, true)); { const auto rr = rf (L::tRate); g.drawText (tb.rate, Rectangle<float> (rr.getRight() - 80, rr.getY() - 2, 80, rr.getHeight() + 4), Justification::centredRight, false); }
        for (int i = 0; i < 4; ++i) { const auto d = rf (L::tDots); const float s = d.getHeight() - 1; g.setColour (i < tb.lvl ? col::cAcc : Colour (0xff3a3d48)); g.fillEllipse (d.getX() + (float) i * (s + 1.5f), d.getY() + 0.5f, s, s); }
        { const auto r = rf (L::tLine); g.setColour (col::ink); g.setFont (labelFont (16.0f, true)); g.drawText (tb.name, r.withHeight (15), Justification::topLeft, false);
          g.setColour (col::inkDim); g.setFont (labelFont (10.0f)); g.drawText (String (tb.eu).toUpperCase(), r.withTrimmedTop (16), Justification::topLeft, false); }
        { const float tgt = f ("tvolt") > 0.5f ? 1.0f : 0.0f, left = std::abs (tgt - tk), done = 1 - left; const auto r = rf (L::settleB);
          g.setColour (Colour (0xff2a2c34)); g.fillRoundedRectangle (r, 2); g.setColour (col::cL); g.fillRoundedRectangle (r.withWidth (r.getWidth() * done), 2);
          g.setColour (col::ink); g.setFont (labelFont (11.0f, true, 0.12f)); g.drawText (left > 0.03f ? "SETTLING " + String (roundToInt (done * 100)) + " %" : String ("SETTLED"), rf (L::settleT), Justification::centredLeft, false); }
        // dynamics GR meter
        { const auto r = rf (L::cvGR); g.setColour (col::disp); g.fillRect (r); const float bw = (r.getWidth() - 44) / 24, gr = jmin (24.0f, mGr);
          for (int i = 0; i < 24; ++i) { const bool on = gr >= (float) i + 0.5f; g.setColour ((i >= 16 ? col::cR : i >= 6 ? col::cAcc : col::cL).withAlpha (on ? 1.0f : 0.15f)); g.fillRect (r.getX() + 6 + (float) i * bw, r.getY() + 8, bw - 1.5f, 16.0f); }
          for (int d : { 0, 3, 6, 12, 18, 24 }) text (g, String (d), r.getX() + 6 + (float) d * bw, r.getBottom() - 6, col::inkDim, Justification::horizontallyCentred, 9.5f);
          const float br = mEg <= 0.2f ? 0.1f : mEg < 5 ? 0.45f : 1.0f; g.setColour (col::cAcc.withAlpha (br)); g.fillEllipse (r.getRight() - 26, r.getY() + 9, 16, 16); }
        paintVU (g); paintMeters (g); paintRo (g);
        g.setColour (col::inkDim); g.setFont (labelFont (12.0f, true, 0.15f)); g.drawText ("WINDOW " + String (roundToInt ((float) owner.getWidth() / kDefaultW * 100)) + " %", rf (L::sizeT).expanded (20, 2), Justification::centred, false);
        { AttributedString a; a.append ("STUDIO 87", labelFont (12.5f, true, 0.12f), col::ink); a.append (String::fromUTF8 (" \xc2\xb7 PURRR VOICE CHANNEL V") + JucePlugin_VersionString, labelFont (12.5f, false, 0.12f), col::inkDim);
          a.draw (g, rf (L::foot).withWidth (500).expanded (0, 2)); }
    }
    void paintVU (Graphics& g)
    {
        const auto r = rf (L::cvVU); g.setColour (Colour (0xffe9dfb8)); g.fillRect (r); const float w = r.getWidth(), h = r.getHeight(), cx = r.getX() + w / 2, cy = r.getY() + h + 22, R = h + 2;
        auto ang = [] (float vu) { return -MathConstants<float>::halfPi - 0.82f + (jlimit (-20.0f, 3.0f, vu) + 20) / 23 * 1.64f; };
        for (int v : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
        {
            const float a = ang ((float) v), r0 = R - 10, r1 = R - 2 - ((v % 5 == 0) ? 5.0f : 0.0f) + 4; g.setColour (v > 0 ? Colour (0xffb01030) : Colour (0xff222222));
            g.drawLine (cx + std::cos (a) * r0, cy + std::sin (a) * r0, cx + std::cos (a) * r1, cy + std::sin (a) * r1, 1.2f);
            if (v == -20 || v == -10 || v == -5 || v == 0 || v == 3) text (g, String (v), cx + std::cos (a) * (R - 22), cy + std::sin (a) * (R - 22) + 3, v > 0 ? Colour (0xffb01030) : Colour (0xff222222), Justification::horizontallyCentred, 10.0f);
        }
        text (g, "VU", cx, r.getY() + h - 26, Colour (0xff222222), Justification::horizontallyCentred, 10.0f);
        Graphics::ScopedSaveState ss (g); g.reduceClipRegion (r.toNearestInt());
        const float a2 = ang (mVu + 20); g.setColour (Colour (0xff111111)); g.drawLine (cx, cy, cx + std::cos (a2) * (R - 6), cy + std::sin (a2) * (R - 6), 1.6f);
    }
    void paintMeters (Graphics& g)
    {
        const auto r = rf (L::cvM); g.setColour (col::disp); g.fillRect (r);
        const float x0 = r.getX() + 26, x1 = r.getRight() - 52, lo = -50, sw = (x1 - x0) / 50, rh = (r.getHeight() - 26) / 2 - 3;
        const float lv[2] = { mAl, mAr }, pk[2] = { mPl, mPr };
        for (int k = 0; k < 2; ++k)
        {
            const float y = r.getY() + 6 + (float) k * (rh + 4); text (g, k ? "R" : "L", r.getX() + 12, y + rh / 2 + 4, col::inkDim, Justification::horizontallyCentred, 11.0f);
            for (int s = 0; s < 50; ++s) { const float d = lo + (float) (s + 1) * (0 - lo) / 50, on = lv[k] >= d - 0.5f * (0 - lo) / 50; g.setColour ((d > -0.01f ? col::cR : d > -12 ? col::cM : Colour (0xff6CC9F5)).withAlpha (on ? 1.0f : 0.13f)); g.fillRect (x0 + (float) s * sw, y, sw - 1, rh); }
            const float px = x0 + jlimit (0.0f, 1.0f, (pk[k] - lo) / (0 - lo)) * (x1 - x0); g.setColour (col::ink); g.fillRect (px - 1, y - 1, 2.0f, rh + 2);
            const bool clip = pk[k] > -0.2f; g.setColour (col::cR.withAlpha (clip ? 1.0f : 0.2f)); g.fillRect (x1 + 8, y, 38.0f, rh);
            if (k == 0) text (g, "CLIP", x1 + 27, y + rh / 2 + 3, clip ? Colours::white : col::inkDim, Justification::horizontallyCentred, 9.0f);
        }
        for (int d : { -50, -40, -30, -20, -10, -5, 0 }) text (g, String (d), x0 + (d - lo) / (0 - lo) * (x1 - x0), r.getBottom() - 3, d == -20 ? col::cAcc : col::inkDim, Justification::horizontallyCentred, 9.5f);
    }
    void paintRo (Graphics& g)
    {
        auto fmt = [] (float v) { return v > -89 ? String (v, 1) : String ("-inf"); }; auto gr = [] (float v) { return v > 0.05f ? "-" + String (v, 1) : String ("0.0"); };
        const String rows[7][2] = { { "In peak", fmt (mIn) }, { "Out peak", fmt (mOut) }, { "Comp GR", gr (mGr) }, { "Exp/gate", gr (mEg) }, { "De-ess GR", gr (mDg) },
                                    { "Tube", kTubes[jlimit (0, 9, (int) f ("tube"))].name }, { "Voltage", f ("tvolt") > 0.5f ? "HIGH" : "NORMAL" } };
        const auto r = rf (L::ro); const float cw = r.getWidth() / 2, rh = 17;
        for (int i = 0; i < 7; ++i)
        {
            const Rectangle<float> c (r.getX() + (float) (i % 2) * cw + (i % 2 ? 6.0f : 0.0f), r.getY() + (float) (i / 2) * rh, cw - 12, rh);
            g.setColour (col::inkDim); g.setFont (monoFont (12.0f)); g.drawText (rows[i][0], c, Justification::centredLeft, false);
            g.setColour (col::cAcc); g.drawText (rows[i][1], c, Justification::centredRight, false);
        }
    }
    void tick()
    {
        if (! isShowing()) return;   // pause drawing while hidden; audio keeps running
        const auto& lv = proc.live; const double now = Time::getMillisecondCounterHiRes();
        auto db = [] (float v) { return 20.0f * std::log10 (v + 1e-9f); };
        const float ol = db (lv.outL.load()), orr = db (lv.outR.load()), i = jmax (db (lv.inL.load()), db (lv.inR.load())), o = jmax (ol, orr);
        mAl += (ol - mAl) * (ol > mAl ? 0.85f : 0.23f); mAr += (orr - mAr) * (orr > mAr ? 0.85f : 0.23f);
        if (ol > mPl || now - tPl > 2000) { mPl = ol; tPl = now; } if (orr > mPr || now - tPr > 2000) { mPr = orr; tPr = now; }
        if (i > mIn || now - tIn > 2000) { mIn = i; tIn = now; } if (o > mOut || now - tOut > 2000) { mOut = o; tOut = now; }
        mVu += (db (lv.rms.load()) - mVu) * 0.33f; mGr = lv.gr.load(); mEg = lv.eg.load(); mDg = lv.dgr.load();
        display.tick();
        for (auto r : { L::cvGR, L::cvVU, L::cvM, L::ro, L::gTot, L::settleB, L::settleT }) repaint (ri (r).expanded (32, 4));
        if (proc.presetName != shownName) { shownName = proc.presetName; repaint (ri (L::pName)); }
    }
    void mouseDoubleClick (const MouseEvent& e) override { if (e.x < 26 || e.x > L::W - 26) owner.resetZoom(); }
    bool keyPressed (const KeyPress& k) override
    {
        const auto c = k.getTextCharacter(); if (c >= '1' && c <= '6') { setView (c - '1'); return true; }
        if (c == 'b' || c == 'B') { if (auto* p = proc.apvts.getParameter ("power")) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); return true; }
        return false;
    }

    // ---- presets ----
    struct Item { bool user; int index; String name; };
    std::vector<Item> allPresets() const
    {
        std::vector<Item> v; for (int i = 0; i < (int) proc.factory.size(); ++i) v.push_back ({ false, i, proc.factory[(size_t) i].name });
        for (auto& n : proc.userPresetNames()) v.push_back ({ true, -1, n }); return v;
    }
    void load (const Item& it) { if (it.user) proc.loadUser (it.name); else proc.loadFactory (it.index); repaint(); }
    void step (int d)
    {
        auto v = allPresets(); if (v.empty()) return; int cur = 0;
        for (int i = 0; i < (int) v.size(); ++i) if (v[(size_t) i].name == proc.presetName && v[(size_t) i].user == (proc.currentFactory < 0)) cur = i;
        load (v[(size_t) ((cur + d + (int) v.size()) % (int) v.size())]);
    }
    void presetMenu()
    {
        PopupMenu m; auto v = allPresets(); m.addSectionHeader ("Factory"); int id = 1;
        for (auto& it : v) { if (it.user && id == (int) proc.factory.size() + 1) m.addSectionHeader ("Yours"); m.addItem (id++, it.name, true, it.name == proc.presetName); }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (hits[2]), [this, v] (int r) { if (r > 0 && r <= (int) v.size()) load (v[(size_t) r - 1]); });
    }
    void ask (const String& title, const String& msg, const String& def, std::function<void (String)> done)
    {
        auto* w = new AlertWindow (title, msg, MessageBoxIconType::NoIcon); w->setLookAndFeel (&laf);
        w->addTextEditor ("name", def); w->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([w, done] (int r) { const String s = w->getTextEditorContents ("name").trim(); if (r == 1 && s.isNotEmpty()) done (s); }), true);
    }
    void info (const String& title, const String& msg) { AlertWindow::showAsync (MessageBoxOptions().withTitle (title).withMessage (msg).withButton ("OK").withAssociatedComponent (this), nullptr); }
    void savePreset()
    {
        const bool isFactory = proc.currentFactory >= 0;
        ask ("Save preset", "Name for your preset:", isFactory ? "My " + proc.presetName : proc.presetName, [this] (String n)
        {
            if (proc.userPresetNames().contains (n))
            {
                AlertWindow::showAsync (MessageBoxOptions().withTitle ("Preset exists").withMessage ("\"" + n + "\" already exists.").withButton ("Overwrite").withButton ("Save as new").withButton ("Cancel").withAssociatedComponent (this),
                    [this, n] (int r) { if (r == 1) proc.saveUser (n); else if (r == 2) { int k = 2; while (proc.userPresetNames().contains (n + " " + String (k))) ++k; proc.saveUser (n + " " + String (k)); } repaint(); });
            }
            else { if (! proc.saveUser (n)) info ("Save", "Could not write the preset file."); repaint(); }
        });
    }
    void moreMenu()
    {
        const bool user = proc.currentFactory < 0 && proc.userPresetNames().contains (proc.presetName);
        PopupMenu m; m.addItem (1, "Rename", user); m.addItem (2, "Delete", user); m.addSeparator(); m.addItem (3, "Export preset..."); m.addItem (4, "Import preset..."); m.addSeparator(); m.addItem (5, "Init (clean start)");
        m.addItem (6, "Open the preset folder");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (hits[4]), [this] (int r)
        {
            if (r == 1) ask ("Rename", "New name:", proc.presetName, [this] (String n) { const String old = proc.presetName; if (proc.saveUser (n)) proc.deleteUser (old); repaint(); });
            else if (r == 2) AlertWindow::showAsync (MessageBoxOptions().withTitle ("Delete").withMessage ("Delete \"" + proc.presetName + "\"?").withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                    [this] (int b) { if (b == 1) { proc.deleteUser (proc.presetName); proc.loadFactory (0); repaint(); } });
            else if (r == 3) { chooser = std::make_unique<FileChooser> ("Export preset", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (File::createLegalFileName (proc.presetName) + ".pvcpreset"), "*.pvcpreset");
                chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& c) { if (c.getResult() != File()) proc.exportTo (c.getResult().withFileExtension ("pvcpreset")); }); }
            else if (r == 4) { chooser = std::make_unique<FileChooser> ("Import preset", File::getSpecialLocation (File::userDocumentsDirectory), "*.pvcpreset");
                chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& c) { if (c.getResult() != File() && ! proc.importFrom (c.getResult())) info ("Import", "That file is not a Purrr Voice Channel preset."); repaint(); }); }
            else if (r == 5) { proc.loadFactory (0); repaint(); }
            else if (r == 6) { PurrVoiceProcessor::presetDir().createDirectory(); PurrVoiceProcessor::presetDir().startAsProcess(); }
        });
    }
    void guide()
    {
        info ("Guide", "Purrr Voice Channel is a tube channel strip for voice: preamp with a tube, expander or gate, compressor with de-esser, 4 band EQ before or after the dynamics, output.\n\n"
                       "Start with Gain: it drives the tube, like the hardware. Pick a tube, then shape it with Voicing (its own tone), Colour (warm to nasty) and Variation (how different the tubes are).\n\n"
                       "Knobs: drag up or down (Shift = fine), wheel, arrow keys, double-click resets.\n"
                       "Keys: 1 to 6 switch the display (EQ, Dynamics, Tube, GR history, Signal flow, Recall notes), B toggles Power.\n"
                       "Display: drag the dots on the EQ and Dynamics views.\n"
                       "Window: drag any edge or corner (the shape stays), double-click a wood side for 100 %.");
    }
    void settings()
    {
        PopupMenu m; m.addItem (1, "Info bubbles", true, proc.bubbles); m.addSeparator();
        for (int p : { 70, 85, 100, 115, 130 }) m.addItem (100 + p, "Window " + String (p) + " %", true, std::abs ((float) owner.getWidth() / kDefaultW * 100 - (float) p) < 1.5f);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (hits[6]), [this] (int r)
        { if (r == 1) proc.bubbles = ! proc.bubbles; else if (r > 100) { const int w = kDefaultW * (r - 100) / 100; owner.setSize (w, (int) std::lround (w * (double) L::H / L::W)); } });
    }
    void about()
    {
        AlertWindow::showAsync (MessageBoxOptions().withTitle ("About & Support").withMessage (String ("Purrr Voice Channel ") + JucePlugin_VersionString + " by Studio 87.\n"
            "Free and open source, AGPL-3.0-or-later. Collects nothing, makes no network calls; the buttons below only open a page in your browser when you click them.\n\n"
            "Your presets live in:\n" + PurrVoiceProcessor::presetDir().getFullPathName() + "\n\n"
            "A software tube channel strip in the spirit of a classic voice channel. No brand names, no affiliation; tube names are our own cat names.")
            .withButton ("Support on Ko-fi").withButton ("Open on GitHub").withButton ("Close").withAssociatedComponent (this),
            [] (int r) { if (r == 1) URL ("https://ko-fi.com/studio87").launchInDefaultBrowser(); else if (r == 2) URL ("https://github.com/SpeakingColors/purrrvoicechannel").launchInDefaultBrowser(); });
    }

    PurrVoiceEditor& owner; PurrVoiceProcessor& proc; LaF laf; Image face; Display display;
    OwnedArray<KnobW> knobs; OwnedArray<LedW> leds; OwnedArray<SwitchW> sws; OwnedArray<HitButton> hits; ComboBox tubeBox; std::unique_ptr<ComboBoxParameterAttachment> tubeAttach;
    std::unique_ptr<FileChooser> chooser; int view = 0; String shownName;
    float mAl = -90, mAr = -90, mPl = -90, mPr = -90, mIn = -90, mOut = -90, mVu = -90, mGr = 0, mEg = 0, mDg = 0; double tPl = 0, tPr = 0, tIn = 0, tOut = 0;
};

//==============================================================================
PurrVoiceEditor::PurrVoiceEditor (PurrVoiceProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    content = std::make_unique<Content> (*this, p); addAndMakeVisible (*content);
    tips.setLookAndFeel (&content->laf);
    if (const char* v = std::getenv ("PVC_VIEW")) content->setView (jlimit (0, 5, std::atoi (v)));   // test hook for screenshots
    constrainer.setFixedAspectRatio ((double) L::W / L::H); constrainer.setSizeLimits (900, (int) (900.0 * L::H / L::W), 2800, (int) (2800.0 * L::H / L::W));
    setConstrainer (&constrainer);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer); border->setBorderThickness (BorderSize<int> (6)); addAndMakeVisible (*border);   // every side and corner
    corner = std::make_unique<ResizableCornerComponent> (this, &constrainer); addAndMakeVisible (*corner);
    setResizable (true, false);
    const int w = jlimit (900, 2800, proc.editorWidth > 0 ? proc.editorWidth : kDefaultW);
    setSize (w, (int) std::lround (w * (double) L::H / L::W));
    startTimerHz (30);
}
PurrVoiceEditor::~PurrVoiceEditor() { stopTimer(); tips.setLookAndFeel (nullptr); }
void PurrVoiceEditor::resetZoom() { setSize (kDefaultW, (int) std::lround (kDefaultW * (double) L::H / L::W)); }
void PurrVoiceEditor::resized()
{
    const float s = (float) getWidth() / (float) L::W;
    content->setTransform (AffineTransform::scale (s)); content->setBounds (0, 0, L::W, L::H);
    if (border) { border->setBounds (getLocalBounds()); border->toFront (false); }
    if (corner) { corner->setBounds (getWidth() - 18, getHeight() - 18, 18, 18); corner->toFront (false); }
    proc.editorWidth = getWidth(); content->repaint (ri (L::sizeT).expanded (20, 4));
}
void PurrVoiceEditor::timerCallback() { content->tick(); }
