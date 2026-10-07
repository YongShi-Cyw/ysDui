/**
 * 文件名：DuiUiDispatcher.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：实现 UI 线程派发器的线程安全队列、按键合并与唤醒通知。
 */
#include "ysDui/core/DuiUiDispatcher.hpp"

#include <deque>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ysDui::core {

class DuiUiDispatcher::Impl
{
public:
    std::mutex mutex;
    /** 顺序任务队列。 */
    std::deque<std::function<void()>> queue;
    /** 按键合并槽：key → 最新任务。 */
    std::unordered_map<CoalesceKey, std::function<void()>> coalesced;
    /** 合并槽的首次投递顺序，保证执行顺序稳定可预期。 */
    std::deque<CoalesceKey> order;
    /** Clear() 递增；Drain 据此发现"执行中途被清空"并停止后续任务。 */
    std::uint64_t generation{};
    std::function<void()> pending;

    /** @return 当前是否有待处理任务（调用方须已持锁）。 */
    [[nodiscard]] bool HasPendingLocked() const
    {
        return !queue.empty() || !coalesced.empty();
    }
};

DuiUiDispatcher::DuiUiDispatcher() : impl_(std::make_unique<Impl>()) {}
DuiUiDispatcher::~DuiUiDispatcher() = default;

void DuiUiDispatcher::Post(std::function<void()> task)
{
    if (!task)
        return;
    std::function<void()> notify;
    {
        const std::lock_guard lock(impl_->mutex);
        const bool wasEmpty = !impl_->HasPendingLocked();
        impl_->queue.push_back(std::move(task));
        // 唤醒回调在锁外调用：它可能反过来投递任务（如平台后端重绘时又产生任务）
        if (wasEmpty)
            notify = impl_->pending;
    }
    if (notify)
        notify();
}

void DuiUiDispatcher::PostCoalesced(CoalesceKey key, std::function<void()> task)
{
    if (!task)
        return;
    std::function<void()> notify;
    {
        const std::lock_guard lock(impl_->mutex);
        const bool wasEmpty = !impl_->HasPendingLocked();
        const auto found = impl_->coalesced.find(key);
        if (found == impl_->coalesced.end())
        {
            impl_->coalesced.emplace(key, std::move(task));
            impl_->order.push_back(key);
        }
        else
        {
            // 覆盖载荷但保留原位置：同一 key 的执行次序不会因为反复投递而漂移
            found->second = std::move(task);
        }
        if (wasEmpty)
            notify = impl_->pending;
    }
    if (notify)
        notify();
}

void DuiUiDispatcher::CancelCoalesced(CoalesceKey key)
{
    const std::lock_guard lock(impl_->mutex);
    if (impl_->coalesced.erase(key) == 0)
        return;
    for (auto it = impl_->order.begin(); it != impl_->order.end(); ++it)
    {
        if (*it == key)
        {
            impl_->order.erase(it);
            break;
        }
    }
}

std::size_t DuiUiDispatcher::Drain()
{
    std::deque<std::function<void()>> queue;
    std::vector<std::function<void()>> coalescedTasks;
    std::uint64_t generation{};
    {
        const std::lock_guard lock(impl_->mutex);
        generation = impl_->generation;
        queue.swap(impl_->queue);
        coalescedTasks.reserve(impl_->order.size());
        for (const CoalesceKey key : impl_->order)
        {
            const auto found = impl_->coalesced.find(key);
            if (found != impl_->coalesced.end() && found->second)
                coalescedTasks.push_back(std::move(found->second));
        }
        impl_->coalesced.clear();
        impl_->order.clear();
    }

    std::size_t executed{};
    const auto run = [this, generation, &executed](std::function<void()>& task)
    {
        if (!task)
            return false;
        task();
        ++executed;
        // 任务内部可能销毁了控件树并调用 Clear()：此时后续任务必须丢弃
        const std::lock_guard lock(impl_->mutex);
        return impl_->generation == generation;
    };

    for (auto& task : queue)
    {
        if (!run(task))
            return executed;
    }
    for (auto& task : coalescedTasks)
    {
        if (!run(task))
            return executed;
    }
    return executed;
}

std::size_t DuiUiDispatcher::Clear()
{
    const std::lock_guard lock(impl_->mutex);
    ++impl_->generation;
    const std::size_t discarded = impl_->queue.size() + impl_->coalesced.size();
    impl_->queue.clear();
    impl_->coalesced.clear();
    impl_->order.clear();
    return discarded;
}

bool DuiUiDispatcher::HasPending() const
{
    const std::lock_guard lock(impl_->mutex);
    return impl_->HasPendingLocked();
}

std::size_t DuiUiDispatcher::PendingCount() const
{
    const std::lock_guard lock(impl_->mutex);
    return impl_->queue.size() + impl_->coalesced.size();
}

void DuiUiDispatcher::SetPendingHandler(std::function<void()> handler)
{
    const std::lock_guard lock(impl_->mutex);
    impl_->pending = std::move(handler);
}

} // namespace ysDui::core
