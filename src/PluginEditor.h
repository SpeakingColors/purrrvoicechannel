#pragma once
#include "PluginProcessor.h"

// The plugin window is the approved mockup: the static faceplate (labels, scales, panels, wood) is drawn from
// assets/ui/faceplate.png, made from the mockup by tools/extract_layout.py, and every live part (knobs, LEDs,
// switches, displays, meters, readouts, preset bar) is drawn here at the positions in src/ui/LayoutGen.h.
class PurrVoiceEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PurrVoiceEditor (PurrVoiceProcessor&);
    ~PurrVoiceEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}
    void resetZoom();
    static constexpr int kDefaultW = 1540;   // 100 % = 70 % of the 2200 px design size (fits a 1080p screen)
    struct Content;
private:
    void timerCallback() override;
    PurrVoiceProcessor& proc;
    std::unique_ptr<Content> content;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> border;
    std::unique_ptr<juce::ResizableCornerComponent> corner;
    juce::TooltipWindow tips { nullptr, 1000 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrVoiceEditor)
};
