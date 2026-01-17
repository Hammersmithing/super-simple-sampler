#pragma once

#include "PluginProcessor.h"

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();
    void paint(juce::Graphics& g) override;
    void setZone(const SampleZone* zone);

private:
    const SampleZone* currentZone = nullptr;
};

class InstrumentListBox : public juce::Component,
                          public juce::ListBoxModel
{
public:
    InstrumentListBox(SuperSimpleSamplerProcessor& p);
    void resized() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    void refresh();

private:
    SuperSimpleSamplerProcessor& processor;
    juce::ListBox listBox;
    std::vector<InstrumentInfo> instruments;
};

class SampleListBox : public juce::Component,
                      public juce::ListBoxModel
{
public:
    SampleListBox(SuperSimpleSamplerProcessor& p);
    void resized() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    void refresh();

    std::function<void()> onSelectionChanged;

private:
    SuperSimpleSamplerProcessor& processor;
    juce::ListBox listBox;
};

class SuperSimpleSamplerEditor : public juce::AudioProcessorEditor,
                                  public SuperSimpleSamplerProcessor::Listener
{
public:
    explicit SuperSimpleSamplerEditor(SuperSimpleSamplerProcessor&);
    ~SuperSimpleSamplerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Processor listener
    void instrumentChanged() override;

private:
    SuperSimpleSamplerProcessor& processorRef;

    InstrumentListBox instrumentList;
    SampleListBox sampleList;
    WaveformDisplay waveformDisplay;

    juce::TextButton refreshButton;
    juce::TextButton openFolderButton;

    juce::Label instrumentNameLabel;
    juce::Label instrumentAuthorLabel;

    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider sustainSlider;
    juce::Slider releaseSlider;
    juce::Slider gainSlider;

    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label gainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    void updateInstrumentInfo();
    void updateWaveformDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerEditor)
};
