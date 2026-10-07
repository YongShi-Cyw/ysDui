/**
 * 文件名：DuiWatermark.hpp
 * 开发者：青蓝
 * 开发时间：2026-10-07
 * 用途：声明水印控件，按矩形或六边形排布平铺半透明的文字与图片内容。
 */
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ysDui/core/DuiControl.hpp"
#include "ysDui/render/DuiImage.hpp"
#include "ysDui/render/DuiRenderable.hpp"

namespace ysDui::controls::basic {

/** 水印平铺时的行错位方式。 */
enum class DuiWatermarkLayout
{
    Rectangular, // 矩形：各行竖直对齐，正交平铺。
    Hexagonal,   // 六边形：奇数行水平错位半个水平步长，错位平铺。
};

/**
 * 一条文字水印内容。
 * 水印块内可堆叠多条文字，再叠加一块图片（见 `DuiWatermark::SetImage`）。
 */
struct DuiWatermarkText final
{
    std::string text;                   // 水印文本（UTF-8）；空串表示这一条不绘制。
    core::Color fontColor{0, 0, 0, 25}; // 文字颜色，默认约 10% 黑（对齐参考实现 rgba(0,0,0,0.1)）。
    std::string fontFamily;             // 字体族；空串表示使用库默认字体。
    int pointSize{16};                  // 字号（点），对齐参考实现的 fontSize 默认值。
    bool bold{};                        // 是否加粗，对应参考实现 fontWeight 的 bold/bolder。
};

/** 一块图片水印内容。 */
struct DuiWatermarkImage final
{
    std::shared_ptr<const render::DuiImage> image; // 图片源；为空表示不绘制图片。
    bool isGrayScale{};                            // 是否按灰度绘制。
};

/**
 * 水印控件。
 *
 * 用途：在自身 Bounds 内周期性平铺半透明的文字或图片水印，常用于标识归属、防止截屏外泄。
 * 控件是叶子节点、不承载子控件；需要覆盖内容时，把水印作为目标容器的最后一个子控件加入
 * （子控件按插入顺序绘制，末位即画在最上层），并令其 Bounds 与容器一致。
 *
 * 旋转：`rotate != 0` 时把单块水印离屏栅格化，再经 `DuiImage::CreateRotatedClockwise` 旋转后平铺
 * （画布变换栈本身只有缩放+平移）。`rotate == 0` 时走矢量文字路径，更清晰。栅格化依赖
 * 当前 Canvas 的可选 `Rasterize` 能力；不可用时退回未旋转绘制。
 *
 * 与参考实现的差异（有意为之的裁剪，避免超出本控件的职责）：
 * - `content` / `default`（被覆盖的内容节点）、`zindex`（层级）由调用方的控件树与绘制顺序表达，
 *   不作为控件参数。
 * - `movable` / `moveInterval`（定时移动）需要动画时钟与产品侧的防泄漏策略，本期不实现。
 * - `removable`（用户可删除水印）属于产品交互策略，本控件只负责绘制，不实现删除入口。
 * - 参考实现的 `watermarkContent` 为数组时可在块内混排文字与图片；本控件限定为「文字行在上、图片在下」。
 */
class DuiWatermark final : public core::Control, public render::DuiRenderable
{
public:
    DuiWatermark();
    ~DuiWatermark() override;
    DuiWatermark(const DuiWatermark&) = delete;
    DuiWatermark& operator=(const DuiWatermark&) = delete;
    DuiWatermark(DuiWatermark&&) noexcept;
    DuiWatermark& operator=(DuiWatermark&&) noexcept;

    /**
     * 设置水印整体不透明度。
     * @param alpha 取值 [0,1]，越界按 0/1 截断；会与文字颜色、图片像素的原始透明度相乘。
     */
    void SetAlpha(double alpha);
    /** @return 当前整体不透明度 [0,1]。 */
    [[nodiscard]] double Alpha() const;

    /** 设置单条文字水印，等价于 `SetTexts` 传入只含该条目的数组。 */
    void SetText(DuiWatermarkText text);
    /** 设置文字水印条目列表，按顺序自上而下堆叠。 */
    void SetTexts(std::vector<DuiWatermarkText> texts);
    /** @return 当前文字水印条目列表。 */
    [[nodiscard]] const std::vector<DuiWatermarkText>& Texts() const;

    /** 设置图片水印内容；传入空的 image 表示移除图片。 */
    void SetImage(DuiWatermarkImage image);
    /** @return 当前图片水印内容。 */
    [[nodiscard]] const DuiWatermarkImage& Image() const;

    /** 设置平铺时的行错位方式（勿命名为 Layout，以免遮蔽 Control::Layout）。 */
    void SetTileLayout(DuiWatermarkLayout layout);
    /** @return 当前平铺行错位方式。 */
    [[nodiscard]] DuiWatermarkLayout TileLayout() const;

    /** 设置是否平铺；false 时只在偏移位置绘制单块水印。 */
    void SetRepeat(bool repeat);
    /** @return 是否平铺。 */
    [[nodiscard]] bool Repeat() const;

    /** 设置相邻水印块之间的水平与垂直间距（DIP）；分量负值按 0 处理。 */
    void SetGap(core::Size gap);
    /** @return 当前水印块间距。 */
    [[nodiscard]] core::Size Gap() const;

    /**
     * 设置平铺起点偏移（DIP）。
     * 未显式设置时取间距的一半（对齐参考实现 offset 默认 [gapX/2, gapY/2]）。
     */
    void SetOffset(core::Point offset);
    /** @return 当前平铺起点偏移，未显式设置时为间距的一半。 */
    [[nodiscard]] core::Point Offset() const;

    /**
     * 设置单块水印的内容尺寸（DIP）。
     * 分量 <= 0 表示按实测内容自适应；显式尺寸会把内容在块内居中。
     */
    void SetContentSize(core::Size size);
    /** @return 当前显式内容尺寸；分量为 0 表示自适应。 */
    [[nodiscard]] core::Size ContentSize() const;

    /** 设置水印块内文字行之间、文字与图片之间的行间距（DIP）；负值按 0 处理。 */
    void SetLineSpace(int pixels);
    /** @return 当前行间距。 */
    [[nodiscard]] int LineSpace() const;

    /**
     * 设置水印旋转角度（度，正值顺时针，与 CSS / TDesign 一致；默认 -22）。
     * @param degrees 旋转角
     */
    void SetRotate(int degrees);
    /** @return 当前旋转角度（度）。 */
    [[nodiscard]] int Rotate() const;

    void Paint(render::Canvas& canvas, core::Rect dirty) const override;

    /**
     * 水印是纯绘制覆盖层，必须让指针输入穿透到下层内容。
     * @return 恒为 nullptr。
     */
    [[nodiscard]] core::Control* HitTest(core::Point point) override;

private:
    class Impl;
    std::unique_ptr<Impl> watermark_;
};

} // namespace ysDui::controls::basic
