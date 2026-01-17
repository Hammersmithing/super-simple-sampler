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
        g.drawFittedText("No sample selected", bounds, juce::Justification::centred, 1);
    }
}

void WaveformDisplay::setZone(const SampleZone* zone)
{
    currentZone = zone;
    repaint();
}

//==============================================================================
// InstrumentListBox
//==============================================================================

InstrumentListBox::InstrumentListBox(SuperSimpleSamplerProcessor& p)
    : processor(p)
{
    listBox.setModel(this);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff252525));
    listBox.setRowHeight(28);
    addAndMakeVisible(listBox);
    refresh();
}

void InstrumentListBox::resized()
{
    listBox.setBounds(getLocalBounds());
}

int InstrumentListBox::getNumRows()
{
    return static_cast<int>(instruments.size());
}

void InstrumentListBox::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
        g.fillAll(juce::Colour(0xff3a7bcc));
    else if (row % 2 == 0)
        g.fillAll(juce::Colour(0xff2a2a2a));
    else
        g.fillAll(juce::Colour(0xff252525));

    if (row >= 0 && row < static_cast<int>(instruments.size()))
    {
        const auto& info = instruments[static_cast<size_t>(row)];

        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(info.name, 8, 0, width - 16, height, juce::Justification::centredLeft, true);
    }
}

void InstrumentListBox::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    // Single click just selects
    listBox.selectRow(row);
}

void InstrumentListBox::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    // Double click loads the instrument
    if (row >= 0 && row < static_cast<int>(instruments.size()))
    {
        processor.loadInstrument(row);
    }
}

void InstrumentListBox::refresh()
{
    processor.refreshInstrumentList();
    instruments = processor.getAvailableInstruments();
    listBox.updateContent();
}

//==============================================================================
// SampleListBox
//==============================================================================

SampleListBox::SampleListBox(SuperSimpleSamplerProcessor& p)
    : processor(p)
{
    listBox.setModel(this);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff252525));
    listBox.setRowHeight(22);
    addAndMakeVisible(listBox);
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
        g.setFont(12.0f);

        juce::String text = zone->name;
        text += " [" + juce::String(zone->lowNote) + "-" + juce::String(zone->highNote) + "]";
        text += " v" + juce::String(zone->lowVelocity) + "-" + juce::String(zone->highVelocity);

        g.drawText(text, 5, 0, width - 10, height, juce::Justification::centredLeft, true);
    }
}

void SampleListBox::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    processor.setSelectedZoneIndex(row);
    listBox.selectRow(row);
    if (onSelectionChanged)
        onSelectionChanged();
}

void SampleListBox::refresh()
{
    listBox.updateContent();
    listBox.selectRow(processor.getSelectedZoneIndex());
}

//==============================================================================
// SuperSimpleSamplerEditor
//==============================================================================

SuperSimpleSamplerEditor::SuperSimpleSamplerEditor(SuperSimpleSamplerProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), instrumentList(p), sampleList(p)
{
    processorRef.addZoneListener(this);

    // Instrument list
    addAndMakeVisible(instrumentList);

    // Sample list
    addAndMakeVisible(sampleList);
    sampleList.onSelectionChanged = [this]() { updateWaveformDisplay(); };

    // Waveform display
    addAndMakeVisible(waveformDisplay);

    // Refresh button
    refreshButton.setButtonText("Refresh");
    refreshButton.onClick = [this]()
    {
        instrumentList.refresh();
    };
    addAndMakeVisible(refreshButton);

    // Open folder button
    openFolderButton.setButtonText("Open Folder");
    openFolderButton.onClick = []()
    {
        InstrumentLoader::ensureInstrumentsFolderExists();
        InstrumentLoader::getInstrumentsFolder().startAsProcess();
    };
    addAndMakeVisible(openFolderButton);

    // Instrument info labels
    instrumentNameLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    instrumentNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(instrumentNameLabel);

    instrumentAuthorLabel.setFont(juce::FontOptions(12.0f));
    instrumentAuthorLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(instrumentAuthorLabel);

    // Last played sample display (debug)
    lastPlayedLabel.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    lastPlayedLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    lastPlayedLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff333333));
    lastPlayedLabel.setText("Play a note to see RR info", juce::dontSendNotification);
    addAndMakeVisible(lastPlayedLabel);

    // Start timer to update last played display
    startTimerHz(30);

    // Setup ADSR sliders
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& labelText)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(label);
    };

    setupSlider(attackSlider, attackLabel, "Attack");
    setupSlider(decaySlider, decayLabel, "Decay");
    setupSlider(sustainSlider, sustainLabel, "Sustain");
    setupSlider(releaseSlider, releaseLabel, "Release");
    setupSlider(gainSlider, gainLabel, "Gain");
    setupSlider(polyphonySlider, polyphonyLabel, "Voices");

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
    polyphonyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getParameters(), "polyphony", polyphonySlider);

    // Initial state
    updateInstrumentInfo();
    updateWaveformDisplay();
    sampleList.refresh();

    setSize(620, 380);
}

