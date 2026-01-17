#pragma once

#include "PluginProcessor.h"

class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget
{
public:
    WaveformDisplay(SuperSimpleSamplerProcessor& p);
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Drag and drop
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

private:
    SuperSimpleSamplerProcessor& processor;
    bool isDraggingOver = false;
};

class SuperSimpleSamplerEditor : public juce::AudioProcessorEditor
{
public:
    explicit SuperSimpleSamplerEditor(SuperSimpleSamplerProcessor&);
    ~SuperSimpleSamplerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SuperSimpleSamplerProcessor& processorRef;

    WaveformDisplay waveformDisplay;

    juce::TextButton loadButton;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuperSimpleSamplerEditor)
};
