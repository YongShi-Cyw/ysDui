#include "ysDui/core/DuiAnimationClock.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace ysDui::core {
class AnimationClock::Impl { public: struct Task { TaskId id; int duration; int elapsed; std::function<void(double)> callback; }; TaskId next{}; std::vector<Task> tasks; std::function<void(bool)> activityChanged; };
AnimationClock::AnimationClock() : impl_(std::make_unique<Impl>()) {}
AnimationClock::~AnimationClock() = default;
AnimationClock::AnimationClock(AnimationClock&&) noexcept = default;
AnimationClock& AnimationClock::operator=(AnimationClock&&) noexcept = default;
AnimationClock::TaskId AnimationClock::Schedule(int duration, std::function<void(double)> callback) {
    const bool wasEmpty = impl_->tasks.empty();
    const TaskId id = ++impl_->next;
    impl_->tasks.push_back({id, (std::max)(0, duration), 0, std::move(callback)});
    if (wasEmpty && impl_->activityChanged) impl_->activityChanged(true);
    return id;
}
void AnimationClock::Cancel(TaskId task) {
    const bool wasActive = !impl_->tasks.empty();
    std::erase_if(impl_->tasks, [task](const Impl::Task& current) { return current.id == task; });
    if (wasActive && impl_->tasks.empty() && impl_->activityChanged) impl_->activityChanged(false);
}
void AnimationClock::Advance(int elapsed) {
    elapsed = (std::max)(0, elapsed);
    std::vector<TaskId> ids; for (const auto& task : impl_->tasks) ids.push_back(task.id);
    for (TaskId id : ids) {
        auto it = std::find_if(impl_->tasks.begin(), impl_->tasks.end(), [id](const Impl::Task& task) { return task.id == id; });
        if (it == impl_->tasks.end()) continue;
        it->elapsed += elapsed; const double progress = it->duration == 0 ? 1.0 : (std::min)(1.0, static_cast<double>(it->elapsed) / it->duration);
        auto callback = it->callback; callback(progress);
        if (progress >= 1.0) Cancel(id);
    }
}
bool AnimationClock::HasScheduledTasks() const { return !impl_->tasks.empty(); }
void AnimationClock::SetActivityChangedHandler(std::function<void(bool)> handler) {
    impl_->activityChanged = std::move(handler);
    if (impl_->activityChanged && !impl_->tasks.empty()) impl_->activityChanged(true);
}
} // namespace ysDui::core
