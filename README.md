# Super Simple Sampler

A sampler VST3/AU plugin built with JUCE, inspired by Decent Sampler and Native Instruments Kontakt.

## Current Features

- **XML-based instrument format** (.sss files)
- **Multi-sample support** with key range and velocity layer mapping
- **Round-robin sample selection** - randomly selects from matching samples
- **Velocity layers** - up to 127 velocity layers per note
- **Polyphony control** - 1 to 64 voices, adjustable in UI
- **Sustain pedal support** (MIDI CC 64)
- **ADSR envelope** (Attack, Decay, Sustain, Release)
- **Gain control**
- **Pitch-down only** - samples are never pitched up, only down
- **Instrument browser** - scans `~/Documents/Super Simple Sampler/Instruments/`
- **Waveform display** for selected samples
- **Python build tool** - auto-generates instrument.sss from sample folders
- **DFD (Direct From Disk) Streaming** - load 100GB+ libraries without running out of RAM

## Sample Naming Convention

Samples should follow this naming pattern:

```
{Note}_{Velocity}_{RoundRobin}_{OptionalSuffix}.wav
```

### Examples
| Filename | Note | Velocity | Round Robin |
|----------|------|----------|-------------|
| `C3_033_01.wav` | C3 | 33 | 1 |
| `C#3_127_02.wav` | C#3 | 127 | 2 |
| `Db3_064_01.wav` | Db3 | 64 | 1 |
| `F#4_100_03_piano.wav` | F#4 | 100 | 3 |

### Velocity Value = Ceiling

The velocity number in the filename is the **ceiling** (highest velocity) for that layer:

| Filename Velocity | Actual Range |
|-------------------|--------------|
| `_001_` | 1 only |
| `_033_` | 2-33 |
| `_064_` | 34-64 |
| `_096_` | 65-96 |
| `_127_` | 97-127 |

### Pitch Behavior

Samples are **only pitched down**, never up:
- Each sample's `hiNote` equals its `rootNote`
- Notes above the highest sample won't trigger any sound
- This prevents unnatural upward pitch shifting

## Creating Instruments

### Option 1: Use the Build Tool (Recommended)

The Python build tool automatically generates instrument.sss files from a folder of samples:

```bash
cd tools
python build_instrument.py /path/to/samples --name "My Piano" --author "Your Name"
```

This will:
1. Scan for audio files (.wav, .aiff, .flac, .mp3, .ogg)
2. Parse filenames using the naming convention
3. Calculate velocity ranges (ceiling-based)
4. Calculate key ranges (pitch-down only)
5. Generate instrument.sss in the parent folder

#### Build Tool Options
```
python build_instrument.py <samples_dir> [options]

Options:
  -n, --name      Instrument name (default: "My Instrument")
  -a, --author    Author name
  -o, --output    Output path for instrument.sss
```

### Option 2: Manual XML Creation

#### Folder Structure
```
~/Documents/Super Simple Sampler/Instruments/
└── MyPiano/
    ├── instrument.sss      <- XML definition
    └── samples/
        ├── C3_001_01.wav
        ├── C3_033_01.wav
        ├── C3_064_01.wav
        └── ...
```

#### instrument.sss Format
```xml
<?xml version="1.0" encoding="UTF-8"?>
<SuperSimpleSampler version="1.0">
  <meta>
    <name>My Piano</name>
    <author>Your Name</author>
  </meta>

  <samples>
    <!-- C3 velocity layer 33 (covers vel 2-33) with 3 round robins -->
    <sample file="samples/C3_033_01.wav" rootNote="48" loNote="0" hiNote="48" loVel="2" hiVel="33"/>
    <sample file="samples/C3_033_02.wav" rootNote="48" loNote="0" hiNote="48" loVel="2" hiVel="33"/>
    <sample file="samples/C3_033_03.wav" rootNote="48" loNote="0" hiNote="48" loVel="2" hiVel="33"/>

    <!-- D3 velocity layer 33 (covers vel 2-33) -->
    <sample file="samples/D3_033_01.wav" rootNote="50" loNote="49" hiNote="50" loVel="2" hiVel="33"/>
  </samples>
</SuperSimpleSampler>
```

### Sample Attributes

| Attribute | Description | Default |
|-----------|-------------|---------|
| `file` | Path to audio file (relative to instrument folder) | Required |
| `rootNote` | MIDI note where sample plays at original pitch | 60 (C4) |
| `loNote` | Lowest MIDI note that triggers this sample | 0 |
| `hiNote` | Highest MIDI note (should equal rootNote for pitch-down only) | 127 |
| `loVel` | Lowest velocity that triggers this sample | 1 |
| `hiVel` | Highest velocity that triggers this sample | 127 |

## UI Controls

