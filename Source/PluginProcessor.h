#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

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

    // Sample loading
    void loadSample(const juce::File& file);
    void loadSampleFromData(const void* data, size_t size, const juce::String& name);
    bool hasSampleLoaded() const { return sampleLoaded; }

    // Get waveform for display
    const juce::AudioBuffer<float>& getWaveform() const { return waveform; }
    int getSampleRootNote() const { return rootNote; }

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

private:
    juce::AudioProcessorValueTreeState parameters;

    juce::Synthesiser sampler;
    juce::AudioFormatManager formatManager;

    juce::AudioBuffer<float> waveform;
    bool sampleLoaded = false;
    int rootNote = 60; // Middle C

    // ADSR parameters
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* gainParam = nullptr;

    void updateADSR();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerProcessor)
};
