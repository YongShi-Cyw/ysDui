/**
 * 文件名：lifecycle_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：验证控件树、外部输入、Popup 和异步加载器的销毁边界。
 */
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ysDui/controls/input/DuiColorPicker.hpp"
#include "ysDui/controls/input/DuiComboBox.hpp"
#include "ysDui/controls/input/DuiDateTimePicker.hpp"
#include "ysDui/controls/input/DuiDoubleSpinBox.hpp"
#include "ysDui/controls/input/DuiEditHost.hpp"
#include "ysDui/controls/input/DuiPathEdit.hpp"
#include "ysDui/controls/input/DuiRichEditHost.hpp"
#include "ysDui/controls/input/DuiSearchBox.hpp"
#include "ysDui/controls/input/DuiSpinBox.hpp"
#include "ysDui/controls/list/DuiDataGrid.hpp"
#include "ysDui/controls/list/DuiMenu.hpp"
#include "ysDui/controls/list/DuiPropertyGrid.hpp"
#include "ysDui/controls/layout/DuiLayout.hpp"
#include "ysDui/controls/media/DuiAsyncImageLoader.hpp"
#include "ysDui/core/DuiHost.hpp"
#include "ysDui/core/DuiSkin.hpp"
#include "ysDui/core/DuiSubscription.hpp"
#include "ysDui/core/DuiTheme.hpp"
#include "ysDui/core/DuiUiDispatcher.hpp"
#include "ysDui/ui/DuiPopupHost.hpp"
#include "ysDui/ui/DuiRichTextInput.hpp"
#include "ysDui/ui/DuiTextInput.hpp"

namespace {
/** 记录 Layout 调用次数：用于验证"任务先执行、排版紧随其后"的同帧不变量。 */
class LayoutRecordingControl final : public ysDui::core::Control
{
public:
    void Layout(ysDui::core::Rect bounds) override
    {
        ++layoutCount;
        ysDui::core::Control::Layout(bounds);
    }

    int layoutCount{};
};

class TextInputMock final : public ysDui::ui::DuiTextInput
{
public:
    void SetBounds(ysDui::core::Rect value) override { bounds = value; }
    [[nodiscard]] ysDui::core::Rect Bounds() const override { return bounds; }
    void SetVisible(bool) override {}
    void SetEnabled(bool) override {}
    void SetBorderVisible(bool) override {}
    void SetOptions(const ysDui::ui::DuiTextInputOptions&) override {}
    void SetPlaceholder(std::string) override {}
    void SetText(std::string value) override
    {
        text = std::move(value);
        if (changed) changed();
    }
    [[nodiscard]] std::string Text() const override { return text; }
    void Focus() override {}
    void SetChangedHandler(std::function<void()> handler) override { changed = std::move(handler); }
    void SetFocusLostHandler(std::function<void()> handler) override { focusLost = std::move(handler); }
    void SetSubmitHandler(std::function<void()> handler) override { submitted = std::move(handler); }
    void LoseFocus() { if (focusLost) focusLost(); }
    void Submit() { if (submitted) submitted(); }

