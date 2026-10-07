/**
 * 文件名：win32_golden_codec_tests.cpp
 * 开发者：青蓝
 * 开发时间：2026-07-26
 * 用途：验证 Win32 基准图像 PNG 编解码往返。
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#include "ysDui/platform/win32/DuiImageDecoder.hpp"
#include "ysDui/platform/win32/DuiWin32GoldenCodec.hpp"
#include "ysDui/render/DuiGolden.hpp"

int main()
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "ysdui_golden_codec_test.png";
    std::filesystem::remove(path);
    const auto source = ysDui::render::DuiPixelBuffer::Create({2, 1}, {
        0, 0, 255, 255,
        0, 255, 0, 255,
    });
    assert(ysDui::platform::win32::DuiWin32GoldenCodec::SavePng(source, path.string()));
    const auto loaded = ysDui::platform::win32::DuiWin32GoldenCodec::LoadPng(path.string());
    assert(loaded.has_value());
    const auto result = ysDui::render::CompareGolden(*loaded, source, 0);
    assert(!result.sizeMismatch && result.differingPixels == 0);
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    assert(input);
    const std::streamsize size = input.tellg();
    assert(size > 0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    assert(input.read(reinterpret_cast<char*>(bytes.data()), size));
    {
        const auto decoded = ysDui::platform::win32::DuiImageDecoder::DecodeBytes(bytes);
        assert(decoded && decoded->Size() == source.Size());
        const auto shared = ysDui::platform::win32::DuiImageDecoder::LoadSharedBytes(bytes);
        assert(shared && !shared->Empty() && shared->Size() == source.Size());
        const auto sharedFile = ysDui::platform::win32::DuiImageDecoder::LoadSharedFile(path.string());
        assert(sharedFile && !sharedFile->Empty() && sharedFile->Size() == source.Size());
    }
    input.close();
    std::filesystem::remove(path);
}
