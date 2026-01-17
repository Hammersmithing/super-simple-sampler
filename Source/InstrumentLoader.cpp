#include "InstrumentLoader.h"

InstrumentLoader::InstrumentLoader()
{
    formatManager.registerBasicFormats();
}

juce::File InstrumentLoader::getInstrumentsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Super Simple Sampler")
        .getChildFile("Instruments");
}

void InstrumentLoader::ensureInstrumentsFolderExists()
{
    auto folder = getInstrumentsFolder();
    if (!folder.exists())
    {
        folder.createDirectory();
    }
}

std::vector<InstrumentInfo> InstrumentLoader::scanForInstruments()
{
    std::vector<InstrumentInfo> instruments;

    auto instrumentsFolder = getInstrumentsFolder();
    if (!instrumentsFolder.exists())
        return instruments;

    // Scan for folders containing instrument.sss
    for (const auto& entry : juce::RangedDirectoryIterator(instrumentsFolder, false, "*", juce::File::findDirectories))
    {
        auto folder = entry.getFile();
        auto definitionFile = folder.getChildFile("instrument.sss");

        if (definitionFile.existsAsFile())
        {
            InstrumentInfo info;
            info.folder = folder;
            info.definitionFile = definitionFile;
            info.name = folder.getFileNameWithoutExtension(); // Default to folder name

            // Try to parse XML to get actual name and author
            if (auto xml = juce::XmlDocument::parse(definitionFile))
            {
                if (xml->hasTagName("SuperSimpleSampler"))
                {
                    if (auto* meta = xml->getChildByName("meta"))
                    {
                        if (auto* nameElem = meta->getChildByName("name"))
                            info.name = nameElem->getAllSubText().trim();
                        if (auto* authorElem = meta->getChildByName("author"))
                            info.author = authorElem->getAllSubText().trim();
                    }
                }
            }

            instruments.push_back(info);
        }
    }

    // Sort by name
    std::sort(instruments.begin(), instruments.end(),
              [](const InstrumentInfo& a, const InstrumentInfo& b) {
                  return a.name.compareIgnoreCase(b.name) < 0;
              });

    return instruments;
}

LoadedInstrument InstrumentLoader::loadInstrument(const juce::File& definitionFile)
{
    LoadedInstrument result;
    result.info.definitionFile = definitionFile;
    result.info.folder = definitionFile.getParentDirectory();

    parseInstrumentXML(definitionFile, result.info, result.zones);

    return result;
}

LoadedInstrument InstrumentLoader::loadInstrumentFromFolder(const juce::File& folder)
{
    auto definitionFile = folder.getChildFile("instrument.sss");
    return loadInstrument(definitionFile);
}

bool InstrumentLoader::parseInstrumentXML(const juce::File& xmlFile, InstrumentInfo& info,
                                           std::vector<SampleZone>& zones)
{
    auto xml = juce::XmlDocument::parse(xmlFile);
    if (xml == nullptr)
        return false;

    if (!xml->hasTagName("SuperSimpleSampler"))
        return false;

    // Parse meta
    if (auto* meta = xml->getChildByName("meta"))
    {
        if (auto* nameElem = meta->getChildByName("name"))
            info.name = nameElem->getAllSubText().trim();
        if (auto* authorElem = meta->getChildByName("author"))
            info.author = authorElem->getAllSubText().trim();
    }

    // Parse samples
    if (auto* samples = xml->getChildByName("samples"))
    {
        for (auto* sampleElem : samples->getChildIterator())
        {
            if (sampleElem->hasTagName("sample"))
            {
                SampleZone zone;

                // Get file path (relative to instrument folder)
                auto filePath = sampleElem->getStringAttribute("file");
                auto sampleFile = info.folder.getChildFile(filePath);

                if (!loadSampleFile(sampleFile, zone))
                    continue;

                zone.name = sampleFile.getFileNameWithoutExtension();

                // Parse mapping attributes with defaults
                zone.rootNote = sampleElem->getIntAttribute("rootNote", 60);
                zone.lowNote = sampleElem->getIntAttribute("loNote", 0);
                zone.highNote = sampleElem->getIntAttribute("hiNote", 127);
                zone.lowVelocity = sampleElem->getIntAttribute("loVel", 1);
                zone.highVelocity = sampleElem->getIntAttribute("hiVel", 127);

                zones.push_back(std::move(zone));
            }
        }
    }

    return !zones.empty();
}

bool InstrumentLoader::loadSampleFile(const juce::File& sampleFile, SampleZone& zone)
{
    if (!sampleFile.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(sampleFile));
    if (reader == nullptr)
        return false;

    zone.sampleRate = reader->sampleRate;
    zone.audioData.setSize(static_cast<int>(reader->numChannels),
                           static_cast<int>(reader->lengthInSamples));
    reader->read(&zone.audioData, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    return zone.audioData.getNumSamples() > 0;
}
