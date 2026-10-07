# 第三方依赖治理

## 当前依赖

| 依赖 | 版本或固定来源 | 获取方式 | 许可证 | 使用范围 |
|---|---|---|---|---|
| pugixml | 1.16 | `third_party/pugixml` | MIT，见 `third_party/pugixml/LICENSE.md` | core、render、controls 的私有 XML 解析 |
| NanoSVG | BalloonUI `d1ab3f3` 快照 | `third_party/nanosvg` | zlib，见 `third_party/nanosvg/LICENSE.txt` | render 私有 SVG 光栅化 |
| MD4C | `release-0.5.2` | `third_party/md4c` | MIT，见 `third_party/md4c/LICENSE.md` | controls 私有 Markdown 解析（`DuiMarkdownView`） |
| zlib-ng | `425439062b114a0f6cf625022c41d929c7e879f9` | CMake FetchContent | zlib | 可选 resource 模块 |
| minizip-ng | `7b2387161c542fa9f427352dcdef76097d0d692b` | CMake FetchContent | zlib | 可选 resource 模块 |
| libsodium | 由消费者提供的 CMake package | vcpkg 或系统包 | ISC | 可选 resource 加密 |
| Font Awesome Free | 6.7.2，`examples/DuiGallery/fonts/fa-solid-900.ttf` | Gallery 外部运行时文件 | SIL OFL 1.1（字体），CC BY 4.0（图标） | Gallery 图标，不属于 SDK |

Vendored 文件通过以下 SHA-256 识别，更新时必须同时更新版本、许可证和哈希：

```text
third_party/nanosvg/nanosvg.h|91AFC79E91D84F3F6DE7662382DD3D8383869CC0152576F1B4B5F14F70A9C98B
third_party/nanosvg/nanosvgrast.h|93EFB49F5166259A466AA87124E458899EB01A7F5EFF099ED41382A9D0E88D64
third_party/pugixml/pugixml.hpp|C66F1BE9BDD4F1265629D699BBBCC4BA60EDA2FE45D04D0493F2B5ED22E3F626
third_party/pugixml/pugixml.cpp|04CDC6BDE588039E7E3F2AF195A6CDAD33303B2DB39568067FA5CE55E0B723C9
third_party/md4c/md4c.h|4EFD19BF7EC270691D5B4189F496886E421768A814B5E817EB945AA85E859F18
third_party/md4c/md4c.c|52E39843B96B64D0281BF795128BCC88C9C4DC52D7436737E9F4306648DC4590
third_party/md4c/LICENSE.md|D30937367D5413E7EAA218B1640B8946FF76FD34D97152F6979FD96169D5D0FC
```

## 更新流程

1. 只从依赖的官方仓库或正式发布包更新，并固定版本或完整提交哈希。
2. 检查许可证是否变化，将许可证文件与源码一并更新。
3. 更新上表和 vendored 文件 SHA-256，不允许只替换源码。
4. 运行严格编译、Clang-Tidy、ASan/UBSan、公开头审计、resource 测试、加密测试和安装消费者。
5. 在 `CHANGELOG.md` 记录依赖升级及可能影响的文件格式或行为。

## 发布限制

ysDui 仓库当前没有声明项目自身的许可证。在所有者明确选择开源或商业许可证并加入根目录 `LICENSE` 前，不应公开分发发布产物；第三方许可证不能替代项目许可证。

Font Awesome 的字体文件只随 Gallery 源码保留并按其 `LICENSE.txt` 条款部署；SDK 安装包不会复制该字体。
