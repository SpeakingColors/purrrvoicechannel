# Roadmap

- [x] Interactive design mockup with the real signal chain (tube, dynamics, EQ, loudness)
- [x] Ten tubes with different voicing, drive-dependent saturation
- [x] Mockup page in this repo (docs/mockup) with a web page that plays it (docs/index.html)
- [x] Init preset renamed; Voicing saved in presets; tube view follows Voicing and Colour
- [x] C++ engine ported from the mockup, parity test against the mockup (max difference -144 dB)
- [x] High tube voltage uses the high-voltage tube curve (bug in the mockup, fixed in both)
- [x] JUCE editor built from the mockup faceplate, every control live and automatable
- [x] Installers: Windows .exe, macOS .pkg with uninstaller, Linux Flatpak and plain VST3, one zip with manual
- [ ] First release checked in a real DAW (Bitwig, Studio One on Bazzite)
- [ ] Measure and tune each tube (harmonics, level, tone) and confirm by ear
- [ ] Switch on GitHub Pages for /docs (owner: Settings, Pages)
- [ ] The other two colour looks in Settings (the plugin has the default look only)
- [ ] Before / After with level match and LUFS inside the plugin (now only in the mockup's preview rig)
