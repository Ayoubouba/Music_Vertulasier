#pragma once
#include <cuda_runtime.h>
#include <stdexcept>

template<typename T>
class PinnedBuffer {
public:
    PinnedBuffer() = default;
    explicit PinnedBuffer(size_t n) { alloc(n); }
    ~PinnedBuffer() { free(); }

    void alloc(size_t n) {
        free();
        size_ = n;
        auto err = cudaHostAlloc((void**)&ptr_, n * sizeof(T), cudaHostAllocPortable);
        if (err != cudaSuccess) throw std::runtime_error("cudaHostAlloc failed");
    }
    void free() {
        if (ptr_) { cudaFreeHost(ptr_); ptr_ = nullptr; size_ = 0; }
    }
    T* data() { return ptr_; }
    const T* data() const { return ptr_; }
    size_t size() const { return size_; }

private:
    T* ptr_ = nullptr;
    size_t size_ = 0;
};
