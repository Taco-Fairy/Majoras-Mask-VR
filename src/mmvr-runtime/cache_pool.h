#pragma once
#include <algorithm>
#include <cstddef>
#include <memory_resource>

namespace mmvr {
// Renderer-thread only. Reuse allocator storage, never resource objects or
// cache keys. Clear each map at its original lifetime boundary. Maps must be
// declared after this owner so they are destroyed before their allocator.
class CachePool final : private std::pmr::memory_resource {
  public:
    CachePool() : pool_({}, this) {
    }
    CachePool(const CachePool&) = delete;
    CachePool& operator=(const CachePool&) = delete;
    std::pmr::memory_resource* Resource() noexcept {
        return &pool_;
    }
    std::size_t Allocations() const noexcept {
        return allocations_;
    }
    std::size_t RetainedBytes() const noexcept {
        return retained_;
    }
    std::size_t PeakBytes() const noexcept {
        return peak_;
    }

  private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* value = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        ++allocations_;
        retained_ += bytes;
        peak_ = std::max(peak_, retained_);
        return value;
    }
    void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) override {
        retained_ -= bytes;
        std::pmr::new_delete_resource()->deallocate(value, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
    std::size_t allocations_ = 0, retained_ = 0, peak_ = 0;
    // Use the standard library's supported size classes. Explicit small limits
    // can bypass pooling for native nodes on Android libc++. The pool retains
    // a storage high-water mark and releases it when this owner is destroyed.
    std::pmr::unsynchronized_pool_resource pool_;
};
} // namespace mmvr
