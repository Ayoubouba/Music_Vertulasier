#include <iostream>
#include <memory>
#include <thread>
#include "config.h"
#include "audio_cupture.h"

#include <vector>
#include "audio_processor.h"
#include "cuda_runtime.h"
#include <array>


class MusicVisualizer {
private:
    std::unique_ptr<AudioCapture> audioCapture;
    bool isRunning = false;

public:
    bool initialize();
    void run();
    void shutdown();

private:
    bool update();
    void render();
};

bool MusicVisualizer::initialize() {
    std::cout << "Initializing Music Visualizer...\n";

#if ENABLE_REAL_AUDIO
    audioCapture = std::make_unique<AudioCapture>();
    if (!audioCapture->initialize() || !audioCapture->start()) {
        std::cerr << "Failed to start audio capture.\n";
        return false;
    }
#endif

    std::cout << "Initialization complete!\n";
    return true;
}

void MusicVisualizer::run() {
    std::cout << "Starting main loop...\n";
    isRunning = true;

    for (int frame = 0; frame < 200 && isRunning; frame++) {
        if (!update()) break;
        render();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

bool MusicVisualizer::update() {
#if ENABLE_REAL_AUDIO
    std::vector<float> buffer;
    if (audioCapture->getAudioFrame(buffer)) {
        // Compute average absolute value of the frame
        float sum = 0.0f;
        for (float sample : buffer) {
            sum += std::abs(sample);
        }
        float avg = sum / buffer.size();

        // Map average to a bar length
        int barLength = static_cast<int>(avg * 50); // scale factor
        if (barLength > 50) barLength = 50;        // max width

        // Print volume bar
        std::cout << "Volume: ";
        for (int i = 0; i < barLength; ++i) std::cout << "#";
        std::cout << "\n";
    }
#endif
    return isRunning;
}
void MusicVisualizer::render() {
    static int frameCount = 0;
    if (frameCount++ % 30 == 0) {
        std::cout << "Frame " << frameCount << " - Visualizer running...\n";
    }
}

void MusicVisualizer::shutdown() {
    std::cout << "Shutting down...\n";
    isRunning = false;

#if ENABLE_REAL_AUDIO
    if (audioCapture) audioCapture->shutdown();
#endif
}
// CPU reference function for validation
void cpuAbsGainClamp(const float* in, float* out, int N, float gain) {
    for (int i = 0; i < N; ++i) {
        float v = std::fabs(in[i]) * gain;
        if (v > 1.0f) v = 1.0f;
        out[i] = v;
    }
}

int main() {
    std::cout << "=== CUDA Music Visualizer ===\n";

    MusicVisualizer visualizer;

    if (!visualizer.initialize()) {
        std::cerr << "Failed to initialize visualizer!\n";
        return -1;
    }

    visualizer.run();

    visualizer.shutdown();

    std::cout << "Program completed successfully!\n";
    // Dummy allocation to check CUDA setup
	std::cout << "cuda testing...\n";

    // Create test data

        const int bufferSize = 1024;
        const int N = 4;
        float input[N] = { -0.2f, 0.0f, 0.5f, 2.0f };
        float gpuOutput[N];
        float cpuOutput[N];

        AudioProcessor processor(bufferSize);
        if (!processor.initialize()) return 1;
		std::cout << "AudioProcessor initialized successfully!\n";

        processor.setGain(0.5f);
        processor.process(input,gpuOutput,N);
        cpuAbsGainClamp(input, cpuOutput, N, 0.5f);
        // Compare results
        bool pass = true;
        for (int i = 0; i < N; ++i) {
            if (std::fabs(cpuOutput[i] - gpuOutput[i]) > 1e-6) {
                pass = false;
            }
            std::cout << "i=" << i
                << " in=" << input[i]
                << " cpu=" << cpuOutput[i]
                << " gpu=" << gpuOutput[i]
                << "\n";
        }

        if (pass) {
            std::cout << "PASS ? — GPU matches CPU\n";
        }
        else {
            std::cout << "FAIL ? — mismatch found\n";
        }



    return 0;
}