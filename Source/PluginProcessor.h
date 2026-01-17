#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleZone.h"

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

    // Sample zone management
    int addSampleZone(const juce::File& file);
    int addSampleZone(const juce::File& file, int lowNote, int highNote, int rootNote, int lowVel, int highVel);
    void removeSampleZone(int index);
    void clearAllZones();
    void updateZoneMapping(int index, int lowNote, int highNote, int rootNote, int lowVel, int highVel);

    // Getters
    int getNumZones() const { return static_cast<int>(sampleZones.size()); }
    const SampleZone* getZone(int index) const;
    bool hasAnySamples() const { return !sampleZones.empty(); }

    // Selected zone for UI
    int getSelectedZoneIndex() const { return selectedZoneIndex; }
    void setSelectedZoneIndex(int index) { selectedZoneIndex = index; }
    const SampleZone* getSelectedZone() const;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // Listener for UI updates
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void zonesChanged() = 0;
    };

    void addListener(Listener* listener) { listeners.add(listener); }
    void removeListener(Listener* listener) { listeners.remove(listener); }

private:
    juce::AudioProcessorValueTreeState parameters;

    juce::Synthesiser sampler;
    juce::AudioFormatManager formatManager;

    std::vector<SampleZone> sampleZones;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerProcessor)
};
