#include "ysDui/controls/list/DuiTabPage.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "ysDui/render/DuiPaintChildren.hpp"

namespace ysDui::controls::list {
namespace {
const std::shared_ptr<const render::DuiImage> EmptyImage;
constexpr int MinimumHeaderHeight = 12;
}

class DuiTabPage::Impl {
public:
    DuiTab* header{};
    std::vector<core::Control*> pages;
    std::function<void(int)> selectionChanged;
    int selectedIndex{-1};
    int headerHeight{32};
};

DuiTabPage::DuiTabPage() : tabPage_(std::make_unique<Impl>()) {
    auto header = std::make_unique<DuiTab>();
    tabPage_->header = header.get();
    header->SetSelectionChangedHandler([this](int index) { SetSelectedIndex(index); });
    AddChild(std::move(header));
}

DuiTabPage::~DuiTabPage() = default;

void DuiTabPage::SetHeaderHeight(int pixels) {
    tabPage_->headerHeight = (std::max)(MinimumHeaderHeight, pixels);
    Layout(Bounds());
}

int DuiTabPage::HeaderHeight() const { return tabPage_->headerHeight; }
void DuiTabPage::SetIconSize(int pixels) { tabPage_->header->SetIconSize(pixels); }
void DuiTabPage::SetIconGap(int pixels) { tabPage_->header->SetIconGap(pixels); }
void DuiTabPage::SetAutoFitTabWidth(bool enabled) { tabPage_->header->SetAutoFitTabWidth(enabled); }

int DuiTabPage::AddPage(std::string title, std::unique_ptr<core::Control> page,
                        std::shared_ptr<const render::DuiImage> icon) {
    const int index = tabPage_->header->AddTab(std::move(title), false, false, 0, std::move(icon));
    core::Control* raw = page.get();
    if (page) AddChild(std::move(page));
    tabPage_->pages.push_back(raw);
    if (tabPage_->selectedIndex < 0) {
        tabPage_->selectedIndex = 0;
        tabPage_->header->SetSelectedIndex(0, false);
    }
    ApplyVisibility();
    LayoutContent();
    return index;
}

void DuiTabPage::RemovePage(int index) {
    (void)ReleasePage(index);
}

std::unique_ptr<core::Control> DuiTabPage::ReleasePage(int index) {
    if (index < 0 || index >= PageCount()) return {};
    std::unique_ptr<core::Control> released;
    if (core::Control* page = tabPage_->pages[index]) released = RemoveChild(page);
    tabPage_->pages.erase(tabPage_->pages.begin() + index);
    tabPage_->header->RemoveTab(index);
    if (tabPage_->pages.empty()) tabPage_->selectedIndex = -1;
    else tabPage_->selectedIndex = (std::clamp)(tabPage_->selectedIndex, 0, PageCount() - 1);
    tabPage_->header->SetSelectedIndex(tabPage_->selectedIndex, false);
    ApplyVisibility();
    LayoutContent();
    return released;
}

void DuiTabPage::SetPage(int index, std::unique_ptr<core::Control> page) {
    if (index < 0 || index >= PageCount()) return;
    if (core::Control* previous = tabPage_->pages[index]) (void)RemoveChild(previous);
    tabPage_->pages[index] = page.get();
    if (page) AddChild(std::move(page));
    ApplyVisibility();
    LayoutContent();
}

core::Control* DuiTabPage::PageAt(int index) const {
    return index >= 0 && index < PageCount() ? tabPage_->pages[index] : nullptr;
}

int DuiTabPage::PageCount() const { return static_cast<int>(tabPage_->pages.size()); }
void DuiTabPage::SetPageTitle(int index, std::string title) { tabPage_->header->SetTextAt(index, std::move(title)); }
std::string DuiTabPage::PageTitle(int index) const { return tabPage_->header->TextAt(index); }
void DuiTabPage::SetPageIcon(int index, std::shared_ptr<const render::DuiImage> icon) { tabPage_->header->SetIcon(index, std::move(icon)); }
const std::shared_ptr<const render::DuiImage>& DuiTabPage::PageIcon(int index) const { return index >= 0 && index < PageCount() ? tabPage_->header->IconAt(index) : EmptyImage; }

void DuiTabPage::SetSelectedIndex(int index, bool notify) {
    if (index < 0 || index >= PageCount()) return;
    const bool changed = tabPage_->selectedIndex != index;
    tabPage_->selectedIndex = index;
    tabPage_->header->SetSelectedIndex(index, false);
    ApplyVisibility();
    LayoutContent();
    if (changed && notify && tabPage_->selectionChanged) tabPage_->selectionChanged(index);
}

int DuiTabPage::SelectedIndex() const { return tabPage_->selectedIndex; }
void DuiTabPage::SetSelectionChangedHandler(std::function<void(int)> handler) { tabPage_->selectionChanged = std::move(handler); }
DuiTab& DuiTabPage::Header() { return *tabPage_->header; }
const DuiTab& DuiTabPage::Header() const { return *tabPage_->header; }

core::Rect DuiTabPage::HeaderRect() const {
    return {Bounds().left, Bounds().top, Bounds().right, (std::min)(Bounds().bottom, Bounds().top + HeaderHeight())};
}

core::Rect DuiTabPage::ContentRect() const {
    const core::Rect header = HeaderRect();
    return {Bounds().left, header.bottom, Bounds().right, Bounds().bottom};
}

void DuiTabPage::Layout(core::Rect bounds) {
    SetBounds(bounds);
    tabPage_->header->Layout(HeaderRect());
    LayoutContent();
}

void DuiTabPage::ApplyVisibility() {
    for (int index = 0; index < PageCount(); ++index) {
        if (core::Control* page = tabPage_->pages[index]) page->SetVisible(index == tabPage_->selectedIndex);
    }
}

void DuiTabPage::LayoutContent() {
    if (core::Control* page = PageAt(tabPage_->selectedIndex)) page->SetBounds(ContentRect());
}

void DuiTabPage::Paint(render::Canvas& canvas, core::Rect dirty) const {
    if (EffectivelyVisible()) render::PaintChildren(*this, canvas, dirty);
}

} // namespace ysDui::controls::list
