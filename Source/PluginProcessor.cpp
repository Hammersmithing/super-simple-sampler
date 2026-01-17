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

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("polyphony", 1),
        "Polyphony",
        1, 64, 16
    ));

    return { params.begin(), params.end() };
}

SuperSimpleSamplerProcessor::SuperSimpleSamplerProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("SuperSimpleSampler"), createParameterLayout())
{
    // Ensure instruments folder exists
    InstrumentLoader::ensureInstrumentsFolderExists();

    // Add max voices to the sampler (actual polyphony controlled by parameter)
    for (int i = 0; i < 64; ++i)
        sampler.addVoice(new SampleZoneVoice());

    attackParam = parameters.getRawParameterValue("attack");
    decayParam = parameters.getRawParameterValue("decay");
    sustainParam = parameters.getRawParameterValue("sustain");
    releaseParam = parameters.getRawParameterValue("release");
    gainParam = parameters.getRawParameterValue("gain");
    polyphonyParam = parameters.getRawParameterValue("polyphony");

    // Initial scan for instruments
    refreshInstrumentList();
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

    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SampleZoneVoice*>(sampler.getVoice(i)))
        {
            voice->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }

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

    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SampleZoneVoice*>(sampler.getVoice(i)))
        {
            voice->setADSRParameters(adsrParams);
        }
    }
}

void SuperSimpleSamplerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    updateADSR();

    // Process MIDI with custom handling for proper velocity/round-robin selection
    juce::MidiBuffer processedMidi;

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            handleNoteOn(message.getChannel(), message.getNoteNumber(),
                        message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            handleNoteOff(message.getChannel(), message.getNoteNumber(),
                         message.getFloatVelocity());
        }
        else if (message.isController() && message.getControllerNumber() == 64)
        {
            // Sustain pedal (CC 64)
            handleSustainPedal(message.getControllerValue() >= 64);
        }
        else
        {
            // Pass through other MIDI messages (pitch bend, CC, etc.)
            processedMidi.addEvent(message, metadata.samplePosition);
        }
    }

    // Render audio (note on/off already handled above)
    sampler.renderNextBlock(buffer, processedMidi, 0, buffer.getNumSamples());

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

    // Store current instrument path
    if (currentInstrument.isValid())
    {
        state.setProperty("instrumentPath",
                          currentInstrument.info.definitionFile.getFullPathName(),
                          nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SuperSimpleSamplerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(state);

            // Restore instrument
            auto instrumentPath = state.getProperty("instrumentPath").toString();
            if (instrumentPath.isNotEmpty())
            {
                juce::File instrumentFile(instrumentPath);
                if (instrumentFile.existsAsFile())
                {
                    loadInstrumentFromFile(instrumentFile);
                }
            }
        }
    }
}

std::vector<InstrumentInfo> SuperSimpleSamplerProcessor::getAvailableInstruments()
{
    return availableInstruments;
}

void SuperSimpleSamplerProcessor::refreshInstrumentList()
{
    availableInstruments = instrumentLoader.scanForInstruments();
}

void SuperSimpleSamplerProcessor::loadInstrument(int index)
{
    if (index >= 0 && index < static_cast<int>(availableInstruments.size()))
    {
        loadInstrumentFromFile(availableInstruments[static_cast<size_t>(index)].definitionFile);
    }
}

void SuperSimpleSamplerProcessor::loadInstrumentFromFile(const juce::File& definitionFile)
{
    currentInstrument = instrumentLoader.loadInstrument(definitionFile);

    // Reset round-robin counters for new instrument
    roundRobinCounters.clear();

    if (currentInstrument.isValid())
    {
        selectedZoneIndex = 0;
        rebuildSampler();
    }
    else
    {
        selectedZoneIndex = -1;
        sampler.clearSounds();
    }

    notifyListeners();
}

void SuperSimpleSamplerProcessor::unloadInstrument()
{
    currentInstrument = LoadedInstrument();
    selectedZoneIndex = -1;
    sampler.clearSounds();
    notifyListeners();
}

