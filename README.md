# Super Simple Sampler

A sampler VST3/AU plugin built with JUCE, with the long-term goal of becoming a powerful, flexible sampler inspired by Decent Sampler and Native Instruments Kontakt.

## Vision

Create a sampler that combines the best aspects of:
- **Decent Sampler** - Simple, lightweight, easy to create instruments
- **Native Instruments Kontakt** - Deep scripting, advanced modulation, professional sound design

## Long-Term Goals

### Core Sampler Engine
- [ ] Multi-sample support with velocity layers
- [ ] Round-robin sample playback
- [ ] Key and velocity mapping with crossfades
- [ ] Multiple sample formats (WAV, AIFF, FLAC, MP3)
- [ ] Streaming from disk for large libraries
- [ ] Sample start/end, loop points with crossfade

### Sound Shaping
- [ ] ADSR amplitude envelope (per voice)
- [ ] Filter section (LP, HP, BP, Notch) with envelope
- [ ] Pitch envelope
- [ ] LFOs (multiple, assignable to any parameter)
- [ ] Built-in effects (reverb, delay, chorus, saturation)
- [ ] EQ section

### Modulation System
- [ ] Modulation matrix (any source to any destination)
- [ ] Velocity, key tracking, aftertouch as mod sources
- [ ] MIDI CC mapping
- [ ] Mod wheel, pitch bend with customizable ranges

### Scripting & Customization
- [ ] Lua scripting for custom instrument behavior
- [ ] Custom GUI support for instrument developers
- [ ] XML/JSON-based instrument format
- [ ] Preset/patch management system

### User Interface
- [ ] Waveform display with loop point editing
- [ ] Keyboard mapping editor (drag & drop)
- [ ] Visual modulation routing
- [ ] Resizable, scalable UI
- [ ] Dark/light themes

### Advanced Features
- [ ] Convolution reverb with impulse response loading
- [ ] Time-stretching and pitch-shifting
- [ ] Granular playback mode
- [ ] Articulation switching (keyswitches, UACC)
- [ ] Microtuning support
- [ ] MPE support

## Development Roadmap

### Phase 1: Foundation (Current)
- [ ] Basic single-sample playback
- [ ] MIDI note triggering with pitch shifting
- [ ] Simple ADSR envelope
- [ ] Basic UI with waveform display

### Phase 2: Multi-Sample Support
- [ ] Load multiple samples
- [ ] Key range mapping
- [ ] Velocity layer support
- [ ] Basic sample editor

### Phase 3: Sound Design Basics
- [ ] Filter section
- [ ] Additional envelopes
- [ ] Single LFO
- [ ] Basic effects (reverb, delay)

### Phase 4: Modulation
- [ ] Modulation matrix
- [ ] MIDI CC learn
- [ ] Multiple LFOs
- [ ] Key/velocity tracking

### Phase 5: Instrument Format
- [ ] Define instrument file format
- [ ] Save/load instruments
- [ ] Preset browser
- [ ] Import Decent Sampler format (stretch goal)

### Phase 6: Scripting
- [ ] Lua integration
- [ ] Script API for instrument behavior
- [ ] Custom UI framework

### Phase 7: Polish & Advanced
- [ ] Disk streaming
- [ ] Granular mode
- [ ] Time-stretch
- [ ] MPE support
- [ ] Performance optimization

## Building

### Requirements
- [JUCE](https://github.com/juce-framework/JUCE) (clone to `~/JUCE`)
- CMake 3.22+
- C++17 compiler

### Build Steps
```bash
git clone https://github.com/Hammersmithing/super-simple-sampler.git
cd super-simple-sampler
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## License

MIT

---

*This is an ambitious long-term project. Development will proceed incrementally, with each phase building on the previous.*
