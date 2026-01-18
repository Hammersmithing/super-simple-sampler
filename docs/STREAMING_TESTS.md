# DFD Streaming Test Plan

## The Problem

We implemented disk streaming, sound plays, but we haven't verified:
1. Streaming is actually happening (not just using preload buffer)
2. Disk thread is actually running
3. Memory is actually reduced
4. Ring buffer is being filled and consumed correctly
5. Lock-free communication works

## Test Categories

### Category 1: Instrumentation Tests (Add Debug Logging)

Before we can test, we need to see what's happening inside. Add logging to verify internal state.

---

## Phase 1: Add Observable Debug Output

### Test 1.1: Verify Streaming Mode is Active

**Location:** `PluginProcessor.cpp` - `setStreamingEnabled()`

**Add this logging:**
```cpp
void SuperSimpleSamplerProcessor::setStreamingEnabled(bool enabled)
{
    debugLog(">>> setStreamingEnabled(" + juce::String(enabled ? "true" : "false") + ")");
    // ... existing code
}
```

**Expected:** When you call `setStreamingEnabled(true)`, you see the log.

---

### Test 1.2: Verify Preload Size

**Location:** `PluginProcessor.cpp` - `loadPreloadedSample()`

**Add this logging after loading:**
```cpp
debugLog("Loaded preload: " + sample.name
         + " totalFrames=" + juce::String(sample.totalSampleFrames)
         + " preloadFrames=" + juce::String(sample.preloadSizeFrames)
         + " needsStreaming=" + juce::String(sample.needsStreaming() ? "YES" : "no")
         + " preloadBytes=" + juce::String(sample.preloadBuffer.getNumSamples() * sample.numChannels * 4));
```

**Expected:** For a 10-second stereo sample @ 44.1kHz:
- `totalFrames` = 441000
- `preloadFrames` = 8192 (64KB / 2ch / 4 bytes)
- `needsStreaming` = YES
- `preloadBytes` = 65536

---

### Test 1.3: Verify Disk Thread is Running

**Location:** `DiskStreamer.cpp` - `run()`

**Add this logging:**
```cpp
void DiskStreamer::run()
{
    debugLog(">>> DiskStreamer thread STARTED");
    int loopCount = 0;

    while (!threadShouldExit())
    {
        loopCount++;
        if (loopCount % 200 == 0)  // Every ~1 second
        {
            debugLog("DiskStreamer heartbeat: " + juce::String(loopCount) + " loops");
        }
        // ... rest of loop
    }

    debugLog(">>> DiskStreamer thread STOPPED");
}
```

**Expected:** See "heartbeat" messages appearing every second while streaming is enabled.

---

### Test 1.4: Verify Voice Starts in Streaming Mode

**Location:** `PluginProcessor.cpp` - `handleNoteOnStreaming()`

**Add at the end of the function:**
```cpp
debugLog("Started streaming voice " + juce::String(i)
         + " for note " + juce::String(midiNote)
         + " sample=" + selectedSample->name
         + " needsStreaming=" + juce::String(selectedSample->needsStreaming() ? "YES" : "no"));
```

**Expected:** When you play a note, see which voice was started and whether it needs streaming.

---

### Test 1.5: Verify Disk Thread is Filling Buffers

**Location:** `DiskStreamer.cpp` - `fillVoiceBuffer()`

**Add at start and end:**
```cpp
void DiskStreamer::fillVoiceBuffer(int voiceIndex)
{
    // At start:
    debugLog("fillVoiceBuffer(" + juce::String(voiceIndex) + ") - ENTER");

    // ... existing code ...

    // Before clearing needsData at the end:
    debugLog("fillVoiceBuffer(" + juce::String(voiceIndex)
             + ") filled " + juce::String(totalFramesRead) + " frames"
             + " filePos now=" + juce::String(filePos)
             + " EOF=" + juce::String(voice->hasReachedEndOfFile() ? "yes" : "no"));
}
```

**Wait:** We removed `totalFramesRead`. Add it back for debugging:
```cpp
int totalFramesRead = 0;
// in the loop:
totalFramesRead += framesToRead;
```

**Expected:** See continuous "fillVoiceBuffer" messages while a note is playing and sample needs streaming.

---

### Test 1.6: Verify Ring Buffer Positions are Moving

**Location:** `StreamingVoice.cpp` - `renderNextBlock()` (at end, after processing)

