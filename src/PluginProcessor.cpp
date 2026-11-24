#include "PluginProcessor.h"
#include "PluginEditor.h"

PurrVoiceProcessor::PurrVoiceProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PurrrVoiceChannel", pvc::makeLayout())
{
    dsp = std::make_unique<purrvc::VoiceDsp> (48000.0);   // builds the tube tables once, off the audio thread
    setLatencySamples (dsp->latency());
}

bool PurrVoiceProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void PurrVoiceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    dsp = std::make_unique<purrvc::VoiceDsp> (sampleRate);
    pvc::toEngine (apvts, dsp->T);
    dsp->setTubeK (dsp->T.tvoltHigh ? 1.0 : 0.0); jumpVoltage = false;
    setLatencySamples (dsp->latency());
    dryDelay.setSize (2, 4096); dryDelay.clear(); dryPos = 0;
    juce::ignoreUnused (samplesPerBlock);
}

void PurrVoiceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), nin = getTotalNumInputChannels(), nout = getTotalNumOutputChannels();
    if (n <= 0) return;
    pvc::toEngine (apvts, dsp->T);
    if (jumpVoltage) { dsp->setTubeK (dsp->T.tvoltHigh ? 1.0 : 0.0); jumpVoltage = false; }
    float* L = buffer.getWritePointer (0);
    float* R = nout > 1 ? buffer.getWritePointer (1) : L;
    if (nin == 1 && nout > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);   // mono in, stereo out
    float peakIn = 0; for (int i = 0; i < n; ++i) peakIn = std::max (peakIn, std::max (std::abs (L[i]), std::abs (R[i])));
    const auto& st = dsp->process (L, R, L, R, n);                       // in place is safe: each sample is read before it is written
    live.inL = (float) st.inL; live.inR = (float) st.inR; live.outL = (float) st.outL; live.outR = (float) st.outR; live.rms = (float) st.rms;
    live.gr = (float) st.gr; live.eg = (float) st.eg; live.dgr = (float) st.dgr; live.tubeK = (float) st.tubeK; live.gopen = st.gopen; live.mk = (float) st.mk;
    live.lvIn = 20.0f * std::log10 (peakIn + 1e-9f);
    int w = specW.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) { specBuf[(size_t) w] = 0.5f * (L[i] + R[i]); w = (w + 1) & (specN - 1); }
    specW.store (w, std::memory_order_release);
}

void PurrVoiceProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // host bypass: the dry signal, delayed by the plugin latency so nothing jumps in time
    const int n = buffer.getNumSamples(), lat = dsp ? dsp->latency() : 0, ch = juce::jmin (2, buffer.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < ch; ++c) { float* d = dryDelay.getWritePointer (c); d[dryPos] = buffer.getSample (c, i); buffer.setSample (c, i, d[(dryPos - lat + 4096) & 4095]); }
        dryPos = (dryPos + 1) & 4095;
    }
}

juce::AudioProcessorEditor* PurrVoiceProcessor::createEditor() { return new PurrVoiceEditor (*this); }

juce::ValueTree PurrVoiceProcessor::stateTree() const
{
    auto t = const_cast<juce::AudioProcessorValueTreeState&> (apvts).copyState();
    t.setProperty ("preset", presetName, nullptr); t.setProperty ("notes", notes, nullptr); t.setProperty ("editorWidth", editorWidth, nullptr);
    t.setProperty ("bubbles", bubbles, nullptr); t.setProperty ("version", JucePlugin_VersionString, nullptr);
    return t;
}
void PurrVoiceProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = stateTree().createXml()) copyXmlToBinary (*xml, dest);
}
void PurrVoiceProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            presetName = t.getProperty ("preset", presetName).toString(); notes = t.getProperty ("notes", "").toString();
            editorWidth = (int) t.getProperty ("editorWidth", 0); bubbles = (bool) t.getProperty ("bubbles", true);
            apvts.replaceState (t); jumpVoltage = true;
        }
}

juce::File PurrVoiceProcessor::presetDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Studio 87").getChildFile ("Purrr Voice Channel").getChildFile ("presets");
}
juce::StringArray PurrVoiceProcessor::userPresetNames() const
{
    juce::StringArray s; for (auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.pvcpreset")) s.add (f.getFileNameWithoutExtension());
    s.sortNatural(); return s;
}
void PurrVoiceProcessor::resetToDefaults()
{
    for (auto* p : getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) r->setValueNotifyingHost (r->getDefaultValue());
}
void PurrVoiceProcessor::applyValues (const std::vector<std::pair<juce::String, float>>& v)
{
    resetToDefaults();
    for (auto& [id, val] : v) if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (val));
    jumpVoltage = true;
}
void PurrVoiceProcessor::loadFactory (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) factory.size())) return;
    applyValues (factory[(size_t) i].v); presetName = factory[(size_t) i].name; currentFactory = i;
}
static bool readPresetFile (const juce::File& f, juce::AudioProcessorValueTreeState& s)
{
    auto xml = juce::XmlDocument::parse (f); if (xml == nullptr || ! xml->hasTagName ("PurrrVoicePreset")) return false;
    for (auto* e : xml->getChildWithTagNameIterator ("p"))   // validated: unknown ids and bad numbers are ignored
        if (auto* p = s.getParameter (e->getStringAttribute ("id")))
        { const double v = e->getDoubleAttribute ("v", std::nan ("")); if (std::isfinite (v)) p->setValueNotifyingHost (p->convertTo0to1 ((float) v)); }
    return true;
}
static bool writePresetFile (const juce::File& f, juce::AudioProcessorValueTreeState& s, const juce::String& name)
{
    juce::XmlElement x ("PurrrVoicePreset"); x.setAttribute ("name", name); x.setAttribute ("version", JucePlugin_VersionString);
    for (auto* p : s.processor.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
    { auto* e = x.createNewChildElement ("p"); e->setAttribute ("id", r->paramID); e->setAttribute ("v", r->convertFrom0to1 (r->getValue())); }
    f.getParentDirectory().createDirectory(); return x.writeTo (f);
}
bool PurrVoiceProcessor::loadUser (const juce::String& name)
{
    resetToDefaults(); if (! readPresetFile (presetDir().getChildFile (juce::File::createLegalFileName (name) + ".pvcpreset"), apvts)) return false;
    presetName = name; currentFactory = -1; jumpVoltage = true; return true;
}
bool PurrVoiceProcessor::saveUser (const juce::String& name)
{
    if (! writePresetFile (presetDir().getChildFile (juce::File::createLegalFileName (name) + ".pvcpreset"), apvts, name)) return false;
    presetName = name; currentFactory = -1; return true;
}
bool PurrVoiceProcessor::deleteUser (const juce::String& name) { return presetDir().getChildFile (juce::File::createLegalFileName (name) + ".pvcpreset").deleteFile(); }
bool PurrVoiceProcessor::exportTo (const juce::File& f) { return writePresetFile (f, apvts, presetName); }
bool PurrVoiceProcessor::importFrom (const juce::File& f)
{
    resetToDefaults(); if (! readPresetFile (f, apvts)) return false;
    presetName = f.getFileNameWithoutExtension(); currentFactory = -1; jumpVoltage = true; return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PurrVoiceProcessor(); }
