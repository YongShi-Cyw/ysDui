/**
 * 文件名：DuiDropTarget.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-29
 * 用途：声明平台无关的拖放载荷和接收能力。
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ysDui/render/DuiImage.hpp"

namespace ysDui::ui {

/** 文件拖放载荷，路径使用 UTF-8。 */
struct DuiDropFiles final
{
    std::vector<std::string> paths;
};

/** 图像拖放载荷，图像在平台后端完成解码。 */
struct DuiDropImage final
{
    std::shared_ptr<const render::DuiImage> image;
};

using DuiDropPayload = std::variant<DuiDropFiles, DuiDropImage>;

class DuiDropTarget
{
public:
    using Handler = std::function<void(const DuiDropPayload&)>;

    virtual ~DuiDropTarget() = default;
    /** 设置拖放完成处理器；空处理器表示忽略载荷。 */
    virtual void SetHandler(Handler handler) = 0;
    /** 启用或禁用拖放接收。 */
    virtual void SetEnabled(bool enabled) = 0;
};

} // namespace ysDui::ui
