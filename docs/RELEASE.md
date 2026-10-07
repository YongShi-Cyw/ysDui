# ysDui 发布清单

## 发布前置条件

1. 根目录存在项目所有者选定的 `LICENSE`。项目许可证不能由第三方依赖许可证替代。
2. `CHANGELOG.md` 已记录本次版本变化，版本头、CMake 项目版本和 Git 标签一致。
3. 依赖版本、来源哈希和许可证已在 `docs/DEPENDENCIES.md` 中同步。

## 自动验证

推送 `v*` 标签会触发 Windows 静态包工作流。工作流必须完成：

- Release 配置构建和完整 CTest；
- 安装目录布局检查及独立 `find_package(ysDui)` 消费者构建；
- 静态库、公开头、CMake package、resource packer 和第三方许可证归档；
- ZIP 归档 SHA-256 输出。

缺少根目录 `LICENSE` 时，工作流在构建前失败，不会创建 GitHub Release。

## 交付边界

当前只交付静态库和源兼容的 2.x API，不承诺 DLL ABI。Font Awesome Free 字体只属于 Gallery 外部运行时资源，不进入 SDK 安装包。
