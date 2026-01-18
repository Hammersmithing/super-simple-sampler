#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <array>
#include "SampleZone.h"
#include "InstrumentLoader.h"
#include "DiskStreaming.h"
#include "StreamingVoice.h"
#include "DiskStreamer.h"

class SuperSimpleSamplerProcessor : public juce::AudioProcessor
{
public:
    SuperSimpleSamplerProcessor();
    ~SuperSimpleSamplerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Instrument management
    std::vector<InstrumentInfo> getAvailableInstruments();
    void loadInstrument(int index);
    void loadInstrumentFromFile(const juce::File& definitionFile);
    void unloadInstrument();
    void refreshInstrumentList();

    // Getters
    bool hasInstrumentLoaded() const { return currentInstrument.isValid(); }
    const LoadedInstrument& getCurrentInstrument() const { return currentInstrument; }
    int getNumZones() const { return static_cast<int>(currentInstrument.zones.size()); }
    const SampleZone* getZone(int index) const;

    // Selected zone for waveform display
    int getSelectedZoneIndex() const { return selectedZoneIndex; }
    void setSelectedZoneIndex(int index) { selectedZoneIndex = index; }
    const SampleZone* getSelectedZone() const;

    // Round-robin debug info
    juce::String getLastPlayedSample() const { return lastPlayedSample; }

    // Streaming mode controls
    bool isStreamingEnabled() const { return streamingEnabled; }
    void setStreamingEnabled(bool enabled);
    void loadInstrumentStreaming(const juce::File& definitionFile);

    // Get preloaded sample info (for streaming mode)
    const PreloadedSample* getPreloadedSample(int index) const;
    int getNumPreloadedSamples() const { return static_cast<int>(preloadedSamples.size()); }

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // Listener for UI updates
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void instrumentChanged() = 0;
    };

    void addZoneListener(Listener* listener) { listeners.add(listener); }
    void removeZoneListener(Listener* listener) { listeners.remove(listener); }

private:
    juce::AudioProcessorValueTreeState parameters;

    juce::Synthesiser sampler;
    InstrumentLoader instrumentLoader;

    std::vector<InstrumentInfo> availableInstruments;
    LoadedInstrument currentInstrument;
    int selectedZoneIndex = -1;

    juce::ListenerList<Listener> listeners;

    // ADSR parameters
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* polyphonyParam = nullptr;

    void rebuildSampler();
    void updateADSR();
    void notifyListeners();

    // Custom MIDI handling for proper velocity/round-robin selection
    void handleNoteOn(int midiChannel, int midiNote, float velocity);
    void handleNoteOff(int midiChannel, int midiNote, float velocity);
    void handleSustainPedal(bool isDown);
    std::vector<int> findMatchingZones(int midiNote, int velocity);

    // Per-note round-robin counters (like SFZ seq_position)
    std::map<int, int> roundRobinCounters;  // key = MIDI note, value = current position
    juce::String lastPlayedSample;

    // Sustain pedal state
    bool sustainPedalDown = false;

    // ==================== Streaming Mode ====================
    bool streamingEnabled = false;

    // Streaming voices (used when streamingEnabled is true)
    std::array<StreamingVoice, StreamingConstants::maxStreamingVoices> streamingVoices;

    // Background disk streaming thread
    std::unique_ptr<DiskStreamer> diskStreamer;

    // Preloaded samples for streaming mode (replaces full audio data with partial preload)
    std::vector<PreloadedSample> preloadedSamples;

    // Audio format manager for streaming (shared with DiskStreamer)
    juce::AudioFormatManager streamingFormatManager;

    // Streaming-specific processing
    void processBlockStreaming(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void handleNoteOnStreaming(int midiChannel, int midiNote, float velocity);
    void handleNoteOffStreaming(int midiChannel, int midiNote, float velocity);

    // Load a sample with only preload data (for streaming mode)
    bool loadPreloadedSample(const juce::File& sampleFile, PreloadedSample& sample);
    std::vector<int> findMatchingPreloadedSamples(int midiNote, int velocity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerProcessor)
};
