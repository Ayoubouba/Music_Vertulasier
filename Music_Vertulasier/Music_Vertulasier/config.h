#pragma once

// Audio Configuration
#define SAMPLE_RATE         44100
#define AUDIO_BUFFER_SIZE   2048
#define FFT_SIZE            1024

// Graphics Configuration
#define WINDOW_WIDTH        1024
#define WINDOW_HEIGHT       768
#define TARGET_FPS          60

// CUDA Configuration
#define CUDA_BLOCK_SIZE     256

// Feature Flags
#define ENABLE_REAL_AUDIO   1
#define ENABLE_GRAPHICS     0
#define ENABLE_BEAT_DETECTION 0

// Debug
#define DEBUG_PRINT_AUDIO   1
#define DEBUG_PRINT_FFT     0
