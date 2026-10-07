/**
 * 文件名：DuiWin32Application.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明 Windows 后端的应用初始化和消息循环服务。
 */
#pragma once

#include <memory>

namespace ysDui::platform::win32 {

/** Windows 后端的应用生命周期服务。 */
class DuiWin32Application final
{
public:
    /** 创建未初始化的应用服务。 */
    DuiWin32Application();
    /** 终止初始化并释放后端资源。 */
    ~DuiWin32Application();
    DuiWin32Application(const DuiWin32Application&) = delete;
    DuiWin32Application& operator=(const DuiWin32Application&) = delete;
    DuiWin32Application(DuiWin32Application&&) noexcept;
    DuiWin32Application& operator=(DuiWin32Application&&) noexcept;

    /** 初始化窗口控件和线程级平台服务。 */
    [[nodiscard]] bool Initialize();
    /** 释放该服务持有的初始化状态。 */
    void Terminate();
    /** 运行当前线程的消息循环。 */
    [[nodiscard]] int Run();
    /** 请求消息循环以指定退出码结束。 */
    void Quit(int exitCode = 0);
    /** 返回是否已成功初始化。 */
    [[nodiscard]] bool Initialized() const;

private:
    class Impl;
    std::unique_ptr<Impl> application_;
};

} // namespace ysDui::platform::win32
