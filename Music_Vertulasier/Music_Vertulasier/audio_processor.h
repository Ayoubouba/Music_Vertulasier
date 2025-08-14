#pragma once

class AudioProcessor {
public:
    AudioProcessor(int bufferSize);
    ~AudioProcessor();

    bool initialize();
    void process(float* input, float* output, int numSamples);

private:
    int bufferSize_;
    float* d_input_ = nullptr;  // Device pointers
    float* d_output_ = nullptr;
};