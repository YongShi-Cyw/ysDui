/**
 * 文件名：DuiUiDispatcher.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明 UI 线程派发器：把其他线程的调用安全地转交到 UI 线程执行。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace ysDui::core {

/**
 * UI 线程派发器。
 *
 * 解决的问题：`THREADING.md` 规定控件树、宿主、画布与回调都只能在 UI 线程访问，
 * 而后台线程（如 LLM 流式响应）产出的增量需要落到界面上。本类提供唯一被允许的跨线程通道。
 *
 * 两条投递通道：
 * - `Post`：**顺序队列**，按投递顺序逐条执行，不丢任务。适合"每个事件都要处理"的场合。
 * - `PostCoalesced`：**按键合并槽**，同一个 key 只保留最后一次投递的任务。适合流式增量——
 *   后台高频产出，UI 线程每帧最多应用一次，中间值被覆盖而不是排队（否则会把消息内容逐字重排版上万次）。
 *
 * 执行时机：由 `core::Host::PrepareFrame()` 在绘制前调用 `Drain()`，因此任务在"应用后同帧排版并绘制"。
 * 队列由空转为非空时触发一次 `SetPendingHandler` 回调，供平台后端唤醒可能正阻塞在 `GetMessage` 的消息循环；
 * 没有这个唤醒，后台投递的任务会一直等到下一次输入事件才被执行（表现为界面"卡住不动"）。
 *
 * 生命周期：任务闭包在 UI 线程执行，因此可以安全地捕获控件指针；但调用方必须保证任务执行时对象仍然存活
 * （页面销毁前调用 `Clear()` 丢弃全部待处理任务）。
 */
class DuiUiDispatcher final {
public:
    /** 按键合并槽的标识类型。 */
    using CoalesceKey = std::uint64_t;

    DuiUiDispatcher();
    ~DuiUiDispatcher();
    // 内部持锁且通常被宿主长期引用：禁止拷贝与移动
    DuiUiDispatcher(const DuiUiDispatcher&) = delete;
    DuiUiDispatcher& operator=(const DuiUiDispatcher&) = delete;
    DuiUiDispatcher(DuiUiDispatcher&&) = delete;
    DuiUiDispatcher& operator=(DuiUiDispatcher&&) = delete;

    /**
     * 投递顺序任务（可在任意线程调用）。
     * @param task 待执行任务；空任务被忽略
     */
    void Post(std::function<void()> task);

    /**
     * 按键投递任务：同一 key 只保留最后一次投递的任务（可在任意线程调用）。
     * 重复投递同一 key 不会改变它在执行序列中的位置，只替换载荷。
     * @param key 合并键，由调用方定义（如消息序号）
     * @param task 待执行任务；空任务被忽略
     */
    void PostCoalesced(CoalesceKey key, std::function<void()> task);

    /**
     * 丢弃指定 key 尚未执行的按键任务（可在任意线程调用）。
     * 用途：流式结束时先丢弃挂起的增量，再投递最终全文，
     * 否则较早的增量会在最终内容之后执行并把它覆盖回旧文本。
     * @param key 合并键
     */
    void CancelCoalesced(CoalesceKey key);

    /**
     * 取出并执行全部待处理任务（仅 UI 线程调用）。
     * 执行期间新投递的任务留到下一次 Drain，避免在本次执行中无限追加。
     * 执行顺序：先顺序队列，后按键合并槽（合并槽存的是"最新状态"，应最后应用）。
     * 若某个任务执行期间调用了 `Clear()`（典型场景：任务里销毁了页面），本次 Drain 立即停止，
     * 剩余快照任务被丢弃——否则它们会去访问已经释放的对象。
     * @return 本次执行的任务数
     */
    std::size_t Drain();

    /**
     * 丢弃全部待处理任务，不执行（仅 UI 线程调用）。
     * 用途：页面或控件树即将销毁时，防止挂起任务访问已释放对象。
     * @return 被丢弃的任务数
     */
    std::size_t Clear();

    /** @return 是否有待处理任务（可在任意线程调用） */
    [[nodiscard]] bool HasPending() const;

    /**
     * 设置"有待处理任务"回调。
     * 队列由空转为非空时触发一次，用于唤醒消息循环。
     * **该回调可能由投递方线程触发**（唤醒必须发生在那一侧），因此实现必须线程安全；
     * Win32 下用 `::InvalidateRect` 即可（允许跨线程对同进程窗口调用）。
     *
     * 约束：回调**必须是非阻塞的异步唤醒**并尽快返回。它在投递线程上同步执行，
     * 而 UI 线程可能正在 join 这个投递方（见 Gallery 的 ChatStream 销毁流程：先 join 再取消合并键）。
     * 若唤醒实现会等待 UI 线程（`SendMessage`、等待事件、获取 UI 线程持有的锁），就会死锁。
     * @param handler 唤醒回调；空回调表示取消
     */
    void SetPendingHandler(std::function<void()> handler);

    /** @return 待处理任务数（顺序队列 + 按键合并槽，可在任意线程调用） */
    [[nodiscard]] std::size_t PendingCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::core