    ysDui::core::Rect bounds;
    std::string text;
    std::function<void()> changed;
    std::function<void()> focusLost;
    std::function<void()> submitted;
};

class RichTextInputMock final : public ysDui::ui::DuiRichTextInput
{
public:
    void SetBounds(ysDui::core::Rect value) override { bounds = value; }
    [[nodiscard]] ysDui::core::Rect Bounds() const override { return bounds; }
    void SetVisible(bool) override {}
    void SetEnabled(bool) override {}
    void SetBorderVisible(bool) override {}
    void SetOptions(const ysDui::ui::DuiTextInputOptions&) override {}
    void SetPlaceholder(std::string) override {}
    void SetText(std::string value) override { text = std::move(value); if (changed) changed(); }
    [[nodiscard]] std::string Text() const override { return text; }
    void Focus() override {}
    void SetChangedHandler(std::function<void()> handler) override { changed = std::move(handler); }
    void SetFocusLostHandler(std::function<void()> handler) override { focusLost = std::move(handler); }
    void SetSelection(ysDui::ui::DuiTextRange value) override { selection = value; }
    [[nodiscard]] ysDui::ui::DuiTextRange Selection() const override { return selection; }
    void SelectAll() override {}
    void ReplaceSelection(std::string) override {}
    void AppendText(std::string) override {}
    [[nodiscard]] bool CanUndo() const override { return false; }
    void Undo() override {}
    void Cut() override {}
    void Copy() override {}
    void Paste() override {}
    void ClearSelection() override {}
    void SetSelectionFormat(ysDui::ui::DuiRichTextFormat) override {}
    void SetAutomaticLinkDetection(bool) override {}
    void SetLinkActivatedHandler(std::function<void(std::string)> handler) override
    {
        linkActivated = std::move(handler);
    }
    [[nodiscard]] bool InsertImage(const ysDui::render::DuiImage&, ysDui::core::Size) override { return false; }
    void InsertQuoteBlock(std::string, std::string) override {}
    void InsertFileCard(std::string, std::uint64_t) override {}
    void EmitLink() { if (linkActivated) linkActivated("https://after-owner.test"); }
    void LoseFocus() { if (focusLost) focusLost(); }

    ysDui::core::Rect bounds;
    ysDui::ui::DuiTextRange selection;
    std::string text;
    std::function<void()> changed;
    std::function<void()> focusLost;
    std::function<void(std::string)> linkActivated;
};

class PopupHostMock final : public ysDui::ui::IPopupHost
{
public:
    bool Show(const ysDui::ui::DuiPopupOptions& options,
              std::unique_ptr<ysDui::core::Control> value,
              LayoutHandler layout,
              PaintHandler) override
    {
        content = std::move(value);
        if (layout) layout({0, 0, options.size.width, options.size.height});
        return true;
    }
    void SetOptions(const ysDui::ui::DuiPopupOptions&) override {}
    void Hide() override { if (dismissed) dismissed(); }
    void RequestHide() override { if (dismissed) dismissed(); }
    void RequestMinimize() override {}
    void RequestMove() override { requestedMove = true; }
    [[nodiscard]] bool Visible() const override { return content != nullptr; }
    [[nodiscard]] ysDui::ui::HostRef Reference() const override { return {}; }
    void SetDismissedHandler(std::function<void()> handler) override { dismissed = std::move(handler); }
    bool Dispatch(const ysDui::core::Event& event) { return content && content->OnEvent(event); }

    std::unique_ptr<ysDui::core::Control> content;
    std::function<void()> dismissed;
    bool requestedMove{};
};

class DeletingControl final : public ysDui::core::Control
{
public:
    bool OnEvent(const ysDui::core::Event&) override
    {
        if (handler) handler();
        return true;
    }

