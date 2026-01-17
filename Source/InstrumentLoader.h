#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleZone.h"

struct InstrumentInfo
{
    juce::String name;
    juce::String author;
    juce::File folder;
    juce::File definitionFile;

    bool isValid() const { return definitionFile.existsAsFile(); }
};

struct LoadedInstrument
{
    InstrumentInfo info;
    std::vector<SampleZone> zones;

    bool isValid() const { return !zones.empty(); }
};

class InstrumentLoader
{
public:
    InstrumentLoader();

    // Get the standard instruments folder
    static juce::File getInstrumentsFolder();

    // Create the instruments folder if it doesn't exist
    static void ensureInstrumentsFolderExists();

    // Scan for available instruments
    std::vector<InstrumentInfo> scanForInstruments();

    // Load an instrument from its definition file
    LoadedInstrument loadInstrument(const juce::File& definitionFile);

    // Load an instrument from its folder (looks for instrument.sss)
    LoadedInstrument loadInstrumentFromFolder(const juce::File& folder);

private:
    juce::AudioFormatManager formatManager;

    bool parseInstrumentXML(const juce::File& xmlFile, InstrumentInfo& info, std::vector<SampleZone>& zones);
    bool loadSampleFile(const juce::File& sampleFile, SampleZone& zone);
};
