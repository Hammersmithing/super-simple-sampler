#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// WaveformDisplay
//==============================================================================

WaveformDisplay::WaveformDisplay(SuperSimpleSamplerProcessor& p)
    : processor(p)
{
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    auto bounds = getLocalBounds().reduced(2);

    if (isDraggingOver)
    {
        g.setColour(juce::Colour(0xff3a7bcc));
        g.drawRect(bounds, 2);
    }

    if (processor.hasSampleLoaded())
    {
        const auto& waveform = processor.getWaveform();
        auto numSamples = waveform.getNumSamples();
        auto numChannels = waveform.getNumChannels();

        if (numSamples > 0)
        {
            g.setColour(juce::Colour(0xff4a9eff));

            juce::Path path;
            auto width = static_cast<float>(bounds.getWidth());
            auto height = static_cast<float>(bounds.getHeight());
            auto centerY = bounds.getCentreY();

            const float* readPtr = waveform.getReadPointer(0);
            int samplesPerPixel = std::max(1, numSamples / static_cast<int>(width));

            for (int x = 0; x < static_cast<int>(width); ++x)
            {
                int startSample = x * numSamples / static_cast<int>(width);
                int endSample = std::min((x + 1) * numSamples / static_cast<int>(width), numSamples);

                float minVal = 0.0f;
                float maxVal = 0.0f;

                for (int s = startSample; s < endSample; ++s)
                {
                    float sample = readPtr[s];
                    minVal = std::min(minVal, sample);
                    maxVal = std::max(maxVal, sample);
                }

                float yMin = centerY - maxVal * (height * 0.45f);
                float yMax = centerY - minVal * (height * 0.45f);

                g.drawVerticalLine(bounds.getX() + x, yMin, yMax);
            }
        }
    }
    else
    {
        g.setColour(juce::Colours::grey);
        g.setFont(16.0f);
        g.drawFittedText("Drop a sample here or click Load", bounds, juce::Justification::centred, 2);
    }
}

void WaveformDisplay::resized()
{
}

bool WaveformDisplay::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        if (file.endsWith(".wav") || file.endsWith(".aif") || file.endsWith(".aiff") ||
            file.endsWith(".mp3") || file.endsWith(".flac") || file.endsWith(".ogg"))
        {
            return true;
        }
    }
    return false;
}

void WaveformDisplay::filesDropped(const juce::StringArray& files, int, int)
{
    isDraggingOver = false;
    for (const auto& file : files)
    {
        juce::File f(file);
        if (f.existsAsFile())
        {
            processor.loadSample(f);
            repaint();
            break;
        }
    }
}

void WaveformDisplay::fileDragEnter(const juce::StringArray&, int, int)
{
    isDraggingOver = true;
    repaint();
}

void WaveformDisplay::fileDragExit(const juce::StringArray&)
{
    isDraggingOver = false;
    repaint();
}

//==============================================================================
// SuperSimpleSamplerEditor
//==============================================================================

SuperSimpleSamplerEditor::SuperSimpleSamplerEditor(SuperSimpleSamplerProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), waveformDisplay(p)
{
    addAndMakeVisible(waveformDisplay);

    // Load button
    loadButton.setButtonText("Load Sample");
    loadButton.onClick = [this]() { loadButtonClicked(); };
    addAndMakeVisible(loadButton);

    // Setup sliders
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& labelText)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };

    setupSlider(attackSlider, attackLabel, "Attack");
    setupSlider(decaySlider, decayLabel, "Decay");
    setupSlider(sustainSlider, sustainLabel, "Sustain");
    setupSlider(releaseSlider, releaseLabel, "Release");
    setupSlider(gainSlider, gainLabel, "Gain");

    // Attachments
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), "attack", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), "decay", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), "sustain", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), "release", releaseSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), "gain", gainSlider);

    setSize(600, 400);
}

SuperSimpleSamplerEditor::~SuperSimpleSamplerEditor()
{
}

void SuperSimpleSamplerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2d2d2d));

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("Super Simple Sampler", getLocalBounds().removeFromTop(35),
                     juce::Justification::centred, 1);
}

void SuperSimpleSamplerEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // Title space

    // Waveform display at top
    auto waveformArea = area.removeFromTop(150);
    waveformDisplay.setBounds(waveformArea);

    area.removeFromTop(10); // Spacing

    // Load button
    auto buttonArea = area.removeFromTop(30);
    loadButton.setBounds(buttonArea.withSizeKeepingCentre(120, 25));

    area.removeFromTop(10); // Spacing

    // ADSR + Gain knobs
    auto knobArea = area;
    const int knobWidth = knobArea.getWidth() / 5;
    const int labelHeight = 20;

    auto attackArea = knobArea.removeFromLeft(knobWidth);
    attackLabel.setBounds(attackArea.removeFromTop(labelHeight));
    attackSlider.setBounds(attackArea);

    auto decayArea = knobArea.removeFromLeft(knobWidth);
    decayLabel.setBounds(decayArea.removeFromTop(labelHeight));
    decaySlider.setBounds(decayArea);

    auto sustainArea = knobArea.removeFromLeft(knobWidth);
    sustainLabel.setBounds(sustainArea.removeFromTop(labelHeight));
    sustainSlider.setBounds(sustainArea);

    auto releaseArea = knobArea.removeFromLeft(knobWidth);
    releaseLabel.setBounds(releaseArea.removeFromTop(labelHeight));
    releaseSlider.setBounds(releaseArea);

    auto gainArea = knobArea;
    gainLabel.setBounds(gainArea.removeFromTop(labelHeight));
    gainSlider.setBounds(gainArea);
}

void SuperSimpleSamplerEditor::loadButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a sample to load",
        juce::File{},
        "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg"
    );

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            processorRef.loadSample(file);
            waveformDisplay.repaint();
        }
    });
}
