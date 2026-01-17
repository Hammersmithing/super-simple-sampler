#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <random>
#include "SampleZone.h"
#include "InstrumentLoader.h"

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

    void rebuildSampler();
    void updateADSR();
    void notifyListeners();

    // Custom MIDI handling for proper velocity/round-robin selection
    void handleNoteOn(int midiChannel, int midiNote, float velocity);
    void handleNoteOff(int midiChannel, int midiNote, float velocity);
    std::vector<int> findMatchingZones(int midiNote, int velocity);

    // Random number generator for round-robin selection
    std::mt19937 randomGenerator{std::random_device{}()};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerProcessor)
};
