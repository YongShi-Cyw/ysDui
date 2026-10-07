/**
 * 文件名：chat_stream_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：验证"后台线程流式产出 → UI 线程派发 → 变高消息列表"这条完整链路。
 *
 * 这是 AI 对话的核心数据通路：生产者在线程侧高频产出增量，消费侧每帧最多应用一次，
 * 且应用后同帧完成测量与排版。测试用真实线程与真实 Host，而不是模拟派发。
 *
 * 同步方式：生产者与 UI 线程**双向握手**（produced / consumed），所有断言都不依赖墙钟。
 * 若只做单向等待，"生产者还在投递、UI 线程已排空队列"会让断言随机失败。
 */
#include "test_support.hpp"

#include "ysDui/controls/chat/DuiChatBubble.hpp"
#include "ysDui/controls/chat/DuiChatList.hpp"
#include "ysDui/controls/content/DuiMarkdownView.hpp"

namespace {

using ysDui::core::DuiUiDispatcher;
using ysDui::core::Host;
using ysDui::controls::chat::DuiChatBubble;
using ysDui::controls::chat::DuiChatList;
using ysDui::controls::chat::DuiChatRole;
using ysDui::controls::content::DuiMarkdownView;

/** 一轮里产出的增量块数：刻意远多于执行次数，用来验证合并。 */
constexpr int kBlocksPerRound = 40;
/** 总相数：两轮增量 + 一轮收尾。 */
constexpr int kRoundCount = 3;
/** 列表视口高度：远小于最终内容，确保出现滚动与贴底行为。 */
constexpr int kListHeight = 240;
/** 合并键：一条消息一个键。 */
constexpr DuiUiDispatcher::CoalesceKey kStreamKey = 1;
/** 等待对端相位推进的上限，防止缺陷导致测试挂死（只用于退出，不参与断言）。 */
constexpr int kWaitTimeoutMilliseconds = 20000;

/** 构造第 index 块增量文本；finished 为真时追加结束标记。 */
std::string BuildTextUpTo(int index, bool finished)
{
    std::string text;
    for (int chunk = 0; chunk <= index; ++chunk)
        text += "第 " + std::to_string(chunk) + " 块 **增量** 内容，用于撑高这一条消息。\n\n";
    if (finished)
        text += "以上为完整回答。";
    return text;
}

/** 有界等待：只用于避免挂死，条件本身在正常路径下必然成立。 */
bool WaitFor(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(kWaitTimeoutMilliseconds);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return condition();
}

/** 一条流式消息的场景：列表 + 气泡 + Markdown 正文。 */
struct StreamScene final
{
    DuiChatList* list{};
    DuiMarkdownView* view{};
    int initialHeight{};
};

StreamScene BuildStreamScene(Host& host)
{
    StreamScene scene;
    auto list = std::make_unique<DuiChatList>();
    list->SetFollowBottom(true);
    list->SetOverscan(0);
    list->SetEstimatedItemHeight(80);
    scene.list = list.get();

    auto bubble = std::make_unique<DuiChatBubble>();
    bubble->SetRole(DuiChatRole::Assistant);
    bubble->SetName("AI");
    bubble->SetMaxWidth(360);
    auto view = std::make_unique<DuiMarkdownView>();
    scene.view = view.get();
    view->SetStreaming(true);
    bubble->SetContent(std::move(view));
    list->AppendItem(std::move(bubble));

    host.SetRoot(std::move(list));
    scene.list->Layout({0, 0, 420, kListHeight});
    scene.initialHeight = scene.list->ContentHeight();
    return scene;
}

/**
 * 流式主链路：分三相推进，每相"生产者投递完 → UI 线程只 Drain 一次"。
 * 因此可以精确断言每相只应用一次，从而证明合并生效（40 次投递 → 1 次执行）。
 */
void VerifyStreamingIntoChatList()
{
    Host host;
    const StreamScene scene = BuildStreamScene(host);
    assert(scene.list->FollowBottom() && scene.list->AtBottom());
    // 初始为空内容：气泡只有内边距，高度很小
    assert(scene.initialHeight < kListHeight);
    assert(scene.list->MeasuredItemCount() == 1);

    // 闭包真正执行时 +1：用它证明"投递次数 ≫ 执行次数"
    int applied{};
    std::atomic<int> produced{};   // 生产者已投递完的相位数
    std::atomic<int> consumed{};   // UI 线程已消费的相位数
    std::thread producer([&host, list = scene.list, view = scene.view, &applied, &produced, &consumed]
    {
        for (int round = 0; round < kRoundCount - 1; ++round)
        {
            const int first = round * kBlocksPerRound;
            for (int chunk = first; chunk < first + kBlocksPerRound; ++chunk)
            {
                // 生产者只产出文本快照，绝不触碰控件；控件访问全部发生在 UI 线程的闭包内
                const std::string snapshot = BuildTextUpTo(chunk, false);
                host.Dispatcher().PostCoalesced(kStreamKey, [list, view, snapshot, &applied]
                {
                    ++applied;
                    view->SetContent(snapshot);
                    list->InvalidateItem(0);
                });
            }
            produced.store(round + 1);
            // 等 UI 线程把这一相消费掉再继续，否则两相会被合并成一相
            (void)WaitFor([&consumed, expected = round + 1] { return consumed.load() >= expected; });
        }
        // 收尾相：最终全文走**同一个合并键**，于是天然只有最后一次生效
        const std::string complete = BuildTextUpTo(kBlocksPerRound * (kRoundCount - 1) - 1, true);
        host.Dispatcher().PostCoalesced(kStreamKey, [list, view, complete, &applied]
        {
            ++applied;
            view->SetContent(complete);
            list->InvalidateItem(0);
            view->SetStreaming(false);
        });
        produced.store(kRoundCount);
    });

    const auto consumeRound = [&host, &applied, &produced, &consumed](int round)
    {
        assert(WaitFor([&produced, round] { return produced.load() >= round; }));
        const int before = applied;
        // 一次 PrepareFrame 先 Drain 再排版，因此"应用增量"与"测量排版"必然同帧
        host.PrepareFrame();
        // 关键断言：本相投递了 40 次增量，却只执行 1 次——中间值被覆盖而非排队
        assert(applied == before + 1);
        assert(!host.Dispatcher().HasPending());
        consumed.store(round);
    };

    // 第 1 相：40 次投递 → 只执行 1 次
    consumeRound(1);
    assert(applied == 1);
    assert(scene.view->Content() == BuildTextUpTo(kBlocksPerRound - 1, false));
    assert(scene.list->FollowBottom() && scene.list->AtBottom());
    const int firstRoundHeight = scene.list->ContentHeight();
    assert(firstRoundHeight > scene.initialHeight);

    // 第 2 相：再 40 次投递 → 仍然只多执行 1 次，内容继续增长
    consumeRound(2);
    assert(applied == 2);
    assert(scene.view->Content() == BuildTextUpTo(2 * kBlocksPerRound - 1, false));
    assert(scene.list->FollowBottom() && scene.list->AtBottom());
    assert(scene.list->ContentHeight() > firstRoundHeight);

    // 第 3 相：最终全文 → 再执行 1 次，内容恰好是完整文本（未被较早增量覆盖）
    consumeRound(kRoundCount);
    producer.join();
    assert(applied == 3);
    assert(scene.view->Content() == BuildTextUpTo(2 * kBlocksPerRound - 1, true));
    assert(!scene.view->Streaming());

    // 贴底的表现：内容远超视口，而末尾条目的底边正好贴在视口底边
    assert(scene.list->ContentHeight() > kListHeight);
    assert(scene.list->FollowBottom() && scene.list->AtBottom());
    assert(scene.list->MeasuredItemCount() == 1);
    assert(scene.list->FirstVisibleIndex() == 0 && scene.list->LastVisibleIndex() == 0);
    assert(scene.list->ItemRect(0).bottom == kListHeight);

    // 销毁路径：挂起任务必须被丢弃，不能去碰已经释放的控件
    int lateRuns{};
    host.Dispatcher().Post([&lateRuns] { ++lateRuns; });
    assert(host.Dispatcher().Clear() == 1);
    host.SetRoot(nullptr);
    host.PrepareFrame();
    assert(lateRuns == 0);
}

/** 用户中途上滚：贴底跟随自动关闭，后续增量不得把视图拉回末尾。 */
void VerifyUserScrollReleasesFollow()
{
    Host host;
    const StreamScene scene = BuildStreamScene(host);
    host.Dispatcher().Post([list = scene.list, view = scene.view]
    {
        view->SetContent(BuildTextUpTo(2 * kBlocksPerRound - 1, true));
        list->InvalidateItem(0);
    });
    host.PrepareFrame();
    assert(scene.list->AtBottom());

    // 上滚离开末尾：跟随关闭，位置被钉住
    assert(scene.list->OnEvent(ysDui::test::MakeEvent(
        ysDui::core::EventType::PointerWheel, {10, 10}, 0, 0, 0, 120)));
    assert(!scene.list->FollowBottom() && !scene.list->AtBottom());
    const int parked = scene.list->ScrollOffset();

    // 再有增量时视图不再移动
    host.Dispatcher().Post([list = scene.list, view = scene.view]
    {
        view->SetContent(BuildTextUpTo(2 * kBlocksPerRound - 1, true) + "\n\n追加一段。");
        list->InvalidateItem(0);
    });
    host.PrepareFrame();
    assert(scene.list->ScrollOffset() == parked);
    assert(!scene.list->AtBottom());

    // 点回末尾：跟随重新开启
    scene.list->ScrollToBottom();
    assert(scene.list->FollowBottom() && scene.list->AtBottom());
}

} // namespace

int main()
{
    VerifyStreamingIntoChatList();
    VerifyUserScrollReleasesFollow();
    return 0;
}
