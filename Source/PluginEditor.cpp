#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// WaveformDisplay
//==============================================================================

WaveformDisplay::WaveformDisplay()
{
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    auto bounds = getLocalBounds().reduced(2);

    if (currentZone != nullptr && currentZone->isValid())
    {
        const auto& waveform = currentZone->audioData;
        auto numSamples = waveform.getNumSamples();

        if (numSamples > 0)
        {
            g.setColour(juce::Colour(0xff4a9eff));

            auto width = static_cast<float>(bounds.getWidth());
            auto height = static_cast<float>(bounds.getHeight());
            auto centerY = bounds.getCentreY();

            const float* readPtr = waveform.getReadPointer(0);

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
        g.setFont(14.0f);
        g.drawFittedText("Select a sample or drag files to the list", bounds, juce::Justification::centred, 2);
    }
}

void WaveformDisplay::setZone(const SampleZone* zone)
{
    currentZone = zone;
    repaint();
}

//==============================================================================
// SampleListBox
//==============================================================================

SampleListBox::SampleListBox(SuperSimpleSamplerProcessor& p)
    : processor(p)
{
    listBox.setModel(this);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff252525));
    listBox.setRowHeight(24);
    addAndMakeVisible(listBox);
}

void SampleListBox::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff252525));
}

void SampleListBox::resized()
{
    listBox.setBounds(getLocalBounds());
}

int SampleListBox::getNumRows()
{
    return processor.getNumZones();
}

void SampleListBox::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
        g.fillAll(juce::Colour(0xff3a7bcc));
    else if (row % 2 == 0)
        g.fillAll(juce::Colour(0xff2a2a2a));
    else
        g.fillAll(juce::Colour(0xff252525));

    if (const auto* zone = processor.getZone(row))
    {
        g.setColour(juce::Colours::white);
        g.setFont(13.0f);

        juce::String text = zone->name;
        text += " [" + juce::String(zone->lowNote) + "-" + juce::String(zone->highNote) + "]";

        g.drawText(text, 5, 0, width - 10, height, juce::Justification::centredLeft, true);
    }
}

void SampleListBox::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    processor.setSelectedZoneIndex(row);
    if (onSelectionChanged)
        onSelectionChanged();
}

void SampleListBox::deleteKeyPressed(int lastRowSelected)
{
    processor.removeSampleZone(lastRowSelected);
}

bool SampleListBox::isInterestedInFileDrag(const juce::StringArray& files)
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

void SampleListBox::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& file : files)
    {
        juce::File f(file);
        if (f.existsAsFile())
        {
            processor.addSampleZone(f);
        }
    }
}

void SampleListBox::refresh()
{
    listBox.updateContent();
    listBox.selectRow(processor.getSelectedZoneIndex());
}

//==============================================================================
// ZoneMappingEditor
//==============================================================================

ZoneMappingEditor::ZoneMappingEditor(SuperSimpleSamplerProcessor& p)
    : processor(p)
{
    titleLabel.setText("Zone Mapping", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& labelText,
                               double min, double max, double defaultVal)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        slider.setRange(min, max, 1.0);
        slider.setValue(defaultVal);
        slider.onValueChange = [this]() { updateZone(); };
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    setupSlider(lowNoteSlider, lowNoteLabel, "Low Note:", 0, 127, 0);
    setupSlider(highNoteSlider, highNoteLabel, "High Note:", 0, 127, 127);
    setupSlider(rootNoteSlider, rootNoteLabel, "Root Note:", 0, 127, 60);
    setupSlider(lowVelSlider, lowVelLabel, "Low Vel:", 1, 127, 1);
    setupSlider(highVelSlider, highVelLabel, "High Vel:", 1, 127, 127);

    // Custom text formatting for note names
    auto noteFormatter = [](double value) { return noteNumberToName(static_cast<int>(value)); };
    lowNoteSlider.textFromValueFunction = noteFormatter;
    highNoteSlider.textFromValueFunction = noteFormatter;
    rootNoteSlider.textFromValueFunction = noteFormatter;
}

void ZoneMappingEditor::resized()
{
    auto area = getLocalBounds().reduced(5);

    titleLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);

    const int rowHeight = 25;
    const int labelWidth = 70;

    auto makeRow = [&](juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop(rowHeight);
        label.setBounds(row.removeFromLeft(labelWidth));
        slider.setBounds(row);
        area.removeFromTop(3);
    };

    makeRow(lowNoteLabel, lowNoteSlider);
    makeRow(highNoteLabel, highNoteSlider);
    makeRow(rootNoteLabel, rootNoteSlider);
    makeRow(lowVelLabel, lowVelSlider);
    makeRow(highVelLabel, highVelSlider);
}

void ZoneMappingEditor::setZoneIndex(int index)
{
    currentZoneIndex = index;
    refresh();
}

