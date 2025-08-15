#pragma once
#include "config.h"
#include <vector>
#include <mutex>
#include <string>
#include <atomic>
#include <SDL2/SDL_stdinc.h>
#include <cstddef>

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    bool initialize();
    bool start();
    void shutdown();
    bool getAudioFrame(std::vector<float>& buffer);

    const std::string& getLastError() const { return lastError; }
    size_t getSampleRate() const { return SAMPLE_RATE; }
    size_t getBufferSize() const { return AUDIO_BUFFER_SIZE; }

private:
    struct AudioDevice;
    std::unique_ptr<AudioDevice> device;
    std::string lastError;
    std::atomic<bool> isRunning{ false };

    void processAudioData(const float* data, size_t sampleCount);
    static void audioCallback(void* userdata, Uint8* stream, int len);
};