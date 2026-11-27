#include "doctest.h"

#include "threading/ConcurrentQueue.hpp"
#include "threading/ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace voxelforge;

TEST_CASE("thread pool: runs every submitted job") {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    for (int i = 0; i < 1000; ++i) {
        pool.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    pool.waitIdle();
    CHECK(counter.load() == 1000);
    CHECK(pool.queuedJobs() == 0);
    CHECK(pool.runningJobs() == 0);
}

TEST_CASE("thread pool: move-only jobs and results through a concurrent queue") {
    ThreadPool pool(3);
    ConcurrentQueue<std::unique_ptr<int>> results;
    for (int i = 0; i < 200; ++i) {
        auto payload = std::make_unique<int>(i);
        pool.submit([&results, p = std::move(payload)]() mutable { results.push(std::move(p)); });
    }
    pool.waitIdle();
    CHECK(results.size() == 200);
    long long sum = 0;
    std::unique_ptr<int> v;
    while (results.tryPop(v)) {
        sum += *v;
    }
    CHECK(sum == 199LL * 200 / 2);
}

TEST_CASE("thread pool: submissions from several threads") {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&] {
            for (int i = 0; i < 250; ++i) {
                pool.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); },
                            i % 2 ? ThreadPool::Priority::High : ThreadPool::Priority::Normal);
            }
        });
    }
    for (auto& t : producers) t.join();
    pool.waitIdle();
    CHECK(counter.load() == 1000);
}

TEST_CASE("thread pool: high priority jobs run before queued normal ones") {
    ThreadPool pool(1);
    std::atomic<bool> release{false};
    std::vector<int> order;
    std::mutex orderMutex;
    // Block the single worker so that the queue fills up.
    pool.submit([&release] {
        while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    while (pool.runningJobs() == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    for (int i = 0; i < 3; ++i) {
        pool.submit([&, i] { std::lock_guard l(orderMutex); order.push_back(i); });
    }
    pool.submit([&] { std::lock_guard l(orderMutex); order.push_back(100); }, ThreadPool::Priority::High);
    release = true;
    pool.waitIdle();
    REQUIRE(order.size() == 4);
    CHECK(order.front() == 100);
}

TEST_CASE("thread pool: cancel and destroy with pending work") {
    std::atomic<int> ran{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 500; ++i) {
            pool.submit([&ran] {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                ran.fetch_add(1);
            });
        }
        const std::size_t dropped = pool.cancelPending();
        CHECK(dropped + static_cast<std::size_t>(ran.load()) <= 500);
        pool.waitIdle();
        // Destructor must join cleanly even though jobs were queued.
        for (int i = 0; i < 100; ++i) {
            pool.submit([&ran] { ran.fetch_add(1); });
        }
    }
    CHECK(ran.load() <= 600);
}
