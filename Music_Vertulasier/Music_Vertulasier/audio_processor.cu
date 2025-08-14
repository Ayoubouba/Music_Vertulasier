#include "audio_processor.h"
#include <cuda_runtime.h>
#include <iostream>

// 1. First add this kernel definition
__global__ void copyKernel(float* in, float* out, int N) {
    int i = threadIdx.x + blockIdx.x * blockDim.x;
    if (i < N) out[i] = in[i];
}

AudioProcessor::AudioProcessor(int bufferSize) : bufferSize_(bufferSize) {}

bool AudioProcessor::initialize() {
    cudaMalloc(&d_input_, bufferSize_ * sizeof(float));
    cudaMalloc(&d_output_, bufferSize_ * sizeof(float));
    return true;
}

void AudioProcessor::process(float* input, float* output, int N) {
    // 2. Add these three lines for the kernel launch
    cudaMemcpy(d_input_, input, N * sizeof(float), cudaMemcpyHostToDevice);
    copyKernel << <(N + 255) / 256, 256 >> > (d_input_, d_output_, N);
    cudaMemcpy(output, d_output_, N * sizeof(float), cudaMemcpyDeviceToHost);
}

AudioProcessor::~AudioProcessor() {
    cudaFree(d_input_);
    cudaFree(d_output_);
}