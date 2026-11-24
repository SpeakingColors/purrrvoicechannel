**Test build.** First plugin build of Purrr Voice Channel, not yet checked in a real DAW. Please report anything odd.

One zip: Windows installer (.exe), macOS installer (.pkg, not notarized), Linux Flatpak extension and plain .vst3, the PDF manual, INSTALL.txt and SHA-256 sums. Everything works offline; the plugin collects nothing and makes no network calls.

- The window is the approved mockup: the faceplate is taken from it and every knob, switch, display and meter is drawn live at the same place. Every control is automatable.
- The audio engine is the mockup's engine ported line by line to C++, checked sample for sample against the mockup (max difference -144 dB, rounding only).
- Fixed: High tube voltage now really uses the high-voltage tube curve (before it only changed gain and bandwidth).
- Not in this build: the other two colour looks, and the mockup's preview rig (before/after with level match, LUFS).
