/**
 * 文件名：DuiWorkbookSheetOperations.hpp
 * 开发者：青蓝
 * 开发时间：2026-08-16
 * 用途：声明工作簿模型可选的工作表结构操作能力。
 */
#pragma once

#include <string>

#include "ysDui/controls/list/DuiWorkbookModel.hpp"

namespace ysDui::controls::list {

/** \brief 由工作簿模型按需实现的工作表插入与复制能力。 */
class IDuiWorkbookSheetOperations
{
public:
    virtual ~IDuiWorkbookSheetOperations() = default;

    /**
     * \brief 在指定工作表之前插入工作表。
     * \param before 插入位置对应的现有工作表。
     * \param name 新工作表的 UTF-8 名称。
     * \return 新工作表标识；操作失败时返回 0。
     */
    [[nodiscard]] virtual DuiWorksheetId InsertWorksheetBefore(
        DuiWorksheetId before, std::string name) = 0;

    /**
     * \brief 复制指定工作表，具体数据复制策略由模型负责。
     * \param source 被复制的工作表。
     * \param name 副本的 UTF-8 名称。
     * \return 副本工作表标识；操作失败时返回 0。
     */
    [[nodiscard]] virtual DuiWorksheetId DuplicateWorksheet(
        DuiWorksheetId source, std::string name) = 0;
};

} // namespace ysDui::controls::list
