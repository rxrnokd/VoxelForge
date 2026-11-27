#include "threading/ThreadPool.hpp"

#include <algorithm>
#include <utility>

namespace voxelforge {

ThreadPool::ThreadPool(unsigned threadCount) {
    const unsigned count = std::max(1u, threadCount);
    m_threads.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        m_threads.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
        m_queue.clear();
    }
    m_workAvailable.notify_all();
    for (std::thread& t : m_threads) {
        t.join();
    }
}

unsigned ThreadPool::defaultThreadCount() {
    const unsigned hw = std::thread::hardware_concurrency();
    return std::clamp(hw > 1 ? hw - 1 : 1u, 1u, 12u);
}

void ThreadPool::submit(Job job, Priority priority) {
    {
        std::lock_guard lock(m_mutex);
        if (m_stopping) {
            return;
        }
        if (priority == Priority::High) {
            m_queue.push_front(std::move(job));
        } else {
            m_queue.push_back(std::move(job));
        }
    }
    m_workAvailable.notify_one();
}

void ThreadPool::waitIdle() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [this] { return m_queue.empty() && m_running == 0; });
}

std::size_t ThreadPool::cancelPending() {
    std::size_t dropped = 0;
    {
        std::lock_guard lock(m_mutex);
        dropped = m_queue.size();
        m_queue.clear();
    }
    m_idle.notify_all();
    return dropped;
}

std::size_t ThreadPool::queuedJobs() const {
    std::lock_guard lock(m_mutex);
    return m_queue.size();
}

std::size_t ThreadPool::runningJobs() const {
    std::lock_guard lock(m_mutex);
    return m_running;
}

void ThreadPool::workerLoop() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(m_mutex);
            m_workAvailable.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
            if (m_stopping) {
                return;
            }
            job = std::move(m_queue.front());
            m_queue.pop_front();
            ++m_running;
        }
        job();
        {
            std::lock_guard lock(m_mutex);
            --m_running;
            if (m_queue.empty() && m_running == 0) {
                m_idle.notify_all();
            }
        }
    }
}

} // namespace voxelforge