**Add:**
```cpp
static int debugCounter = 0;
if (++debugCounter % 100 == 0)  // Every 100 blocks (~2 seconds at 512 samples)
{
    debugLog("Voice render: readPos=" + juce::String(readPosition.load())
             + " writePos=" + juce::String(writePosition.load())
             + " available=" + juce::String(samplesAvailable())
             + " sourcePos=" + juce::String(static_cast<int64_t>(sourceSamplePosition)));
}
```

**Expected:**
- `readPos` should increase as audio is consumed
- `writePos` should increase as disk fills
- `writePos` should always be >= `readPos`
- Difference (available) should hover around ring buffer size minus low watermark

---

## Phase 2: Create Test Samples

### Test 2.1: Create Known-Length Test Samples

Create test samples of specific lengths to verify streaming behavior:

```bash
# Install sox if needed: brew install sox

# Create test samples directory
mkdir -p ~/Documents/"Super Simple Sampler"/Instruments/StreamingTest/samples

cd ~/Documents/"Super Simple Sampler"/Instruments/StreamingTest/samples

# 1-second sample (short, may not need streaming)
sox -n -r 44100 -c 2 C3_127_01_1sec.wav synth 1 sine 261.63

# 5-second sample (definitely needs streaming)
sox -n -r 44100 -c 2 C4_127_01_5sec.wav synth 5 sine 523.25

# 30-second sample (well beyond ring buffer)
sox -n -r 44100 -c 2 C5_127_01_30sec.wav synth 30 sine 1046.50

# 60-second sample (stress test)
sox -n -r 44100 -c 2 C6_127_01_60sec.wav synth 60 sine 2093
```

---

### Test 2.2: Create Test Instrument Definition

```xml
<?xml version="1.0" encoding="UTF-8"?>
<SuperSimpleSampler version="1.0">
  <meta>
    <name>Streaming Test</name>
    <author>Test</author>
  </meta>
  <samples>
    <sample file="samples/C3_127_01_1sec.wav" rootNote="48" loNote="0" hiNote="48" loVel="1" hiVel="127"/>
    <sample file="samples/C4_127_01_5sec.wav" rootNote="60" loNote="49" hiNote="60" loVel="1" hiVel="127"/>
    <sample file="samples/C5_127_01_30sec.wav" rootNote="72" loNote="61" hiNote="72" loVel="1" hiVel="127"/>
    <sample file="samples/C6_127_01_60sec.wav" rootNote="84" loNote="73" hiNote="84" loVel="1" hiVel="127"/>
  </samples>
</SuperSimpleSampler>
```

Save as: `~/Documents/Super Simple Sampler/Instruments/StreamingTest/instrument.sss`

---

## Phase 3: Behavioral Tests

### Test 3.1: Short Sample (No Streaming Needed)

1. Load "Streaming Test" instrument in streaming mode
2. Play C3 (the 1-second sample)
3. Check logs

**Expected:**
- `needsStreaming = no` (sample fits in preload)
- No `fillVoiceBuffer` calls
- Sound plays correctly

---

### Test 3.2: Medium Sample (Streaming Required)

1. Play C4 (the 5-second sample)
2. Hold note for full duration
3. Check logs

**Expected:**
- `needsStreaming = YES`
- Multiple `fillVoiceBuffer` calls
- Ring buffer positions advancing
- Sound plays for full 5 seconds with no clicks

---

### Test 3.3: Long Sample (Extended Streaming)

1. Play C5 (the 30-second sample)
2. Hold for full duration
3. Check logs

**Expected:**
- Continuous `fillVoiceBuffer` calls throughout
- `readPos` eventually exceeds ring buffer size (proves wraparound works)
- Sound plays for full 30 seconds

---

### Test 3.4: Multiple Voices Simultaneously

1. Play C4, C5, C6 at the same time
2. Hold all notes
3. Check logs

**Expected:**
- See `fillVoiceBuffer` calls for multiple voice indices
- All three sounds play simultaneously
- No dropouts

---

### Test 3.5: Rapid Note Triggering

1. Set polyphony to 16
2. Rapidly play ascending scale on 5-second samples
3. Check logs

**Expected:**
- Multiple voices active
- Disk thread keeping up
- No underrun messages

---

## Phase 4: Memory Verification

### Test 4.1: Memory Usage Comparison

**RAM Mode:**
1. Disable streaming: `processor.setStreamingEnabled(false)`
2. Load a large instrument (many long samples)
3. Check Activity Monitor for memory usage

