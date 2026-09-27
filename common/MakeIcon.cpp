// make_icon <out.bmp>
//
// Draws the examples' icon, a blue rounded square with a white ring and a gold
// bar, and writes it as a 256-pixel, 32-bit BMP with alpha. It runs during the
// build, so opane_set_app_icon has an icon without the repository storing one.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{

constexpr int Size = 256;

// How much of a pixel's area a shape covers, from sixteen samples in it.
template <typename Inside>
float Coverage(int x, int y, Inside inside)
{
    int hits = 0;
    for (int sy = 0; sy < 4; ++sy)
    {
        for (int sx = 0; sx < 4; ++sx)
        {
            hits += inside(x + (sx + 0.5f) / 4.0f, y + (sy + 0.5f) / 4.0f) ? 1 : 0;
        }
    }
    return hits / 16.0f;
}

void Put32(std::vector<uint8_t>& out, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
    }
}

void Put16(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: make_icon <out.bmp>\n");
        return 2;
    }

    const float center = Size / 2.0f;
    auto InSquare = [&](float px, float py) {
        const float half = Size / 2.0f - 8.0f;
        const float radius = 56.0f;
        const float qx = std::max(std::fabs(px - center) - (half - radius), 0.0f);
        const float qy = std::max(std::fabs(py - center) - (half - radius), 0.0f);
        return qx * qx + qy * qy <= radius * radius;
    };
    auto InRing = [&](float px, float py) {
        const float distance = std::hypot(px - center, py - center);
        return distance >= 32.0f && distance <= 60.0f;
    };
    auto InBar = [&](float px, float py) { return std::fabs(px - center) <= 14.0f && std::fabs(py - center) <= 80.0f; };

    // Rows bottom to top, as BMP stores them; BGRA.
    std::vector<uint8_t> pixels(static_cast<size_t>(Size) * Size * 4);
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            const float t = (x + y) / (2.0f * Size);
            float r = 40 + 30 * t, g = 90 + 80 * t, b = 200 + 40 * t;
            const float square = Coverage(x, y, InSquare);
            const float ring = Coverage(x, y, InRing);
            const float bar = Coverage(x, y, InBar);
            r = r + (255 - r) * ring;
            g = g + (255 - g) * ring;
            b = b + (255 - b) * ring;
            r = r + (255 - r) * bar;
            g = g + (196 - g) * bar;
            b = b + (64 - b) * bar;

            uint8_t* out = &pixels[(static_cast<size_t>(Size - 1 - y) * Size + x) * 4];
            out[0] = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
            out[1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
            out[2] = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
            out[3] = static_cast<uint8_t>(std::clamp(square * 255.0f, 0.0f, 255.0f));
        }
    }

    std::vector<uint8_t> file;
    file.push_back('B');
    file.push_back('M');
    Put32(file, static_cast<uint32_t>(14 + 40 + pixels.size()));
    Put32(file, 0);
    Put32(file, 14 + 40);
    Put32(file, 40); // BITMAPINFOHEADER
    Put32(file, Size);
    Put32(file, Size);
    Put16(file, 1);
    Put16(file, 32);
    Put32(file, 0); // uncompressed
    Put32(file, static_cast<uint32_t>(pixels.size()));
    Put32(file, 2835);
    Put32(file, 2835);
    Put32(file, 0);
    Put32(file, 0);
    file.insert(file.end(), pixels.begin(), pixels.end());

    FILE* out = std::fopen(argv[1], "wb");
    if (out == nullptr)
    {
        std::fprintf(stderr, "make_icon: could not write %s\n", argv[1]);
        return 1;
    }
    const bool written = std::fwrite(file.data(), 1, file.size(), out) == file.size();
    std::fclose(out);
    return written ? 0 : 1;
}
