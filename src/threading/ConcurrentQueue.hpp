#pragma once

#include <deque>
#include <mutex>
#include <utility>

namespace voxelforge {

// Minimal mutex protected multi-producer / multi-consumer queue used to hand results
// from worker threads back to the main thread.
template <typename T>
class ConcurrentQueue {
public:
    void push(T value) {
        std::lock_guard lock(m_mutex);
        m_items.push_back(std::move(value));
    }

    bool tryPop(T& out) {
        std::lock_guard lock(m_mutex);
        if (m_items.empty()) {
            return false;
        }
        out = std::move(m_items.front());
        m_items.pop_front();
        return true;
    }

    std::size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_items.size();
    }

    void clear() {
        std::lock_guard lock(m_mutex);
        m_items.clear();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<T> m_items;
};

} // namespace voxelforge
