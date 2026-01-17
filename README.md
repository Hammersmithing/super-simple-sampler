# Super Simple Sampler

A sampler VST3/AU plugin built with JUCE, with the long-term goal of becoming a powerful, flexible sampler inspired by Decent Sampler and Native Instruments Kontakt.

## Vision

Create a sampler that combines the best aspects of:
- **Decent Sampler** - Simple, lightweight, easy to create instruments
- **Native Instruments Kontakt** - Deep scripting, advanced modulation, professional sound design

## Design Philosophy: Code-Based Instrument Building

Inspired by Decent Sampler's approach, instruments will be defined using a **human-readable XML format**. This means:

- **Instruments are code** - No proprietary binary formats. Instruments are text files you can read, edit, and version control.
- **Shareable and hackable** - Developers can create, share, and modify instruments with any text editor.
- **Package format** - Instruments are distributed as a single file (zipped XML + samples) for easy sharing.
- **Decent Sampler compatibility** - Long-term goal to import `.dspreset` files so existing libraries can be used.

### Example Instrument Definition (Target Format)
```xml
<SuperSimpleSampler version="1.0">
  <meta>
    <name>My Piano</name>
    <author>ALDEN HAMMERSMITH</author>
  </meta>

  <ui background="wood_panel.png">
    <knob id="cutoff" x="50" y="100" param="filter.cutoff"/>
    <knob id="resonance" x="120" y="100" param="filter.resonance"/>
  </ui>

  <groups>
    <group name="sustain" attack="0.01" release="0.3">
      <sample path="samples/C3_pp.wav" rootNote="48" loNote="36" hiNote="54" loVel="1" hiVel="40"/>
      <sample path="samples/C3_mf.wav" rootNote="48" loNote="36" hiNote="54" loVel="41" hiVel="90"/>
      <sample path="samples/C3_ff.wav" rootNote="48" loNote="36" hiNote="54" loVel="91" hiVel="127"/>
    </group>
  </groups>

  <effects>
    <filter type="lowpass" cutoff="8000" resonance="0.5"/>
    <reverb mix="0.2" size="0.6"/>
  </effects>

  <modulation>
    <route source="velocity" dest="filter.cutoff" amount="0.4"/>
    <route source="modwheel" dest="lfo1.amount" amount="1.0"/>
  </modulation>

  <script src="scripts/custom_behavior.lua"/>
</SuperSimpleSampler>
```

This approach empowers instrument developers to build sophisticated sample libraries without needing to compile code.

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
- [ ] Define XML instrument format (.sss files)
- [ ] XML parser for loading instruments
- [ ] Package format (.ssslib - zipped XML + samples)
- [ ] Save/load instruments
- [ ] Preset browser
- [ ] Import Decent Sampler .dspreset format

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
