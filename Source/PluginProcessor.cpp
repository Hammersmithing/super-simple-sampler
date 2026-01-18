#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <fstream>

// Debug logging to file
static void debugLog(const juce::String& msg)
{
    auto logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("sampler_debug.txt");
    logFile.appendText(msg + "\n");
}

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

    // Initialize streaming components
    streamingFormatManager.registerBasicFormats();
    diskStreamer = std::make_unique<DiskStreamer>();
    diskStreamer->setAudioFormatManager(&streamingFormatManager);

    // Register streaming voices with the disk streamer
    for (int i = 0; i < StreamingConstants::maxStreamingVoices; ++i)
    {
        diskStreamer->registerVoice(i, &streamingVoices[static_cast<size_t>(i)]);
    }

    // Initial scan for instruments
    refreshInstrumentList();
}

SuperSimpleSamplerProcessor::~SuperSimpleSamplerProcessor()
{
    // Stop the disk streaming thread before destruction
    if (diskStreamer != nullptr)
    {
        diskStreamer->stopThread();
    }
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

    // Prepare streaming voices
    for (auto& voice : streamingVoices)
    {
        voice.prepareToPlay(sampleRate, samplesPerBlock);
    }

    // Start the disk streaming thread if streaming is enabled
    if (streamingEnabled && diskStreamer != nullptr)
    {
        diskStreamer->startThread();
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

    // Route to appropriate processing method based on streaming mode
    if (streamingEnabled)
    {
        processBlockStreaming(buffer, midiMessages);
    }
    else
    {
        // Original RAM-based processing
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
    }

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

    debugLog("=== Sampler rebuilt: " + juce::String(sampler.getNumSounds()) + " sounds loaded ===");
    for (int i = 0; i < sampler.getNumSounds(); ++i)
    {
        if (auto* sound = dynamic_cast<SampleZoneSound*>(sampler.getSound(i).get()))
        {
            const auto& z = sound->getZone();
            debugLog("  [" + juce::String(i) + "] " + z.name
                     + " note:" + juce::String(z.lowNote) + "-" + juce::String(z.highNote)
                     + " vel:" + juce::String(z.lowVelocity) + "-" + juce::String(z.highVelocity));
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

    // Per-note round-robin (like SFZ seq_position)
    int& rrCounter = roundRobinCounters[midiNote];
    int numMatches = static_cast<int>(matchingZones.size());
    int rrIndex = rrCounter % numMatches;
    int selectedIndex = matchingZones[static_cast<size_t>(rrIndex)];

    debugLog("Note " + juce::String(midiNote) + " vel " + juce::String(intVelocity)
             + " | matches=" + juce::String(numMatches)
             + " | rrCounter=" + juce::String(rrCounter)
             + " | rrIndex=" + juce::String(rrIndex)
             + " | selectedIdx=" + juce::String(selectedIndex));

    rrCounter++;

    auto* selectedSound = sampler.getSound(selectedIndex).get();

    // Store last played sample name for debug display
    if (auto* zoneSound = dynamic_cast<SampleZoneSound*>(selectedSound))
    {
        lastPlayedSample = zoneSound->getZone().name + " (RR" + juce::String(rrIndex + 1) + "/" + juce::String(numMatches) + ")";
        debugLog("  -> Playing: " + lastPlayedSample);
    }

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

// ==================== Streaming Mode Implementation ====================

void SuperSimpleSamplerProcessor::setStreamingEnabled(bool enabled)
{
    if (streamingEnabled == enabled)
        return;

    streamingEnabled = enabled;

    if (enabled)
    {
        // Start the disk streaming thread
        if (diskStreamer != nullptr)
        {
            diskStreamer->startThread();
        }
    }
    else
    {
        // Stop the disk streaming thread and reset all streaming voices
        if (diskStreamer != nullptr)
        {
            diskStreamer->stopThread();
        }

        for (auto& voice : streamingVoices)
        {
            voice.reset();
        }
    }
}

void SuperSimpleSamplerProcessor::loadInstrumentStreaming(const juce::File& definitionFile)
{
    // Clear existing preloaded samples
    preloadedSamples.clear();

    // Reset round-robin counters
    roundRobinCounters.clear();

    auto xml = juce::XmlDocument::parse(definitionFile);
    if (xml == nullptr || !xml->hasTagName("SuperSimpleSampler"))
    {
        notifyListeners();
        return;
    }

    // Store instrument info
    currentInstrument.info.definitionFile = definitionFile;
    currentInstrument.info.folder = definitionFile.getParentDirectory();

    // Parse meta
    if (auto* meta = xml->getChildByName("meta"))
    {
        if (auto* nameElem = meta->getChildByName("name"))
            currentInstrument.info.name = nameElem->getAllSubText().trim();
        if (auto* authorElem = meta->getChildByName("author"))
            currentInstrument.info.author = authorElem->getAllSubText().trim();
    }

    // Parse samples - load as preloaded samples (partial load)
    if (auto* samples = xml->getChildByName("samples"))
    {
        for (auto* sampleElem : samples->getChildIterator())
        {
            if (sampleElem->hasTagName("sample"))
            {
                PreloadedSample sample;

                // Get file path
                auto filePath = sampleElem->getStringAttribute("file");
                auto sampleFile = currentInstrument.info.folder.getChildFile(filePath);

                if (!loadPreloadedSample(sampleFile, sample))
                    continue;

                // Parse mapping attributes
                sample.rootNote = sampleElem->getIntAttribute("rootNote", 60);
                sample.lowNote = sampleElem->getIntAttribute("loNote", 0);
                sample.highNote = sampleElem->getIntAttribute("hiNote", 127);
                sample.lowVelocity = sampleElem->getIntAttribute("loVel", 1);
                sample.highVelocity = sampleElem->getIntAttribute("hiVel", 127);

                preloadedSamples.push_back(std::move(sample));
            }
        }
    }

    if (!preloadedSamples.empty())
    {
        selectedZoneIndex = 0;
    }
    else
    {
        selectedZoneIndex = -1;
    }

    debugLog("=== Streaming mode: " + juce::String(preloadedSamples.size()) + " preloaded samples ===");
    for (size_t i = 0; i < preloadedSamples.size(); ++i)
    {
        const auto& s = preloadedSamples[i];
        debugLog("  [" + juce::String(i) + "] " + s.name
                 + " total:" + juce::String(s.totalSampleFrames) + " frames"
                 + " preload:" + juce::String(s.preloadSizeFrames) + " frames"
                 + " streaming:" + (s.needsStreaming() ? "YES" : "no"));
    }

    notifyListeners();
}

bool SuperSimpleSamplerProcessor::loadPreloadedSample(const juce::File& sampleFile, PreloadedSample& sample)
{
    if (!sampleFile.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(
        streamingFormatManager.createReaderFor(sampleFile));

    if (reader == nullptr)
        return false;

    // Store file metadata
    sample.filePath = sampleFile.getFullPathName();
    sample.sampleRate = reader->sampleRate;
    sample.numChannels = static_cast<int>(reader->numChannels);
    sample.totalSampleFrames = static_cast<int64_t>(reader->lengthInSamples);
    sample.name = sampleFile.getFileNameWithoutExtension();

    // Calculate preload size in frames
    // 64KB / (channels * bytes per sample)
    int bytesPerSample = 4;  // 32-bit float
    sample.preloadSizeFrames = PreloadedSample::preloadSizeBytes /
                               (sample.numChannels * bytesPerSample);

    // Cap preload to total sample length
    int framesToPreload = std::min(sample.preloadSizeFrames,
                                    static_cast<int>(sample.totalSampleFrames));

    // Load the preload buffer
    sample.preloadBuffer.setSize(sample.numChannels, framesToPreload);
    reader->read(&sample.preloadBuffer, 0, framesToPreload, 0, true, true);

    return sample.preloadBuffer.getNumSamples() > 0;
}

const PreloadedSample* SuperSimpleSamplerProcessor::getPreloadedSample(int index) const
{
    if (index >= 0 && index < static_cast<int>(preloadedSamples.size()))
        return &preloadedSamples[static_cast<size_t>(index)];
    return nullptr;
}

std::vector<int> SuperSimpleSamplerProcessor::findMatchingPreloadedSamples(int midiNote, int velocity)
{
    std::vector<int> matches;

    for (int i = 0; i < static_cast<int>(preloadedSamples.size()); ++i)
    {
        if (preloadedSamples[static_cast<size_t>(i)].matches(midiNote, velocity))
        {
            matches.push_back(i);
        }
    }

    return matches;
}

void SuperSimpleSamplerProcessor::processBlockStreaming(juce::AudioBuffer<float>& buffer,
                                                         juce::MidiBuffer& midiMessages)
{
    // Update ADSR for all streaming voices
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = attackParam->load();
    adsrParams.decay = decayParam->load();
    adsrParams.sustain = sustainParam->load();
    adsrParams.release = releaseParam->load();

    for (auto& voice : streamingVoices)
    {
        voice.setADSRParameters(adsrParams);
    }

    // Process MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            handleNoteOnStreaming(message.getChannel(), message.getNoteNumber(),
                                   message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            handleNoteOffStreaming(message.getChannel(), message.getNoteNumber(),
                                    message.getFloatVelocity());
        }
        else if (message.isController() && message.getControllerNumber() == 64)
        {
            // Sustain pedal
            bool isDown = message.getControllerValue() >= 64;
            sustainPedalDown = isDown;

            if (!isDown)
            {
                for (auto& voice : streamingVoices)
                {
                    voice.setSustainPedal(false);
                }
            }
        }
    }

    // Render all active streaming voices
    const int numSamples = buffer.getNumSamples();

    for (auto& voice : streamingVoices)
    {
        if (voice.isActive())
        {
            voice.renderNextBlock(buffer, 0, numSamples);
        }
    }
}

void SuperSimpleSamplerProcessor::handleNoteOnStreaming(int midiChannel, int midiNote, float velocity)
{
    juce::ignoreUnused(midiChannel);

    int intVelocity = static_cast<int>(velocity * 127.0f);

    // Find matching preloaded samples
    auto matchingSamples = findMatchingPreloadedSamples(midiNote, intVelocity);

    if (matchingSamples.empty())
        return;

    // Per-note round-robin
    int& rrCounter = roundRobinCounters[midiNote];
    int numMatches = static_cast<int>(matchingSamples.size());
    int rrIndex = rrCounter % numMatches;
    int selectedIndex = matchingSamples[static_cast<size_t>(rrIndex)];

    rrCounter++;

    const PreloadedSample* selectedSample = &preloadedSamples[static_cast<size_t>(selectedIndex)];

    // Store last played sample name for debug
    lastPlayedSample = selectedSample->name + " (RR" + juce::String(rrIndex + 1) + "/" + juce::String(numMatches) + ")";
    debugLog("Streaming note " + juce::String(midiNote) + " -> " + lastPlayedSample);

    // Get current polyphony setting
    int maxVoices = static_cast<int>(polyphonyParam->load());
    maxVoices = std::min(maxVoices, StreamingConstants::maxStreamingVoices);

    // Find a free streaming voice
    for (int i = 0; i < maxVoices; ++i)
    {
        if (!streamingVoices[static_cast<size_t>(i)].isActive())
        {
            streamingVoices[static_cast<size_t>(i)].startVoice(
                selectedSample, midiNote, velocity, getSampleRate());
            return;
        }
    }

    // No free voice - steal the first one
    streamingVoices[0].stopVoice(false);
    streamingVoices[0].startVoice(selectedSample, midiNote, velocity, getSampleRate());
}

void SuperSimpleSamplerProcessor::handleNoteOffStreaming(int midiChannel, int midiNote, float velocity)
{
    juce::ignoreUnused(midiChannel, velocity);

    // Release all streaming voices playing this note
    for (auto& voice : streamingVoices)
    {
        if (voice.isActive() && voice.getPlayingNote() == midiNote)
        {
            voice.noteReleasedWithPedal(sustainPedalDown);
        }
    }
}
