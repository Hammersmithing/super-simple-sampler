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

class SampleListBox : public juce::Component,
                      public juce::ListBoxModel,
                      public juce::FileDragAndDropTarget
{
public:
    SampleListBox(SuperSimpleSamplerProcessor& p);
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void deleteKeyPressed(int lastRowSelected) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void refresh();

    std::function<void()> onSelectionChanged;

private:
    SuperSimpleSamplerProcessor& processor;
    juce::ListBox listBox;
};

class ZoneMappingEditor : public juce::Component
{
public:
    ZoneMappingEditor(SuperSimpleSamplerProcessor& p);
    void resized() override;
    void setZoneIndex(int index);
    void refresh();

private:
    SuperSimpleSamplerProcessor& processor;
    int currentZoneIndex = -1;

    juce::Label titleLabel;

    juce::Slider lowNoteSlider;
    juce::Slider highNoteSlider;
    juce::Slider rootNoteSlider;
    juce::Slider lowVelSlider;
    juce::Slider highVelSlider;

    juce::Label lowNoteLabel;
    juce::Label highNoteLabel;
    juce::Label rootNoteLabel;
    juce::Label lowVelLabel;
    juce::Label highVelLabel;

    void updateZone();

    static juce::String noteNumberToName(int noteNumber);
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
    void zonesChanged() override;

private:
    SuperSimpleSamplerProcessor& processorRef;

    SampleListBox sampleList;
    WaveformDisplay waveformDisplay;
    ZoneMappingEditor zoneMappingEditor;

    juce::TextButton loadButton;
    juce::TextButton clearButton;

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

    std::unique_ptr<juce::FileChooser> fileChooser;

    void loadButtonClicked();
    void clearButtonClicked();
    void updateWaveformDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerEditor)
};
