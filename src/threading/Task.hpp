#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace voxelforge {

// Move-only type-erased `void()` callable (std::function requires copyable targets, and
// C++20 has no std::move_only_function). Jobs can therefore own unique_ptr buffers.
class Task {
public:
    Task() = default;

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Task>>>
    Task(F&& f) : m_impl(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(f))) {} // NOLINT: implicit by design

    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void operator()() { m_impl->call(); }
    explicit operator bool() const { return m_impl != nullptr; }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void call() = 0;
    };
    template <typename F>
    struct Model final : Concept {
        explicit Model(F&& fn) : f(std::move(fn)) {}
        explicit Model(const F& fn) : f(fn) {}
        void call() override { f(); }
        F f;
    };

    std::unique_ptr<Concept> m_impl;
};

} // namespace voxelforge
