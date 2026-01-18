#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <atomic>
#include "DiskStreaming.h"
#include "StreamingVoice.h"

/**
 * DiskStreamer is a background thread that handles all disk I/O for streaming voices.
 *
 * Design:
 * - Polls active voices every few milliseconds
 * - When a voice signals needsData, reads from disk and fills the voice's ring buffer
 * - Manages file readers to avoid repeatedly opening/closing files
 * - Completely non-blocking from audio thread perspective
 */
class DiskStreamer : public juce::Thread
{
public:
    DiskStreamer();
    ~DiskStreamer() override;

    /** Start the disk streaming thread */
    void startThread();

    /** Stop the thread and clean up resources */
    void stopThread();

    /** Register a voice for disk streaming (call from main/message thread) */
    void registerVoice(int voiceIndex, StreamingVoice* voice);

    /** Unregister a voice (call from main/message thread) */
    void unregisterVoice(int voiceIndex);

    /** Set the audio format manager for creating file readers */
    void setAudioFormatManager(juce::AudioFormatManager* manager) { formatManager = manager; }

private:
    void run() override;

    /** Fill a single voice's ring buffer from disk */
    void fillVoiceBuffer(int voiceIndex);

    /** Open a reader for the given sample file path */
    std::unique_ptr<juce::AudioFormatReader> openReader(const juce::String& filePath);

    /** Close reader for a voice */
    void closeReader(int voiceIndex);

    // Array of registered voices (atomic for lock-free access)
    std::array<std::atomic<StreamingVoice*>, StreamingConstants::maxStreamingVoices> voices;

    // File readers - one per voice (managed by disk thread only)
    std::array<std::unique_ptr<juce::AudioFormatReader>, StreamingConstants::maxStreamingVoices> readers;

    // Track which sample each reader was opened for
    std::array<juce::String, StreamingConstants::maxStreamingVoices> readerFilePaths;

    // Temporary buffer for disk reads (to batch reads before writing to ring buffer)
    juce::AudioBuffer<float> tempReadBuffer;

    // Audio format manager (owned by processor, we just hold a pointer)
    juce::AudioFormatManager* formatManager = nullptr;
};
