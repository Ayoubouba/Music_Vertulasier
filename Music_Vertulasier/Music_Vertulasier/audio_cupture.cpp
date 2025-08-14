#include "audio_cupture.h"
#include "config.h"
#include <memory>
#include <thread>
#include <vector>

#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm>

struct AudioCapture::AudioDevice {
    SDL_AudioDeviceID id = 0;
    std::vector<float> circularBuffer;
    std::mutex bufferMutex;
    size_t writePos = 0;
    size_t readPos = 0;
    bool initialized = false;

    AudioDevice() {
        circularBuffer.resize(AUDIO_BUFFER_SIZE * 4); // 4x buffer for safety
    }
};

AudioCapture::AudioCapture() : device(std::make_unique<AudioDevice>()) {}

AudioCapture::~AudioCapture() {
    shutdown();
}

bool AudioCapture::initialize() {
    if (device->initialized) {
        return true;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        lastError = "SDL audio init failed: " + std::string(SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{}, obtained{};
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_F32;
    desired.channels = 1;
    desired.samples = AUDIO_BUFFER_SIZE;
    desired.callback = &AudioCapture::audioCallback;
    desired.userdata = this;

    device->id = SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (device->id == 0) {
        lastError = "Failed to open audio device: " + std::string(SDL_GetError());
        return false;
    }

    if (obtained.format != AUDIO_F32) {
        lastError = "Couldn't get Float32 audio format (got " + std::to_string(obtained.format) + ")";
        SDL_CloseAudioDevice(device->id);
        device->id = 0;
        return false;
    }

    if (obtained.freq != SAMPLE_RATE) {
#if DEBUG_PRINT_AUDIO
        std::cerr << "Warning: Using sample rate " << obtained.freq
            << " instead of requested " << SAMPLE_RATE << std::endl;
#endif
    }

    device->initialized = true;
    return true;
}

void AudioCapture::audioCallback(void* userdata, Uint8* stream, int len) {
    AudioCapture* capture = static_cast<AudioCapture*>(userdata);
    if (capture->isRunning) {
        capture->processAudioData(reinterpret_cast<float*>(stream), len / sizeof(float));
    }
}

void AudioCapture::processAudioData(const float* data, size_t sampleCount) {
    std::lock_guard<std::mutex> lock(device->bufferMutex);

    for (size_t i = 0; i < sampleCount; i++) {
        device->circularBuffer[device->writePos] = data[i];
        device->writePos = (device->writePos + 1) % device->circularBuffer.size();

        // Handle buffer wrap-around
        if (device->writePos == device->readPos) {
            device->readPos = (device->readPos + 1) % device->circularBuffer.size();
#if DEBUG_PRINT_AUDIO
            std::cerr << "Audio buffer overflow! Some samples were dropped." << std::endl;
#endif
        }
    }
}

bool AudioCapture::getAudioFrame(std::vector<float>& buffer) {
    if (!device->initialized || !isRunning) {
        return false;
    }

    std::lock_guard<std::mutex> lock(device->bufferMutex);

    size_t available;
    if (device->writePos >= device->readPos) {
        available = device->writePos - device->readPos;
    }
    else {
        available = device->circularBuffer.size() - device->readPos + device->writePos;
    }

    if (available < FFT_SIZE) {
        return false;
    }

    // Return exactly FFT_SIZE samples
    buffer.resize(FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE; i++) {
        buffer[i] = device->circularBuffer[device->readPos];
        device->readPos = (device->readPos + 1) % device->circularBuffer.size();
    }

#if DEBUG_PRINT_AUDIO
    static int printCounter = 0;
    if (++printCounter % 10 == 0) {
        float sum = 0;
        for (float sample : buffer) sum += std::abs(sample);
        std::cout << "Audio frame avg: " << (sum / FFT_SIZE)
            << " (buffer usage: " << (available * 100 / device->circularBuffer.size()) << "%)" << std::endl;
    }
#endif

    return true;
}

bool AudioCapture::start() {
    if (!device->initialized) {
        lastError = "Audio not initialized";
        return false;
    }
    isRunning = true;
    SDL_PauseAudioDevice(device->id, 0);
    return true;
}

void AudioCapture::shutdown() {
    isRunning = false;
    if (device->id) {
        SDL_PauseAudioDevice(device->id, 1);
        SDL_CloseAudioDevice(device->id);
        device->id = 0;
    }
    device->initialized = false;
}