#include "audio_processor.h"
#include <cuda_runtime.h>
#include <iostream>

// 1) Kernel: abs + gain + clamp to [-1,1]
__global__ void processKernelAbsGainClamp(const float* in, float* out, int N, float gain) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;

    float v = in[i];
    v = fabsf(v) * gain;     // abs + gain
    // clamp
    if (v > 1.0f) v = 1.0f;
    out[i] = v;
}

AudioProcessor::AudioProcessor(int bufferSize) : bufferSize_(bufferSize) {}

bool AudioProcessor::initialize() {
    cudaMalloc(&d_input_, bufferSize_ * sizeof(float));
    cudaMalloc(&d_output_, bufferSize_ * sizeof(float));
    return true;
}

void AudioProcessor::process(float* input, float* output, int N) {
    int threads = 256;
    int blocks = (N + threads - 1) / threads;

    cudaMemcpy(d_input_, input, N * sizeof(float), cudaMemcpyHostToDevice);

    // replace copyKernel with the processing kernel
    processKernelAbsGainClamp << <blocks, threads >> > (d_input_, d_output_, N, gain_);

    // (During bring-up, keep both checks. Later you can remove sync.)
    cudaError_t kerr = cudaGetLastError();
    if (kerr != cudaSuccess) {
        std::cerr << "Kernel launch error: " << cudaGetErrorString(kerr) << "\n";
    }
    cudaDeviceSynchronize();

    cudaMemcpy(output, d_output_, N * sizeof(float), cudaMemcpyDeviceToHost);
}

AudioProcessor::~AudioProcessor() {
    cudaFree(d_input_);
    cudaFree(d_output_);
}