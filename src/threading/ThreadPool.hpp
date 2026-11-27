#pragma once

#include "threading/Task.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace voxelforge {

// Fixed-size worker pool with a two level (normal / high priority) FIFO job queue.
// Destruction discards jobs that have not started and joins the workers after the
// running jobs finish, so anything a job references must outlive the pool.
class ThreadPool {
public:
    using Job = Task;
    enum class Priority { Normal, High };

    explicit ThreadPool(unsigned threadCount);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(Job job, Priority priority = Priority::Normal);

    // Blocks until the queue is empty and no job is running.
    void waitIdle();
    // Drops every job that has not started yet; returns how many were dropped.
    std::size_t cancelPending();

    std::size_t queuedJobs() const;
    std::size_t runningJobs() const;
    unsigned threadCount() const { return static_cast<unsigned>(m_threads.size()); }

    // Sensible default worker count: all hardware threads but one (for the main thread).
    static unsigned defaultThreadCount();

private:
    void workerLoop();

    mutable std::mutex m_mutex;
    std::condition_variable m_workAvailable;
    std::condition_variable m_idle;
    std::deque<Job> m_queue; // high priority jobs are pushed to the front
    std::size_t m_running = 0;
    bool m_stopping = false;
    std::vector<std::thread> m_threads;
};

} // namespace voxelforge