| Control | Range | Description |
|---------|-------|-------------|
| Attack | 0.001 - 5.0s | Envelope attack time |
| Decay | 0.001 - 5.0s | Envelope decay time |
| Sustain | 0 - 100% | Envelope sustain level |
| Release | 0.001 - 10.0s | Envelope release time |
| Gain | 0 - 200% | Output gain |
| Voices | 1 - 64 | Maximum polyphony |

## DFD (Direct From Disk) Streaming

The DFD streaming system allows loading large sample libraries (100GB+) that would otherwise exceed available RAM.

### How It Works

1. **Preload**: Only the first 64KB of each sample is loaded into RAM
2. **Ring Buffer**: Each voice has a 32KB ring buffer (~743ms at 44.1kHz)
3. **Background Thread**: A dedicated disk thread fills ring buffers as voices play
4. **Lock-Free**: Audio thread and disk thread communicate via atomic operations

### Architecture

```
Audio Thread                         Disk Thread
     |                                    |
     | 1. Check samplesAvailable()        |
     | 2. If low: needsData = true        |
     |                                    |
     |                              3. Poll needsData
     |                              4. Read disk -> ring buffer
     |                              5. writePosition += frames
     |                                    |
     | 6. Read ring buffer                |
     | 7. readPosition += frames          |
```

### Files

| File | Purpose |
|------|---------|
| `Source/DiskStreaming.h` | Core types: `PreloadedSample`, `StreamRequest`, constants |
| `Source/StreamingVoice.h/cpp` | Streaming voice with ring buffer and lock-free positions |
| `Source/DiskStreamer.h/cpp` | Background disk read thread |

### Configuration Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `preloadSizeBytes` | 64KB | Initial sample data loaded to RAM |
| `ringBufferFrames` | 32768 | Ring buffer size (~743ms at 44.1kHz) |
| `lowWatermarkFrames` | 8192 | Request data when buffer falls below this (~185ms) |
| `diskReadFrames` | 4096 | Batch read size per disk operation (~93ms) |
| `diskThreadPollMs` | 5 | Disk thread polling interval |
| `maxStreamingVoices` | 64 | Maximum concurrent streaming voices |

### Memory Usage Comparison

| Mode | 100 samples @ 10s stereo 44.1kHz |
|------|-----------------------------------|
| RAM mode | ~336 MB |
| Streaming mode | ~6.4 MB (preloads only) |

### Enabling Streaming

```cpp
// In your code
processor.setStreamingEnabled(true);
processor.loadInstrumentStreaming(instrumentFile);
```

### Error Handling

- **Buffer Underrun**: Quick 64-sample fade out to avoid clicks
- **End of Sample**: Automatic transition to release envelope
- **File Read Errors**: Voice deactivated, error logged

### SSD Recommended

DFD streaming works best with SSDs. On HDDs, reduce polyphony to avoid underruns during heavy playback.

## Design Philosophy

Inspired by Decent Sampler's approach:

- **Instruments are code** - Human-readable XML format, no proprietary binaries
- **Shareable and hackable** - Create, share, and modify with any text editor
- **Version controllable** - Text-based format works great with Git
- **Automation friendly** - Python tool generates instruments from sample folders

## Long-Term Goals

### Core Sampler Engine
- [x] Multi-sample support with velocity layers
- [x] Round-robin sample playback
- [x] Key and velocity mapping
- [x] Sustain pedal support
- [ ] Key/velocity crossfades
- [ ] Multiple sample formats (WAV, AIFF, FLAC, MP3)
- [x] Streaming from disk for large libraries (DFD)
- [ ] Sample start/end, loop points with crossfade

### Sound Shaping
- [x] ADSR amplitude envelope (per voice)
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
- [ ] Preset/patch management system

### User Interface
- [x] Waveform display
- [x] Polyphony control
- [ ] Loop point editing
- [ ] Keyboard mapping editor (drag & drop)
- [ ] Visual modulation routing
- [ ] Resizable, scalable UI
- [ ] Dark/light themes

### Advanced Features
- [ ] Package format (.ssslib - zipped XML + samples)
- [ ] Import Decent Sampler .dspreset format
- [ ] Convolution reverb with impulse response loading
- [ ] Time-stretching and pitch-shifting
- [ ] Granular playback mode
- [ ] Articulation switching (keyswitches, UACC)
- [ ] Microtuning support
- [ ] MPE support

## Building

### Requirements
- [JUCE](https://github.com/juce-framework/JUCE) (clone to `~/JUCE`)
- CMake 3.22+
- C++17 compiler
- Python 3.x (for build tool)

### Build Steps
```bash
git clone https://github.com/Hammersmithing/super-simple-sampler.git
cd super-simple-sampler
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Output Locations
- **VST3**: `build/SuperSimpleSampler_artefacts/Release/VST3/`
- **AU**: `build/SuperSimpleSampler_artefacts/Release/AU/`
- **Standalone**: `build/SuperSimpleSampler_artefacts/Release/Standalone/`

## License

MIT

---

*Super Simple Sampler - Making sample-based instruments accessible to everyone.*
