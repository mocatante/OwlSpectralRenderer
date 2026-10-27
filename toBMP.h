#pragma once
#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>

// 将 RGBA 数组保存为 BMP 文件
inline void saveBMP(const char* filename, const uint32_t* pixels, int width, int height) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return;

    const int rowBytes = width * 3;
    const int padding = (4 - (rowBytes % 4)) % 4;
    const int stride = rowBytes + padding;
    const uint32_t imgSize = static_cast<uint32_t>(stride) * static_cast<uint32_t>(height);
    const uint32_t fileSize = 14 + 40 + imgSize;

    auto writeU16 = [&](uint16_t v) { out.put(v & 0xFF); out.put((v >> 8) & 0xFF); };
    auto writeU32 = [&](uint32_t v) {
        out.put(v & 0xFF); out.put((v >> 8) & 0xFF);
        out.put((v >> 16) & 0xFF); out.put((v >> 24) & 0xFF);
    };

    out.put('B'); out.put('M');
    writeU32(fileSize);
    writeU16(0); writeU16(0);
    writeU32(14 + 40);

    writeU32(40);
    writeU32(width);
    writeU32(height);
    writeU16(1);
    writeU16(24);
    writeU32(0);
    writeU32(imgSize);
    writeU32(2835); writeU32(2835);
    writeU32(0); writeU32(0);

    const uint8_t pad[3] = { 0, 0, 0 };
    for (int j = 0; j <height; ++j) {
        for (int i = 0; i < width; ++i) {
            uint32_t color = pixels[j * width + i];
            uint8_t r = (color >> 0) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = (color >> 16) & 0xFF;

            out.put(b); out.put(g); out.put(r);
        }
        out.write(reinterpret_cast<const char*>(pad), padding);
    }
}