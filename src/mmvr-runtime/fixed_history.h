#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace mmvr {
// Renderer-thread motion samples only. No owning objects and no heap allocation.
// Callers retain their existing 32-sample/time-window pruning; capacity includes
// the transient extra sample before pruning. A full buffer drops its oldest value.
template <class T, std::size_t Capacity = 64> class FixedHistory {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>);
    std::array<T, Capacity> values_{};
    std::size_t first_ = 0, count_ = 0;

  public:
    bool empty() const {
        return count_ == 0;
    }
    std::size_t size() const {
        return count_;
    }
    void clear() {
        first_ = count_ = 0;
    }
    T& operator[](std::size_t i) {
        assert(i < count_);
        return values_[(first_ + i) & (Capacity - 1)];
    }
    const T& operator[](std::size_t i) const {
        assert(i < count_);
        return values_[(first_ + i) & (Capacity - 1)];
    }
    T& front() {
        return (*this)[0];
    }
    const T& front() const {
        return (*this)[0];
    }
    T& back() {
        return (*this)[count_ - 1];
    }
    const T& back() const {
        return (*this)[count_ - 1];
    }
    void pop_front() {
        assert(count_);
        first_ = (first_ + 1) & (Capacity - 1);
        --count_;
    }
    void pop_back() {
        assert(count_);
        --count_;
    }
    void push_back(T value) {
        if (count_ == Capacity)
            pop_front();
        values_[(first_ + count_) & (Capacity - 1)] = value;
        ++count_;
    }
};
} // namespace mmvr