    std::function<void()> handler;
};

template <typename Control>
void VerifyTextInputBinding()
{
    auto control = std::make_unique<Control>();
    {
        TextInputMock input;
        control->SetTextInput(&input);
    }
    control.reset();

    TextInputMock input;
    {
        Control scopedControl;
        scopedControl.SetTextInput(&input);
    }
    input.SetText("after-owner-destruction");
    input.LoseFocus();
    input.Submit();
}

void VerifyRichTextInputBinding()
{
    auto control = std::make_unique<ysDui::controls::input::DuiRichEditHost>();
    {
        RichTextInputMock input;
        control->SetRichTextInput(&input);
    }
    control.reset();

    RichTextInputMock input;
    {
        ysDui::controls::input::DuiRichEditHost scopedControl;
        scopedControl.SetRichTextInput(&input);
    }
    input.SetText("after-owner-destruction");
    input.EmitLink();
    input.LoseFocus();
}

template <typename Control>
void VerifyPopupHostDestroyedFirst()
{
    auto control = std::make_unique<Control>();
    {
        PopupHostMock host;
        control->SetPopupHost(&host);
    }
    control.reset();
}

void VerifyPopupCallbacksAfterOwnerDestruction()
{
    PopupHostMock comboHost;
    {
        ysDui::controls::input::DuiComboBox control;
        control.SetBounds({0, 0, 100, 24});
        control.AddItem("first");
        control.SetPopupHost(&comboHost);
        control.OpenPopup();
    }
    comboHost.Hide();
    (void)comboHost.Dispatch(ysDui::test::MakeEvent(ysDui::core::EventType::PointerDown, {5, 5}));

    PopupHostMock dateHost;
    {
        ysDui::controls::input::DuiDateTimePicker control;
        control.SetBounds({0, 0, 160, 24});
        control.SetPopupHost(&dateHost);
        control.OpenPopup();
    }
    dateHost.Hide();
    (void)dateHost.Dispatch(ysDui::test::MakeEvent(ysDui::core::EventType::KeyDown, {}, ysDui::core::key::Enter));

    PopupHostMock colorHost;
    {
        ysDui::controls::input::DuiColorPicker control;
        control.SetBounds({0, 0, 120, 24});
        control.SetPopupHost(&colorHost);
        control.OpenPopup();
    }
    colorHost.Hide();
    (void)colorHost.Dispatch(ysDui::test::MakeEvent(ysDui::core::EventType::PointerDown, {5, 5}));
}

void VerifyControlRemovalDuringDispatch()
{
    auto root = std::make_unique<ysDui::core::Control>();
    root->SetBounds({0, 0, 100, 100});
    auto child = std::make_unique<DeletingControl>();
    child->SetBounds({10, 10, 20, 20});
    DeletingControl* childPointer = child.get();
    ysDui::core::Control* rootPointer = root.get();
    child->handler = [rootPointer, childPointer]
    {
        (void)rootPointer->RemoveChild(childPointer);
    };
    root->AddChild(std::move(child));

    ysDui::core::Host host;
    host.SetRoot(std::move(root));
    host.SetFocusedControl(childPointer);
    assert(host.Dispatch(ysDui::test::MakeEvent(ysDui::core::EventType::PointerDown, {15, 15})));
    assert(host.FocusedControl() == nullptr);
    assert(rootPointer->Children().empty());
}

void VerifyAsyncLoaderDestruction()
{
    std::atomic<bool> decodeStarted{};
    std::atomic<bool> finishDecode{};
    std::thread finisher;
    {
        ysDui::controls::media::DuiAsyncImageLoader loader;
        loader.SetDecoder([&decodeStarted, &finishDecode](const std::string&) {
            decodeStarted = true;
            while (!finishDecode.load())
                std::this_thread::yield();
            return ysDui::render::DuiImage::CreateBgra8Premultiplied({1, 1}, {0, 0, 0, 255});
        });
        const std::uint64_t request = loader.Submit("destruction.png", 1);
        for (int attempt = 0; attempt < 100 && !decodeStarted.load(); ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(decodeStarted.load());
        loader.Cancel(request);
        finisher = std::thread([&finishDecode] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            finishDecode = true;
        });
    }
    finisher.join();
}

void VerifySubscriptionPublisherDestruction()
{
    ysDui::core::DuiSubscription subscription;
    {
        ysDui::core::DuiTheme theme;
        subscription = theme.SubscribeScoped([] {});
    }
    subscription.Reset();
    assert(!subscription);

    {
        ysDui::core::DuiSkinSession skin;
        subscription = skin.SubscribeScoped([] {});
    }
    subscription.Reset();
    assert(!subscription);

    {
        ysDui::controls::list::DuiMenu menu;
        subscription = menu.SubscribeClosedScoped([] {});
    }
    subscription.Reset();
    assert(!subscription);
}
void VerifyUiDispatcher()
{
    using ysDui::core::DuiUiDispatcher;

    // 顺序队列：先进先出，不丢任务
    {
        DuiUiDispatcher dispatcher;
        std::vector<int> order;
        dispatcher.Post([&order] { order.push_back(1); });
        dispatcher.Post([&order] { order.push_back(2); });
        dispatcher.Post([&order] { order.push_back(3); });
        assert(dispatcher.HasPending() && dispatcher.PendingCount() == 3);
        assert(dispatcher.Drain() == 3);
        assert((order == std::vector<int>{1, 2, 3}));
        // 空任务被忽略，且不占位置
        dispatcher.Post({});
        assert(!dispatcher.HasPending() && dispatcher.PendingCount() == 0);
        assert(dispatcher.Drain() == 0);
    }

    // 按键合并：同一 key 只保留最后一次，且执行次序固定在首次投递的位置
    {
        DuiUiDispatcher dispatcher;
        std::vector<std::string> order;
        dispatcher.PostCoalesced(7, [&order] { order.emplace_back("a-1"); });
        dispatcher.PostCoalesced(9, [&order] { order.emplace_back("b-1"); });
        dispatcher.PostCoalesced(7, [&order] { order.emplace_back("a-2"); });
        dispatcher.PostCoalesced(7, [&order] { order.emplace_back("a-3"); });
        // 合并槽只有两个 key，但投递了四次
        assert(dispatcher.PendingCount() == 2);
        assert(dispatcher.Drain() == 2);
        // key 7 只执行最后一次，且仍在 key 9 之前（保留首次投递位置）
        assert((order == std::vector<std::string>{"a-3", "b-1"}));
    }

    // 执行顺序：先顺序队列，后合并槽（合并槽存的是"最新状态"）
    {
        DuiUiDispatcher dispatcher;
        std::vector<int> order;
        dispatcher.PostCoalesced(1, [&order] { order.push_back(20); });
        dispatcher.Post([&order] { order.push_back(10); });
        assert(dispatcher.Drain() == 2);
        assert((order == std::vector<int>{10, 20}));
    }

    // CancelCoalesced：丢弃挂起的增量，避免较早的增量覆盖最终内容
    {
        DuiUiDispatcher dispatcher;
        std::vector<int> order;
        dispatcher.PostCoalesced(3, [&order] { order.push_back(1); });
        dispatcher.CancelCoalesced(3);
        assert(!dispatcher.HasPending() && dispatcher.Drain() == 0);
        // 取消后同一 key 可以重新投递
        dispatcher.PostCoalesced(3, [&order] { order.push_back(2); });
        assert(dispatcher.Drain() == 1);
        assert((order == std::vector<int>{2}));
        // 取消不存在的 key 是安全的
        dispatcher.CancelCoalesced(999);
        assert(!dispatcher.HasPending());
    }

    // Drain 期间新投递的任务留到下一次，避免本次执行中无限追加
    {
        DuiUiDispatcher dispatcher;
        int executed{};
        dispatcher.Post([&dispatcher, &executed] {
            ++executed;
            dispatcher.Post([&executed] { ++executed; });
        });
        assert(dispatcher.Drain() == 1);
        assert(executed == 1);
        assert(dispatcher.HasPending());
        assert(dispatcher.Drain() == 1);
        assert(executed == 2);
    }

    // Clear：丢弃全部待处理任务
    {
        DuiUiDispatcher dispatcher;
        int executed{};
        dispatcher.Post([&executed] { ++executed; });
        dispatcher.PostCoalesced(1, [&executed] { ++executed; });
        assert(dispatcher.Clear() == 2);
        assert(!dispatcher.HasPending() && dispatcher.Drain() == 0 && executed == 0);
        assert(dispatcher.Clear() == 0);
    }

    // 任务内部销毁了内容并 Clear()：本次 Drain 立即停止，剩余任务不得执行
    {
        DuiUiDispatcher dispatcher;
        int executed{};
        dispatcher.Post([&dispatcher, &executed] {
            ++executed;
            dispatcher.Clear();
        });
        dispatcher.Post([&executed] { ++executed; });
        dispatcher.PostCoalesced(1, [&executed] { ++executed; });
        assert(dispatcher.Drain() == 1);
        assert(executed == 1);
        assert(!dispatcher.HasPending());
    }

    // 唤醒回调：仅在"由空转非空"时触发一次
    {
        DuiUiDispatcher dispatcher;
        int wakes{};
        dispatcher.SetPendingHandler([&wakes] { ++wakes; });
        dispatcher.Post([] {});
        assert(wakes == 1);
        // 队列已非空：不再重复唤醒
        dispatcher.Post([] {});
        dispatcher.PostCoalesced(1, [] {});
        assert(wakes == 1);
        // 清空后再次投递：重新唤醒
        dispatcher.Drain();
        dispatcher.Post([] {});
        assert(wakes == 2);
        // 取消唤醒回调
        dispatcher.Clear();
        dispatcher.SetPendingHandler({});
        dispatcher.Post([] {});
        assert(wakes == 2);
    }

    // 跨线程投递：多线程并发 Post 后一次性执行，任务不丢失、不重复
    {
        DuiUiDispatcher dispatcher;
        std::atomic<int> counter{};
        constexpr int kThreads = 4;
        constexpr int kPerThread = 250;
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        for (int index = 0; index < kThreads; ++index)
        {
            workers.emplace_back([&dispatcher, &counter] {
                for (int task = 0; task < kPerThread; ++task)
                    dispatcher.Post([&counter] { counter.fetch_add(1); });
            });
        }
        for (std::thread& worker : workers)
            worker.join();
        assert(dispatcher.PendingCount() == static_cast<std::size_t>(kThreads * kPerThread));
        assert(dispatcher.Drain() == static_cast<std::size_t>(kThreads * kPerThread));
        assert(counter.load() == kThreads * kPerThread);
        assert(!dispatcher.HasPending());
    }

    // Host 集成：PrepareFrame 先执行投递任务，再按其改动排版（同帧完成）
    {
        ysDui::core::Host host;
        auto root = std::make_unique<LayoutRecordingControl>();
        auto* rootPointer = root.get();
        host.SetRoot(std::move(root));
        host.PrepareFrame();
        // SetRoot 标脏，首帧已消费掉这次布局
        assert(rootPointer->layoutCount == 1);

        auto pending = std::make_unique<ysDui::core::Control>();
        ysDui::core::Control* pendingRaw = pending.get();
        host.Dispatcher().Post([rootPointer, &pending] {
            // AddChild 会使根标脏；若 Drain 排在布局之后，本帧就看不到这次排版
            rootPointer->AddChild(std::move(pending));
        });
        assert(rootPointer->Children().empty());
        host.PrepareFrame();
        assert(rootPointer->Children().size() == 1);
        assert(pendingRaw->Parent() == rootPointer);
        // 关键不变量：同一次 PrepareFrame 内既执行了任务又完成了排版
        assert(rootPointer->layoutCount == 2);
        assert(host.Dispatcher().Drain() == 0);

        // 窗口关闭路径：解除唤醒回调并丢弃挂起任务
        int executed{};
        host.Dispatcher().Post([&executed] { ++executed; });
        assert(host.Dispatcher().Clear() == 1);
        host.PrepareFrame();
        assert(executed == 0);
    }
}
} // namespace

