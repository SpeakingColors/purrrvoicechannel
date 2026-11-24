// Factory presets, the same list and values as the mockup (docs/mockup/index.html, P(...)).
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
namespace pvc {
struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> v; };
inline std::vector<Preset> factoryPresets()
{
    // ids as in the plugin: tube = index (0 Kit Cat .. 9 Alley Cat), eqpre 1 = pre comp, emode 1 = gate, lf 1 = 150 Hz, hf 1 = 15 kHz
    return {
        { "Init (clean start)", { { "tcol", 0 } } },
        { "Lead Vocal, polished", { { "lc", 1 }, { "tube", 2 }, { "tcol", 45 }, { "thr", -22 }, { "ratio", 2.5f }, { "att", 8 }, { "rel", 300 }, { "ds", 40 }, { "dsf", 7000 }, { "eqpre", 1 }, { "m1g", -2 }, { "m1f", 300 }, { "m2g", 2.5f }, { "m2f", 3500 }, { "hg", 3 }, { "hf", 1 } } },
        { "Rap / Spoken, tight", { { "lc", 1 }, { "tube", 4 }, { "tcol", 50 }, { "thr", -26 }, { "ratio", 3.5f }, { "att", 3 }, { "rel", 200 }, { "ds", 55 }, { "dsf", 6500 }, { "m1g", -2 }, { "m1f", 250 }, { "m2g", 3 }, { "m2f", 4000 }, { "hg", 2 }, { "hf", 0 } } },
        { "Warm Tube Vocal", { { "tube", 3 }, { "tcol", 70 }, { "gpos", 12 }, { "lc", 1 }, { "thr", -20 }, { "ratio", 2 }, { "att", 15 }, { "rel", 500 }, { "lg", 2 }, { "lf", 1 }, { "m2g", 1.5f }, { "m2f", 3000 }, { "hg", 2 }, { "hf", 1 } } },
        { "Hardstyle Vocal Chop", { { "lc", 1 }, { "tube", 4 }, { "tcol", 60 }, { "thr", -28 }, { "ratio", 6 }, { "att", 2 }, { "rel", 150 }, { "ds", 30 }, { "dsf", 8000 }, { "eqpre", 1 }, { "m1g", -3 }, { "m1f", 400 }, { "m2g", 4 }, { "m2f", 3000 }, { "hg", 5 }, { "hf", 1 } } },
        { "Podcast / Voice-over, clean", { { "lc", 1 }, { "tube", 0 }, { "tcol", 25 }, { "thr", -24 }, { "ratio", 3 }, { "att", 6 }, { "rel", 250 }, { "ds", 35 }, { "dsf", 6500 }, { "ethr", -46 }, { "emode", 0 }, { "m1g", -1.5f }, { "m1f", 200 }, { "m2g", 2 }, { "m2f", 4500 }, { "hg", 1.5f }, { "hf", 1 } } },
        { "Limiter (catch peaks)", { { "thr", -6 }, { "ratio", 20 }, { "att", 0.5f }, { "rel", 150 }, { "tcol", 20 } } },
        { "Noisy Room Clean-up", { { "lc", 1 }, { "tube", 0 }, { "tcol", 20 }, { "ethr", -44 }, { "emode", 1 }, { "thr", -22 }, { "ratio", 2 }, { "att", 10 }, { "rel", 300 }, { "m1g", -3 }, { "m1f", 250 } } },
        { "Heavy Colour (12AX7 Aged)", { { "tube", 9 }, { "tcol", 100 }, { "gpos", 28 }, { "tvolt", 0 }, { "lc", 1 }, { "thr", -20 }, { "ratio", 3 }, { "att", 10 }, { "rel", 300 } } },
        { "Bass DI, tube warmth", { { "gpos", 25 }, { "tube", 2 }, { "tcol", 55 }, { "thr", -22 }, { "ratio", 4 }, { "att", 20 }, { "rel", 250 }, { "lg", 3 }, { "lf", 0 }, { "m1g", 2 }, { "m1f", 800 }, { "hg", 1.5f }, { "hf", 0 } } },
    };
}
}
