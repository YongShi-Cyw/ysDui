#pragma once

#include <cstddef>
#include <functional>
#include <memory>

namespace ysDui::core {

class AnimationClock final {
public:
    using TaskId = std::size_t;
    AnimationClock();
    ~AnimationClock();
    AnimationClock(const AnimationClock&) = delete;
    AnimationClock& operator=(const AnimationClock&) = delete;
    AnimationClock(AnimationClock&&) noexcept;
    AnimationClock& operator=(AnimationClock&&) noexcept;
    [[nodiscard]] TaskId Schedule(int durationMilliseconds, std::function<void(double)> callback);
    void Cancel(TaskId task);
    void Advance(int elapsedMilliseconds);
    [[nodiscard]] bool HasScheduledTasks() const;
    void SetActivityChangedHandler(std::function<void(bool)> handler);
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::core