int main()
{
    VerifyTextInputBinding<ysDui::controls::input::DuiComboBox>();
    VerifyTextInputBinding<ysDui::controls::input::DuiDoubleSpinBox>();
    VerifyTextInputBinding<ysDui::controls::input::DuiEditHost>();
    VerifyTextInputBinding<ysDui::controls::input::DuiPathEdit>();
    VerifyTextInputBinding<ysDui::controls::input::DuiSearchBox>();
    VerifyTextInputBinding<ysDui::controls::input::DuiSpinBox>();
    VerifyTextInputBinding<ysDui::controls::list::DuiDataGrid>();
    VerifyTextInputBinding<ysDui::controls::list::DuiPropertyGrid>();
    VerifyRichTextInputBinding();

    VerifyPopupHostDestroyedFirst<ysDui::controls::input::DuiComboBox>();
    VerifyPopupHostDestroyedFirst<ysDui::controls::input::DuiDateTimePicker>();
    VerifyPopupHostDestroyedFirst<ysDui::controls::input::DuiColorPicker>();
    VerifyPopupCallbacksAfterOwnerDestruction();
    VerifyControlRemovalDuringDispatch();
    VerifyAsyncLoaderDestruction();
    VerifySubscriptionPublisherDestruction();
    VerifyUiDispatcher();
}
