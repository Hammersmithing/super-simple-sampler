#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1),
        "Attack",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.5f),
        0.01f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 3) + " s"; }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1),
        "Decay",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.5f),
        0.1f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 3) + " s"; }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1),
        "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.8f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(int(value * 100)) + "%"; }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1),
        "Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.5f),
        0.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 3) + " s"; }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gain", 1),
        "Gain",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f),
        1.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(int(value * 100)) + "%"; }
    ));

    return { params.begin(), params.end() };
}

SuperSimpleSamplerProcessor::SuperSimpleSamplerProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("SuperSimpleSampler"), createParameterLayout())
{
    formatManager.registerBasicFormats();

    // Add voices to the sampler (polyphony)
    for (int i = 0; i < 16; ++i)
        sampler.addVoice(new juce::SamplerVoice());

    attackParam = parameters.getRawParameterValue("attack");
    decayParam = parameters.getRawParameterValue("decay");
    sustainParam = parameters.getRawParameterValue("sustain");
    releaseParam = parameters.getRawParameterValue("release");
    gainParam = parameters.getRawParameterValue("gain");
}

SuperSimpleSamplerProcessor::~SuperSimpleSamplerProcessor()
{
}

const juce::String SuperSimpleSamplerProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SuperSimpleSamplerProcessor::acceptsMidi() const { return true; }
bool SuperSimpleSamplerProcessor::producesMidi() const { return false; }
bool SuperSimpleSamplerProcessor::isMidiEffect() const { return false; }
double SuperSimpleSamplerProcessor::getTailLengthSeconds() const { return 0.0; }

int SuperSimpleSamplerProcessor::getNumPrograms() { return 1; }
int SuperSimpleSamplerProcessor::getCurrentProgram() { return 0; }
void SuperSimpleSamplerProcessor::setCurrentProgram(int) {}
const juce::String SuperSimpleSamplerProcessor::getProgramName(int) { return {}; }
void SuperSimpleSamplerProcessor::changeProgramName(int, const juce::String&) {}

void SuperSimpleSamplerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampler.setCurrentPlaybackSampleRate(sampleRate);
    updateADSR();
}

void SuperSimpleSamplerProcessor::releaseResources()
{
}

bool SuperSimpleSamplerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SuperSimpleSamplerProcessor::updateADSR()
{
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = attackParam->load();
    adsrParams.decay = decayParam->load();
    adsrParams.sustain = sustainParam->load();
    adsrParams.release = releaseParam->load();

    for (int i = 0; i < sampler.getNumSounds(); ++i)
    {
        if (auto* sound = dynamic_cast<juce::SamplerSound*>(sampler.getSound(i).get()))
        {
            sound->setEnvelopeParameters(adsrParams);
        }
    }
}

void SuperSimpleSamplerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    updateADSR();

    sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // Apply gain
    float gain = gainParam->load();
    buffer.applyGain(gain);
}

bool SuperSimpleSamplerProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SuperSimpleSamplerProcessor::createEditor()
{
    return new SuperSimpleSamplerEditor(*this);
}

void SuperSimpleSamplerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SuperSimpleSamplerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

void SuperSimpleSamplerProcessor::loadSample(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader != nullptr)
    {
        // Store waveform for display
        waveform.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&waveform, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        // Create the sampler sound
        juce::BigInteger allNotes;
        allNotes.setRange(0, 128, true);

        sampler.clearSounds();

        // Re-read for the sampler sound
        reader.reset(formatManager.createReaderFor(file));
        if (reader != nullptr)
        {
            sampler.addSound(new juce::SamplerSound(
                file.getFileNameWithoutExtension(),
                *reader,
                allNotes,
                rootNote,    // root note
                0.01,        // attack time (will be overridden by ADSR)
                0.1,         // release time (will be overridden by ADSR)
                20.0         // max sample length in seconds
            ));

            sampleLoaded = true;
            updateADSR();
        }
    }
}

void SuperSimpleSamplerProcessor::loadSampleFromData(const void* data, size_t size, const juce::String& name)
{
    auto* inputStream = new juce::MemoryInputStream(data, size, false);
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::unique_ptr<juce::InputStream>(inputStream)));

    if (reader != nullptr)
    {
        waveform.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&waveform, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        juce::BigInteger allNotes;
        allNotes.setRange(0, 128, true);

        sampler.clearSounds();

        // Re-create reader from data
        auto* inputStream2 = new juce::MemoryInputStream(data, size, false);
        reader.reset(formatManager.createReaderFor(std::unique_ptr<juce::InputStream>(inputStream2)));

        if (reader != nullptr)
        {
            sampler.addSound(new juce::SamplerSound(
                name,
                *reader,
                allNotes,
                rootNote,
                0.01,
                0.1,
                20.0
            ));

            sampleLoaded = true;
            updateADSR();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperSimpleSamplerProcessor();
}
