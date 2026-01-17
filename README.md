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

## Current Features

- **XML-based instrument format** (.sss files)
- **Multi-sample support** with key range and velocity layer mapping
- **16-voice polyphony** with pitch interpolation
- **ADSR envelope** (Attack, Decay, Sustain, Release)
- **Gain control**
- **Instrument browser** - scans ~/Documents/Super Simple Sampler/Instruments/
- **Waveform display** for selected samples

## Creating Instruments

### Folder Structure
```
~/Documents/Super Simple Sampler/Instruments/
└── MyPiano/
    ├── instrument.sss      <- XML definition
    └── samples/
        ├── C3_soft.wav
        ├── C3_medium.wav
        └── C3_hard.wav
```

### instrument.sss Format
```xml
<?xml version="1.0" encoding="UTF-8"?>
<SuperSimpleSampler version="1.0">
  <meta>
    <name>My Piano</name>
    <author>Your Name</author>
  </meta>

  <samples>
    <!-- Velocity layers for C3 zone -->
    <sample file="samples/C3_soft.wav" rootNote="48" loNote="36" hiNote="59" loVel="1" hiVel="50"/>
    <sample file="samples/C3_hard.wav" rootNote="48" loNote="36" hiNote="59" loVel="51" hiVel="127"/>

    <!-- Higher zone -->
    <sample file="samples/C5.wav" rootNote="72" loNote="60" hiNote="84" loVel="1" hiVel="127"/>
  </samples>
</SuperSimpleSampler>
```

### Sample Attributes
| Attribute | Description | Default |
|-----------|-------------|---------|
| `file` | Path to audio file (relative to instrument folder) | Required |
| `rootNote` | MIDI note where sample plays at original pitch | 60 (C4) |
| `loNote` | Lowest MIDI note that triggers this sample | 0 |
| `hiNote` | Highest MIDI note that triggers this sample | 127 |
| `loVel` | Lowest velocity that triggers this sample | 1 |
| `hiVel` | Highest velocity that triggers this sample | 127 |

This approach empowers instrument developers to build sample libraries with just a text editor.

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

### Phase 1: Foundation ✅
- [x] Basic single-sample playback
- [x] MIDI note triggering with pitch shifting
- [x] Simple ADSR envelope
- [x] Basic UI with waveform display

### Phase 2: Multi-Sample Support ✅
- [x] Load multiple samples
- [x] Key range mapping
- [x] Velocity layer support
- [x] XML-based instrument format (moved from Phase 5)

### Phase 3: Sound Design Basics (Current)
- [ ] Filter section
- [ ] Additional envelopes
- [ ] Single LFO
- [ ] Basic effects (reverb, delay)

### Phase 4: Modulation
- [ ] Modulation matrix
- [ ] MIDI CC learn
- [ ] Multiple LFOs
- [ ] Key/velocity tracking

### Phase 5: Advanced Instrument Format
- [x] Define XML instrument format (.sss files)
- [x] XML parser for loading instruments
- [x] Instrument browser
- [ ] Package format (.ssslib - zipped XML + samples)
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