const SampleZone* SuperSimpleSamplerProcessor::getZone(int index) const
{
    if (index >= 0 && index < static_cast<int>(currentInstrument.zones.size()))
        return &currentInstrument.zones[static_cast<size_t>(index)];
    return nullptr;
}

const SampleZone* SuperSimpleSamplerProcessor::getSelectedZone() const
{
    return getZone(selectedZoneIndex);
}

void SuperSimpleSamplerProcessor::rebuildSampler()
{
    sampler.clearSounds();

    for (const auto& zone : currentInstrument.zones)
    {
        if (zone.isValid())
        {
            sampler.addSound(new SampleZoneSound(zone));
        }
    }
}

void SuperSimpleSamplerProcessor::notifyListeners()
{
    listeners.call([](Listener& l) { l.instrumentChanged(); });
}

std::vector<int> SuperSimpleSamplerProcessor::findMatchingZones(int midiNote, int velocity)
{
    std::vector<int> matches;

    for (int i = 0; i < sampler.getNumSounds(); ++i)
    {
        if (auto* sound = dynamic_cast<SampleZoneSound*>(sampler.getSound(i).get()))
        {
            const auto& zone = sound->getZone();
            if (zone.matches(midiNote, velocity))
            {
                matches.push_back(i);
            }
        }
    }

    return matches;
}

void SuperSimpleSamplerProcessor::handleNoteOn(int midiChannel, int midiNote, float velocity)
{
    juce::ignoreUnused(midiChannel);

    int intVelocity = static_cast<int>(velocity * 127.0f);

    // Find all zones that match this note AND velocity
    auto matchingZones = findMatchingZones(midiNote, intVelocity);

    if (matchingZones.empty())
        return;

    // True round-robin: cycle through matching zones sequentially
    int rrKey = (midiNote << 8) | intVelocity;
    int& rrIndex = roundRobinCounters[rrKey];

    // Get current RR sample and advance counter
    int selectedIndex = matchingZones[static_cast<size_t>(rrIndex % matchingZones.size())];
    rrIndex = (rrIndex + 1) % static_cast<int>(matchingZones.size());

    auto* selectedSound = sampler.getSound(selectedIndex).get();

    // Get current polyphony setting
    int maxVoices = static_cast<int>(polyphonyParam->load());

    // Find a free voice within the polyphony limit
    for (int i = 0; i < maxVoices; ++i)
    {
        if (auto* voice = dynamic_cast<SampleZoneVoice*>(sampler.getVoice(i)))
        {
            if (!voice->isPlaying())
            {
                voice->startNote(midiNote, velocity, selectedSound, 0);
                return;
            }
        }
    }

    // If no free voice, steal the first one (voice stealing)
    if (auto* voice = dynamic_cast<SampleZoneVoice*>(sampler.getVoice(0)))
    {
        voice->stopNote(0.0f, false);
        voice->startNote(midiNote, velocity, selectedSound, 0);
    }
}

void SuperSimpleSamplerProcessor::handleNoteOff(int midiChannel, int midiNote, float velocity)
{
    juce::ignoreUnused(midiChannel, velocity);

    // Release all voices playing this note (respecting sustain pedal)
    for (int i = 0; i < sampler.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<SampleZoneVoice*>(sampler.getVoice(i)))
        {
            if (voice->isPlaying() && voice->getPlayingNote() == midiNote)
            {
                voice->noteReleasedWithPedal(sustainPedalDown);
            }
        }
    }
}

void SuperSimpleSamplerProcessor::handleSustainPedal(bool isDown)
{
    sustainPedalDown = isDown;

    if (!isDown)
    {
        // Pedal released - release all sustained notes
        for (int i = 0; i < sampler.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<SampleZoneVoice*>(sampler.getVoice(i)))
            {
                voice->setSustainPedal(false);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperSimpleSamplerProcessor();
}
