/**
 * 文件名：DuiIconFont.hpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：声明平台无关的图标字体加载、测量和绘制服务。
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ysDui/core/DuiGeometry.hpp"
#include "ysDui/render/DuiFontRegistry.hpp"
#include "ysDui/render/DuiCanvas.hpp"

namespace ysDui::render {

/**
 * 图标字体服务。
 * 用途：通过字体注册表加载图标字体，并将字形转换为标准文本绘制请求。
 */
class DuiIconFont final {
public:
    /**
     * 创建图标字体服务。
     * @param registry 负责注册和查询字体族的平台能力。
     */
    explicit DuiIconFont(render::DuiFontRegistry& registry);

    /** 释放图标字体服务持有的实现状态。 */
    ~DuiIconFont();

    DuiIconFont(const DuiIconFont&) = delete;
    DuiIconFont& operator=(const DuiIconFont&) = delete;
    DuiIconFont(DuiIconFont&&) noexcept;
    DuiIconFont& operator=(DuiIconFont&&) noexcept;

    /**
     * 从文件注册图标字体。
     * @param path 字体文件的 UTF-16 路径。
     * @param family 注册后应使用的字体族；空值时按内置图标字体族顺序查找。
     * @return 字体注册并解析出字体族时返回 true。
     */
    bool LoadFromFile(std::string path, std::string_view family = {});

    /**
     * 从内存注册图标字体。
     * @param bytes 字体文件的完整二进制内容。
     * @param family 注册后应使用的字体族；空值时按内置图标字体族顺序查找。
     * @return 字体注册并解析出字体族时返回 true。
     */
    bool LoadFromMemory(std::vector<std::uint8_t> bytes, std::string_view family = {});

    /**
     * 设置已注册的图标字体族。
     * @param family 要使用的字体族名称。
     * @return 字体族可用时返回 true。
     */
    bool SetFamily(std::string family);

    /** @return 当前已解析的字体族名称。 */
    [[nodiscard]] std::string_view Family() const;

    /** @return 已解析出可用字体族时返回 true。 */
    [[nodiscard]] bool IsReady() const;

    /**
     * 创建图标字形使用的文本样式。
     * @param pointSize 字号。
     * @param color 字形颜色。
     * @param bold 是否使用粗体。
     * @return 可直接传给画布的文本样式。
     */
    [[nodiscard]] DuiTextStyle TextStyle(int pointSize, core::Color color, bool bold = true) const;

    /**
     * 测量图标字形。
     * @param canvas 提供文本测量能力的画布。
     * @param glyph UTF-16 图标字形。
     * @param style 图标文本样式。
     * @return 字形的尺寸、行数和基线；未就绪时返回空结果。
     */
    [[nodiscard]] DuiTextMetrics Measure(Canvas& canvas, std::string_view glyph,
                                         const DuiTextStyle& style) const;

    /**
     * 绘制图标字形。
     * @param canvas 接收标准文本绘制命令的画布。
     * @param bounds 字形绘制区域。
     * @param glyph UTF-16 图标字形。
     * @param style 图标文本样式。
     * @param alignment 水平对齐方式。
     */
    void Draw(Canvas& canvas, core::Rect bounds, std::string_view glyph,
              const DuiTextStyle& style,
              DuiTextAlignment alignment = DuiTextAlignment::Center) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ysDui::render