**Streaming Mode:**
1. Enable streaming: `processor.setStreamingEnabled(true)`
2. Load same instrument with `loadInstrumentStreaming()`
3. Check Activity Monitor for memory usage

**Expected:**
- Streaming mode uses significantly less memory
- For 100 samples @ 10s stereo: ~336MB (RAM) vs ~6.4MB (streaming)

---

### Test 4.2: Memory Logging

Add to `loadInstrumentStreaming()`:

```cpp
size_t totalPreloadBytes = 0;
for (const auto& sample : preloadedSamples)
{
    totalPreloadBytes += sample.preloadBuffer.getNumSamples()
                       * sample.numChannels
                       * sizeof(float);
}
debugLog("Total preload memory: " + juce::String(totalPreloadBytes / 1024) + " KB");
```

**Expected:** Much smaller than full sample data.

---

## Phase 5: Edge Case Tests

### Test 5.1: Underrun Simulation

Temporarily increase `diskThreadPollMs` to 500ms (slow polling):

```cpp
constexpr int diskThreadPollMs = 500;  // Was 5
```

1. Play a streaming sample
2. Listen for fade-out/clicks

**Expected:**
- May hear fade-out as buffer empties
- Should NOT hear clicks (underrun fade should prevent)

**Cleanup:** Restore to 5ms after test.

---

### Test 5.2: End of File

1. Play 5-second sample
2. Let it play to natural end
3. Check logs

**Expected:**
- See `EOF=yes` in fillVoiceBuffer logs
- Voice stops cleanly
- No lingering noise

---

### Test 5.3: Note Release During Streaming

1. Play 30-second sample
2. Release after 2 seconds
3. Check that release envelope works

**Expected:**
- Sound fades according to release time
- No abrupt stop
- Voice freed after release

---

### Test 5.4: Pitch Shifting with Streaming

1. Play note that causes pitch shift (play D4 which uses C4 sample pitched up slightly)
2. Listen for audio quality

**Expected:**
- Pitch is correct
- No artifacts from ring buffer wraparound
- Interpolation working

---

## Phase 6: Stress Tests

### Test 6.1: Sustain Pedal Stress

1. Hold sustain pedal
2. Play many notes rapidly
3. Check for underruns

**Expected:**
- All voices playing
- Disk thread keeping up
- No dropouts

---

### Test 6.2: Maximum Polyphony

1. Set polyphony to 64
2. Try to trigger 64 simultaneous streaming voices
3. Check CPU and disk activity

**Expected:**
- System remains responsive
- Some voices may be stolen
- No crashes

---

## Test Results Checklist

| Test | Status | Notes |
|------|--------|-------|
| 1.1 Streaming mode active | ⬜ | |
| 1.2 Preload size correct | ⬜ | |
| 1.3 Disk thread running | ⬜ | |
| 1.4 Voice starts streaming | ⬜ | |
| 1.5 Buffer filling | ⬜ | |
| 1.6 Ring positions moving | ⬜ | |
| 2.1 Test samples created | ⬜ | |
| 2.2 Test instrument created | ⬜ | |
| 3.1 Short sample (no stream) | ⬜ | |
| 3.2 Medium sample (stream) | ⬜ | |
| 3.3 Long sample (extended) | ⬜ | |
| 3.4 Multiple voices | ⬜ | |
| 3.5 Rapid triggering | ⬜ | |
| 4.1 Memory comparison | ⬜ | |
| 4.2 Memory logging | ⬜ | |
| 5.1 Underrun simulation | ⬜ | |
| 5.2 End of file | ⬜ | |
| 5.3 Note release | ⬜ | |
| 5.4 Pitch shifting | ⬜ | |
| 6.1 Sustain stress | ⬜ | |
| 6.2 Max polyphony | ⬜ | |

---

## Summary

The core issue is: **we have no visibility into whether streaming is actually happening.**

### Immediate Next Steps:

1. **Add debug logging** (Phase 1) - This is the most important. We can't test what we can't see.

2. **Create test samples** (Phase 2) - Known-length samples let us verify behavior.

3. **Run behavioral tests** (Phase 3) - With logging, verify each component.

4. **Fix any issues found** - Iterate until all tests pass.

### Key Questions to Answer:

1. Is `needsStreaming()` returning true for large samples?
2. Is the disk thread actually calling `fillVoiceBuffer()`?
3. Are ring buffer positions advancing?
4. Is the audio actually coming from the ring buffer (not just preload)?

Without logging, we're flying blind.