void ZoneMappingEditor::refresh()
{
    if (const auto* zone = processor.getZone(currentZoneIndex))
    {
        lowNoteSlider.setValue(zone->lowNote, juce::dontSendNotification);
        highNoteSlider.setValue(zone->highNote, juce::dontSendNotification);
        rootNoteSlider.setValue(zone->rootNote, juce::dontSendNotification);
        lowVelSlider.setValue(zone->lowVelocity, juce::dontSendNotification);
        highVelSlider.setValue(zone->highVelocity, juce::dontSendNotification);

        setEnabled(true);
    }
    else
    {
        setEnabled(false);
    }
}

void ZoneMappingEditor::updateZone()
{
    if (currentZoneIndex >= 0)
    {
        processor.updateZoneMapping(
            currentZoneIndex,
            static_cast<int>(lowNoteSlider.getValue()),
            static_cast<int>(highNoteSlider.getValue()),
            static_cast<int>(rootNoteSlider.getValue()),
            static_cast<int>(lowVelSlider.getValue()),
            static_cast<int>(highVelSlider.getValue())
        );
    }
}

juce::String ZoneMappingEditor::noteNumberToName(int noteNumber)
{
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = (noteNumber / 12) - 1;
    int note = noteNumber % 12;
    return juce::String(noteNames[note]) + juce::String(octave);
}

//==============================================================================
// SuperSimpleSamplerEditor
//==============================================================================

SuperSimpleSamplerEditor::SuperSimpleSamplerEditor(SuperSimpleSamplerProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), sampleList(p), zoneMappingEditor(p)
{
    processorRef.addListener(this);

    addAndMakeVisible(sampleList);
    addAndMakeVisible(waveformDisplay);
    addAndMakeVisible(zoneMappingEditor);

    sampleList.onSelectionChanged = [this]()
    {
        updateWaveformDisplay();
        zoneMappingEditor.setZoneIndex(processorRef.getSelectedZoneIndex());
    };

    // Load button
    loadButton.setButtonText("Add Sample");
    loadButton.onClick = [this]() { loadButtonClicked(); };
    addAndMakeVisible(loadButton);

    // Clear button
    clearButton.setButtonText("Clear All");
    clearButton.onClick = [this]() { clearButtonClicked(); };
    addAndMakeVisible(clearButton);

    // Setup sliders
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& labelText)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f));
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

    // Initial state
    updateWaveformDisplay();
    zoneMappingEditor.setZoneIndex(processorRef.getSelectedZoneIndex());

    setSize(700, 500);
}

SuperSimpleSamplerEditor::~SuperSimpleSamplerEditor()
{
    processorRef.removeListener(this);
}

void SuperSimpleSamplerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2d2d2d));

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("Super Simple Sampler", 0, 5, getWidth(), 30, juce::Justification::centred, 1);
}

void SuperSimpleSamplerEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // Title space

    // Left panel: sample list
    auto leftPanel = area.removeFromLeft(180);

    auto buttonRow = leftPanel.removeFromTop(28);
    loadButton.setBounds(buttonRow.removeFromLeft(88));
    buttonRow.removeFromLeft(4);
    clearButton.setBounds(buttonRow);

    leftPanel.removeFromTop(5);
    sampleList.setBounds(leftPanel);

    area.removeFromLeft(10); // Spacing

    // Right panel
    auto rightPanel = area;

    // Top: waveform display
    waveformDisplay.setBounds(rightPanel.removeFromTop(120));
    rightPanel.removeFromTop(10);

    // Middle: zone mapping editor
    zoneMappingEditor.setBounds(rightPanel.removeFromTop(170));
    rightPanel.removeFromTop(10);

    // Bottom: ADSR + Gain knobs
    auto knobArea = rightPanel;
    const int knobWidth = knobArea.getWidth() / 5;
    const int labelHeight = 18;

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

void SuperSimpleSamplerEditor::zonesChanged()
{
    // Called from processor when zones change
    juce::MessageManager::callAsync([this]()
    {
        sampleList.refresh();
        updateWaveformDisplay();
        zoneMappingEditor.setZoneIndex(processorRef.getSelectedZoneIndex());
    });
}

void SuperSimpleSamplerEditor::loadButtonClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select samples to load",
        juce::File{},
        "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg"
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectFiles |
                 juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto files = fc.getResults();
        for (const auto& file : files)
        {
            if (file.existsAsFile())
            {
                processorRef.addSampleZone(file);
            }
        }
    });
}

void SuperSimpleSamplerEditor::clearButtonClicked()
{
    processorRef.clearAllZones();
}

void SuperSimpleSamplerEditor::updateWaveformDisplay()
{
    waveformDisplay.setZone(processorRef.getSelectedZone());
}
