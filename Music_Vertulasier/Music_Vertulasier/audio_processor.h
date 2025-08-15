#pragma once

class AudioProcessor {
public:
    AudioProcessor(int bufferSize);
    ~AudioProcessor();

    bool initialize();
    void process(float* input, float* output, int numSamples);
    // New: simple parameter(s)
    void setGain(float g) { gain_ = g; }

private:
    int bufferSize_;
	float gain_ = 1.0f;  // Default gain value
    float* d_input_ = nullptr;  // Device pointers
    float* d_output_ = nullptr;
};