SuperSimpleSamplerEditor::~SuperSimpleSamplerEditor()
{
    stopTimer();
    processorRef.removeZoneListener(this);
}

void SuperSimpleSamplerEditor::timerCallback()
{
    auto lastPlayed = processorRef.getLastPlayedSample();
    if (lastPlayed.isNotEmpty())
        lastPlayedLabel.setText("Last: " + lastPlayed, juce::dontSendNotification);
}

void SuperSimpleSamplerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2d2d2d));

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("Super Simple Sampler", 0, 5, getWidth(), 30, juce::Justification::centred, 1);

    // Section labels
    g.setFont(12.0f);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("INSTRUMENTS", 10, 38, 170, 16, juce::Justification::centredLeft);
    g.drawText("SAMPLES", 190, 38, 150, 16, juce::Justification::centredLeft);
}

void SuperSimpleSamplerEditor::resized()
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(25); // Title space

    // Top section labels space
    area.removeFromTop(16);

    // Left panel: instrument list
    auto leftPanel = area.removeFromLeft(140);

    auto buttonRow = leftPanel.removeFromBottom(24);
    refreshButton.setBounds(buttonRow.removeFromLeft(65));
    buttonRow.removeFromLeft(4);
    openFolderButton.setBounds(buttonRow);

    leftPanel.removeFromBottom(4);
    instrumentList.setBounds(leftPanel);

    area.removeFromLeft(6); // Spacing

    // Middle panel: sample list
    auto middlePanel = area.removeFromLeft(160);
    sampleList.setBounds(middlePanel);

    area.removeFromLeft(6); // Spacing

    // Right panel
    auto rightPanel = area;

    // Instrument info
    auto infoArea = rightPanel.removeFromTop(56);
    instrumentNameLabel.setBounds(infoArea.removeFromTop(20));
    instrumentAuthorLabel.setBounds(infoArea.removeFromTop(16));
    lastPlayedLabel.setBounds(infoArea.removeFromTop(18));

    rightPanel.removeFromTop(4);

    // Waveform display
    waveformDisplay.setBounds(rightPanel.removeFromTop(70));
    rightPanel.removeFromTop(6);

    // Knobs - 6 columns
    auto knobArea = rightPanel;
    const int knobWidth = knobArea.getWidth() / 6;
    const int labelHeight = 16;

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

    auto gainArea = knobArea.removeFromLeft(knobWidth);
    gainLabel.setBounds(gainArea.removeFromTop(labelHeight));
    gainSlider.setBounds(gainArea);

    auto polyphonyArea = knobArea;
    polyphonyLabel.setBounds(polyphonyArea.removeFromTop(labelHeight));
    polyphonySlider.setBounds(polyphonyArea);
}

void SuperSimpleSamplerEditor::instrumentChanged()
{
    juce::MessageManager::callAsync([this]()
    {
        updateInstrumentInfo();
        sampleList.refresh();
        updateWaveformDisplay();
    });
}

void SuperSimpleSamplerEditor::updateInstrumentInfo()
{
    if (processorRef.hasInstrumentLoaded())
    {
        const auto& instrument = processorRef.getCurrentInstrument();
        instrumentNameLabel.setText(instrument.info.name, juce::dontSendNotification);

        juce::String authorText = instrument.info.author.isNotEmpty()
            ? "by " + instrument.info.author
            : "";
        authorText += " (" + juce::String(instrument.zones.size()) + " samples)";
        instrumentAuthorLabel.setText(authorText, juce::dontSendNotification);
    }
    else
    {
        instrumentNameLabel.setText("No instrument loaded", juce::dontSendNotification);
        instrumentAuthorLabel.setText("Double-click an instrument to load it", juce::dontSendNotification);
    }
}

void SuperSimpleSamplerEditor::updateWaveformDisplay()
{
    waveformDisplay.setZone(processorRef.getSelectedZone());
}
