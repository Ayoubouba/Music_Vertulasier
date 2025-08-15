#include <iostream>
#include <memory>
#include <thread>
#include "config.h"
#include "audio_cupture.h"
#include <chrono>
#include "audio_processor.h"
#include "pinned_buffer.h"
#include <vector>
#include "cuda_runtime.h"
#include <array>
#include <SDL2/SDL_rwops.h>
#include <SDL2/SDL_audio.h>

/*
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
// BenchStats class to track performance metrics
struct BenchStats {
    double h2d_ms = 0, kernel_ms = 0, d2h_ms = 0, total_ms = 0;
    int frames = 0;
    void add(double a, double b, double c) { h2d_ms += a; kernel_ms += b; d2h_ms += c; total_ms += (a + b + c); ++frames; }
    void print(const char* tag) const {
        double f = frames ? frames : 1;
        std::cout << tag << " avg per frame: "
            << "H2D=" << (h2d_ms / f) << " ms, "
            << "Kern=" << (kernel_ms / f) << " ms, "
            << "D2H=" << (d2h_ms / f) << " ms, "
            << "TotalGPU=" << (total_ms / f) << " ms\n";
    }
};
// CPU reference function for validation
inline void cpuAbsGainClamp(const float* in, float* out, int N, float gain) {
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
    return 0;
}*/

#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iomanip> // for std::setw

#include "config.h"
#include "audio_cupture.h"
#include "audio_processor.h"
#include "pinned_buffer.h"
#include "cuda_runtime.h"

// ====================== Circular Buffer ======================
class AudioCircularBuffer {
    std::vector<float> buffer;
    size_t head = 0, tail = 0;
    std::mutex mtx;
    std::condition_variable cv;

public:
    AudioCircularBuffer(size_t size) : buffer(size) {}

    void push(const float* data, size_t len) {
        std::lock_guard<std::mutex> lock(mtx);
        for (size_t i = 0; i < len; ++i) {
            buffer[head] = data[i];
            head = (head + 1) % buffer.size();
            if (head == tail) tail = (tail + 1) % buffer.size(); // overwrite oldest
        }
        cv.notify_one();
    }

    size_t pop(float* out, size_t len) {
        std::unique_lock<std::mutex> lock(mtx);
        size_t count = 0;
        while (tail != head && count < len) {
            out[count++] = buffer[tail];
            tail = (tail + 1) % buffer.size();
        }
        return count;
    }
};

// ====================== BenchStats ======================
struct BenchStats {
    double gpuMsTotal = 0;
    double cpuMsTotal = 0;
    int frames = 0;

    void add(double gpuMs, double cpuMs) {
        gpuMsTotal += gpuMs;
        cpuMsTotal += cpuMs;
        frames++;
    }

    void print() const {
        double f = frames ? frames : 1;
        std::cout << "\n=== Benchmark (average per frame) ===\n";
        std::cout << "GPU: " << std::fixed << std::setprecision(3) << gpuMsTotal / f << " ms | "
            << "CPU: " << cpuMsTotal / f << " ms\n";
    }
};

// ====================== MusicVisualizer ======================
class MusicVisualizer {
private:
    std::unique_ptr<AudioCapture> audioCapture;
    AudioCircularBuffer circBuffer;
    AudioProcessor gpu;
    PinnedBuffer<float> inPinned;
    PinnedBuffer<float> outPinned;
    bool isRunning = false;
    float gain;
    BenchStats benchStats;

public:
    MusicVisualizer(int frameSize, float g)
        : circBuffer(frameSize * 8),
        gpu(frameSize),
        inPinned(frameSize),
        outPinned(frameSize),
        gain(g) {
    }

    bool initialize() {
        std::cout << "Initializing Music Visualizer...\n";

#if ENABLE_REAL_AUDIO
        audioCapture = std::make_unique<AudioCapture>();
        if (!audioCapture->initialize() || !audioCapture->start()) {
            std::cerr << "Failed to start audio capture.\n";
            return false;
        }
#endif
        if (!gpu.initialize()) {
            std::cerr << "GPU initialization failed.\n";
            return false;
        }
        gpu.setGain(gain);

        std::cout << "Initialization complete!\n";
        return true;
    }

    void run(int maxFrames = 500) {
        isRunning = true;

        // Launch GPU processing thread
        std::thread gpuThread(&MusicVisualizer::processGPU, this);

        int frameCount = 0;
        while (isRunning && frameCount++ < maxFrames) {
            update();  // pull audio and print moving bar
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }

        isRunning = false;
        gpuThread.join();

        benchStats.print();
    }

    void shutdown() {
        std::cout << "\nShutting down...\n";
#if ENABLE_REAL_AUDIO
        if (audioCapture) audioCapture->shutdown();
#endif
    }

private:
    void update() {
#if ENABLE_REAL_AUDIO
        std::vector<float> buffer;
        if (audioCapture->getAudioFrame(buffer)) {
            circBuffer.push(buffer.data(), buffer.size());

            // Compute average amplitude for volume bar
            float sum = 0;
            for (float s : buffer) sum += std::abs(s);
            float avg = sum / buffer.size();

            // Print moving bar
            int barLength = static_cast<int>(avg * 50);
            if (barLength > 50) barLength = 50;

            std::cout << "\rVoice: ";
            for (int i = 0; i < barLength; ++i) std::cout << "#";
            for (int i = barLength; i < 50; ++i) std::cout << " ";
            std::cout << std::flush;
        }
#endif
    }

    void processGPU() {
        std::vector<float> frame(inPinned.size());

        cudaEvent_t eStart, eEnd;
        cudaEventCreate(&eStart);
        cudaEventCreate(&eEnd);

        while (isRunning) {
            size_t n = circBuffer.pop(frame.data(), frame.size());
            if (n > 0) {
                // Copy to pinned memory
                std::copy(frame.begin(), frame.begin() + n, inPinned.data());

                // GPU timing
                cudaEventRecord(eStart, 0);
                gpu.process(inPinned.data(), outPinned.data(), n);
                cudaEventRecord(eEnd, 0);
                cudaEventSynchronize(eEnd);
                float gpuMs = 0.0f;
                cudaEventElapsedTime(&gpuMs, eStart, eEnd);

                // CPU reference
                auto t0 = std::chrono::high_resolution_clock::now();
                std::vector<float> outCPU(n);
                for (size_t i = 0; i < n; ++i) {
                    float v = std::fabs(frame[i]) * gain;
                    if (v > 1.0f) v = 1.0f;
                    outCPU[i] = v;
                }
                auto t1 = std::chrono::high_resolution_clock::now();
                double cpuMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

                benchStats.add(gpuMs, cpuMs);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        cudaEventDestroy(eStart);
        cudaEventDestroy(eEnd);
    }
};

// ====================== Main ======================
int main() {
    const int frameSize = 1024;
    const float gain = 0.8f;

    MusicVisualizer visualizer(frameSize, gain);

    if (!visualizer.initialize()) return -1;

    visualizer.run(500); // run for 500 frames

    visualizer.shutdown();
    std::cout << "\nProgram completed successfully!\n";
    return 0;
}
