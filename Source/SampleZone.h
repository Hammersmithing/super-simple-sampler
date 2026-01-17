#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

struct SampleZone
{
    juce::String name;
    juce::AudioBuffer<float> audioData;
    double sampleRate = 44100.0;

    int rootNote = 60;      // MIDI note where sample plays at original pitch
    int lowNote = 0;        // Lowest MIDI note that triggers this sample
    int highNote = 127;     // Highest MIDI note that triggers this sample
    int lowVelocity = 1;    // Lowest velocity that triggers this sample
    int highVelocity = 127; // Highest velocity that triggers this sample

    bool isValid() const { return audioData.getNumSamples() > 0; }

    bool containsNote(int midiNote) const
    {
        return midiNote >= lowNote && midiNote <= highNote;
    }

    bool containsVelocity(int velocity) const
    {
        return velocity >= lowVelocity && velocity <= highVelocity;
    }

    bool matches(int midiNote, int velocity) const
    {
        return containsNote(midiNote) && containsVelocity(velocity);
    }
};

class SampleZoneSound : public juce::SynthesiserSound
{
public:
    SampleZoneSound(const SampleZone& zone)
        : sampleZone(zone)
    {
    }

    bool appliesToNote(int midiNote) override
    {
        return sampleZone.containsNote(midiNote);
    }

    bool appliesToChannel(int) override { return true; }

    const SampleZone& getZone() const { return sampleZone; }

private:
    SampleZone sampleZone;
};

class SampleZoneVoice : public juce::SynthesiserVoice
{
public:
    SampleZoneVoice() = default;

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SampleZoneSound*>(sound) != nullptr;
    }

    // Check if this voice is currently playing audio
    bool isPlaying() const
    {
        return playing;
    }

    // Get the note this voice is playing (-1 if not playing)
    int getPlayingNote() const
    {
        return playingNote;
    }

    // Check if this voice is being sustained by the pedal
    bool isSustainedByPedal() const
    {
        return sustainedByPedal;
    }

    void setSustainPedal(bool isDown)
    {
        if (!isDown && sustainedByPedal)
        {
            // Pedal released - stop the sustained note
            sustainedByPedal = false;
            adsr.noteOff();
        }
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override
    {
        juce::ignoreUnused(currentPitchWheelPosition);

        if (auto* zoneSound = dynamic_cast<SampleZoneSound*>(sound))
        {
            const auto& zone = zoneSound->getZone();

            // Zone selection (including velocity matching) is done before startNote is called
            currentZone = &zone;
            samplePosition = 0.0;
            playing = true;
            playingNote = midiNoteNumber;
            sustainedByPedal = false;

            // Calculate pitch ratio based on root note
            double frequencyOfNote = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
            double frequencyOfRoot = juce::MidiMessage::getMidiNoteInHertz(zone.rootNote);
            pitchRatio = frequencyOfNote / frequencyOfRoot;

            // Adjust for sample rate difference
            pitchRatio *= zone.sampleRate / getSampleRate();

            level = velocity;

            adsr.noteOn();
        }
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            clearCurrentNote();
            adsr.reset();
            playing = false;
            playingNote = -1;
            sustainedByPedal = false;
        }
    }

    // Called when key is released but sustain pedal may be down
    void noteReleasedWithPedal(bool pedalDown)
    {
        if (pedalDown)
        {
            // Sustain pedal is down - keep the note ringing
            sustainedByPedal = true;
        }
        else
        {
            // No pedal - release normally
            adsr.noteOff();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void setADSRParameters(const juce::ADSR::Parameters& params)
    {
        adsr.setParameters(params);
    }

    void prepareToPlay(double sampleRate, int)
    {
        adsr.setSampleRate(sampleRate);
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (!playing || currentZone == nullptr || !currentZone->isValid())
            return;

        const auto& audioData = currentZone->audioData;
        const int totalSamples = audioData.getNumSamples();
        const int numChannels = std::min(audioData.getNumChannels(), outputBuffer.getNumChannels());

        for (int sample = 0; sample < numSamples; ++sample)
        {
            if (samplePosition >= totalSamples)
            {
                clearCurrentNote();
                adsr.reset();
                playing = false;
                playingNote = -1;
                sustainedByPedal = false;
                break;
            }

            float envelopeValue = adsr.getNextSample();

            if (!adsr.isActive())
            {
                clearCurrentNote();
                playing = false;
                playingNote = -1;
                sustainedByPedal = false;
                break;
            }

            // Linear interpolation for smooth playback
            int pos0 = static_cast<int>(samplePosition);
            int pos1 = pos0 + 1;
            float frac = static_cast<float>(samplePosition - pos0);

            if (pos1 >= totalSamples)
                pos1 = pos0;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = audioData.getReadPointer(ch);
                float sample0 = data[pos0];
                float sample1 = data[pos1];
                float interpolated = sample0 + frac * (sample1 - sample0);

                outputBuffer.addSample(ch, startSample + sample, interpolated * level * envelopeValue);
            }

            // For mono samples, copy to both channels
            if (numChannels == 1 && outputBuffer.getNumChannels() > 1)
            {
                const float* data = audioData.getReadPointer(0);
                float sample0 = data[pos0];
                float sample1 = data[pos1];
                float interpolated = sample0 + frac * (sample1 - sample0);

                outputBuffer.addSample(1, startSample + sample, interpolated * level * envelopeValue);
            }

            samplePosition += pitchRatio;
        }
    }

private:
    const SampleZone* currentZone = nullptr;
    double samplePosition = 0.0;
    double pitchRatio = 1.0;
    float level = 0.0f;
    juce::ADSR adsr;

    bool playing = false;
    int playingNote = -1;
    bool sustainedByPedal = false;
};
