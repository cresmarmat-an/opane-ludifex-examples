// Example 17: surfaces with normal maps, metallic and roughness maps, emission,
// and occlusion.
//
// Every texture is generated at startup, along with a glTF model whose
// material uses all five glTF maps.
//
// The normal map is a field of raised bumps lit from above. Read correctly,
// their tops are lit and their undersides are dark. If the green channel were
// read the wrong way round, they would look like dents.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/Png.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

constexpr float Pi = 3.14159265358979f;

// A tangent-space normal map of round domes on a grid, from a height field.
std::vector<uint8_t> DomeNormals(int size, int cells)
{
    std::vector<float> height(static_cast<size_t>(size) * size);
    const float cell = static_cast<float>(size) / static_cast<float>(cells);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float u = std::fmod(static_cast<float>(x) + 0.5f, cell) / cell * 2.0f - 1.0f;
            const float v = std::fmod(static_cast<float>(y) + 0.5f, cell) / cell * 2.0f - 1.0f;
            const float r = std::sqrt(u * u + v * v);
            height[static_cast<size_t>(y) * size + x] = r < 0.8f ? std::sqrt(0.64f - r * r) : 0.0f;
        }
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    auto H = [&](int x, int y) {
        x = (x + size) % size;
        y = (y + size) % size;
        return height[static_cast<size_t>(y) * size + x];
    };
    const float strength = cell * 0.35f;
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            // Image rows run down; a normal map's green points up the image,
            // so the slope along y is taken bottom minus top.
            const float dx = (H(x + 1, y) - H(x - 1, y)) * strength;
            const float dy = (H(x, y - 1) - H(x, y + 1)) * strength;
            float nx = -dx, ny = -dy, nz = 1.0f;
            const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= length;
            ny /= length;
            nz /= length;
            uint8_t* out = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            out[0] = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255.0f + 0.5f);
            out[1] = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255.0f + 0.5f);
            out[2] = static_cast<uint8_t>((nz * 0.5f + 0.5f) * 255.0f + 0.5f);
            out[3] = 255;
        }
    }
    return pixels;
}

std::vector<uint8_t> Solid(int size, uint8_t r, uint8_t g, uint8_t b)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (size_t index = 0; index < pixels.size(); index += 4)
    {
        pixels[index] = r;
        pixels[index + 1] = g;
        pixels[index + 2] = b;
        pixels[index + 3] = 255;
    }
    return pixels;
}

// Vertical stripes: roughness and metalness alternating, in glTF's channels.
std::vector<uint8_t> MetalRoughStripes(int size)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const bool metal = (x * 6 / size) % 2 == 0;
            uint8_t* out = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            out[0] = 255;                             // unused by glTF here
            out[1] = metal ? 50 : 230;                // roughness
            out[2] = metal ? 255 : 0;                 // metalness
            out[3] = 255;
        }
    }
    return pixels;
}

// A band of light around the middle, for emission.
std::vector<uint8_t> GlowBand(int size)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y)
    {
        const bool lit = y > size * 45 / 100 && y < size * 55 / 100;
        for (int x = 0; x < size; ++x)
        {
            uint8_t* out = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            out[0] = lit ? 255 : 0;
            out[1] = lit ? 140 : 0;
            out[2] = lit ? 40 : 0;
            out[3] = 255;
        }
    }
    return pixels;
}

// Darker toward the stripes' edges, for occlusion.
std::vector<uint8_t> Crevices(int size)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float phase = std::fmod(static_cast<float>(x) * 6.0f / static_cast<float>(size), 1.0f);
            const float edge = std::min(phase, 1.0f - phase) * 2.0f;
            const uint8_t value = static_cast<uint8_t>(80.0f + 175.0f * std::sqrt(edge));
            uint8_t* out = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            out[0] = value;
            out[1] = value;
            out[2] = value;
            out[3] = 255;
        }
    }
    return pixels;
}

// A UV sphere in glTF whose material names every map, each a file beside it.
bool WriteMaterialSphere(const std::filesystem::path& directory)
{
    const int rings = 32;
    const int segments = 64;
    std::vector<float> positions, normals, uvs;
    std::vector<uint16_t> indices;

    for (int ring = 0; ring <= rings; ++ring)
    {
        const float v = static_cast<float>(ring) / rings;
        const float theta = v * Pi;
        for (int segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / segments;
            const float phi = u * 2.0f * Pi;
            const float x = std::sin(theta) * std::cos(phi);
            const float y = std::cos(theta);
            const float z = std::sin(theta) * std::sin(phi);
            positions.insert(positions.end(), { x * 0.8f, y * 0.8f, z * 0.8f });
            normals.insert(normals.end(), { x, y, z });
            uvs.insert(uvs.end(), { u * 3.0f, v * 2.0f });
        }
    }
    for (int ring = 0; ring < rings; ++ring)
    {
        for (int segment = 0; segment < segments; ++segment)
        {
            const uint16_t a = static_cast<uint16_t>(ring * (segments + 1) + segment);
            const uint16_t b = static_cast<uint16_t>(a + segments + 1);
            indices.insert(indices.end(), { a, static_cast<uint16_t>(a + 1), b, static_cast<uint16_t>(a + 1),
                                            static_cast<uint16_t>(b + 1), b });
        }
    }

    std::vector<uint8_t> buffer;
    auto Append = [&](const void* data, size_t bytes) {
        const size_t offset = buffer.size();
        buffer.insert(buffer.end(), static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + bytes);
        while (buffer.size() % 4 != 0)
        {
            buffer.push_back(0);
        }
        return offset;
    };
    const size_t positionOffset = Append(positions.data(), positions.size() * 4);
    const size_t normalOffset = Append(normals.data(), normals.size() * 4);
    const size_t uvOffset = Append(uvs.data(), uvs.size() * 4);
    const size_t indexOffset = Append(indices.data(), indices.size() * 2);

    std::ofstream bin(directory / "sphere.bin", std::ios::binary);
    bin.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    bin.close();

    const size_t vertices = positions.size() / 3;
    char json[4096];
    std::snprintf(json, sizeof(json), R"({
  "asset": { "version": "2.0", "generator": "ludifex examples" },
  "scene": 0,
  "scenes": [ { "nodes": [ 0 ] } ],
  "nodes": [ { "mesh": 0 } ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2 }, "indices": 3, "material": 0 } ] } ],
  "materials": [ {
    "pbrMetallicRoughness": {
      "baseColorFactor": [ 0.9, 0.55, 0.3, 1.0 ],
      "metallicFactor": 1.0, "roughnessFactor": 1.0,
      "metallicRoughnessTexture": { "index": 0 }
    },
    "normalTexture": { "index": 1, "scale": 1.0 },
    "occlusionTexture": { "index": 2, "strength": 1.0 },
    "emissiveTexture": { "index": 3 },
    "emissiveFactor": [ 1.0, 1.0, 1.0 ],
    "extensions": { "KHR_materials_emissive_strength": { "emissiveStrength": 3.0 } }
  } ],
  "extensionsUsed": [ "KHR_materials_emissive_strength" ],
  "textures": [ { "source": 0 }, { "source": 1 }, { "source": 2 }, { "source": 3 } ],
  "images": [ { "uri": "stripes_mr.png" }, { "uri": "domes_n.png" }, { "uri": "crevices_ao.png" }, { "uri": "glow_e.png" } ],
  "buffers": [ { "byteLength": %zu, "uri": "sphere.bin" } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": %zu, "byteLength": %zu },
    { "buffer": 0, "byteOffset": %zu, "byteLength": %zu },
    { "buffer": 0, "byteOffset": %zu, "byteLength": %zu },
    { "buffer": 0, "byteOffset": %zu, "byteLength": %zu }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": %zu, "type": "VEC3", "min": [ -0.8, -0.8, -0.8 ], "max": [ 0.8, 0.8, 0.8 ] },
    { "bufferView": 1, "componentType": 5126, "count": %zu, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": %zu, "type": "VEC2" },
    { "bufferView": 3, "componentType": 5123, "count": %zu, "type": "SCALAR" }
  ]
}
)",
                  buffer.size(), positionOffset, positions.size() * 4, normalOffset, normals.size() * 4, uvOffset,
                  uvs.size() * 4, indexOffset, indices.size() * 2, vertices, vertices, vertices, indices.size());

    std::ofstream file(directory / "sphere.gltf", std::ios::binary);
    file << json;
    return file.good();
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({ .Title = "ludifex: surfaces", .Width = 1240, .Height = 740 });
    if (!app.IsValid())
    {
        return 1;
    }

    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "ludifex-materials";
    std::filesystem::create_directories(directory);
    examples::WritePng((directory / "domes_n.png").string(), 256, 256, DomeNormals(256, 4));
    examples::WritePng((directory / "stripes_mr.png").string(), 256, 256, MetalRoughStripes(256));
    examples::WritePng((directory / "crevices_ao.png").string(), 256, 256, Crevices(256));
    examples::WritePng((directory / "glow_e.png").string(), 256, 256, GlowBand(256));
    WriteMaterialSphere(directory);

    ludifex::World3D world = ludifex::CreateWorld3D({ .Gravity = { 0.0f, 0.0f, 0.0f } });

    ludifex::RenderSettings settings = world.GetRenderSettings();
    settings.Light.Direction = { -0.5f, -0.85f, -0.35f };
    settings.Light.Intensity = 2.4f;
    settings.AmbientColor = ludifex::Color::FromBytes(40, 44, 58);
    world.SetRenderSettings(settings);

    const ludifex::TextureId domes = ludifex::LoadTexture((directory / "domes_n.png").string(), ludifex::TextureUsage::Normal);

    ludifex::Actor3D ground = world.AddGround({ .Width = 24.0f, .Depth = 24.0f });
    ground.SetNormalMap(domes, 0.6f);
    ground.SetTextureTiling({ 12.0f, 12.0f });

    // Left to right: a box with the domes, a sphere with them, a box with
    // them turned off for comparison, a glowing box, and the glTF sphere.
    ludifex::Actor3D bumpyBox = world.AddBox({ .Scale = { 1.4f, 1.4f, 1.4f }, .Position = { -4.4f, 0.7f, 0.0f },
                                               .Type = ludifex::BodyType::Static });
    bumpyBox.SetColor(ludifex::Color::FromBytes(200, 205, 215));
    bumpyBox.SetNormalMap(domes);

    ludifex::Actor3D bumpySphere = world.AddSphere({ .Radius = 0.8f, .Position = { -2.2f, 0.8f, 0.0f },
                                                     .Type = ludifex::BodyType::Static });
    bumpySphere.SetColor(ludifex::Color::FromBytes(200, 205, 215));
    bumpySphere.SetNormalMap(domes);
    bumpySphere.SetTextureTiling({ 3.0f, 2.0f });

    ludifex::Actor3D plainBox = world.AddBox({ .Scale = { 1.4f, 1.4f, 1.4f }, .Position = { 0.0f, 0.7f, 0.0f },
                                               .Type = ludifex::BodyType::Static });
    plainBox.SetColor(ludifex::Color::FromBytes(200, 205, 215));

    ludifex::Actor3D glowing = world.AddBox({ .Scale = { 1.0f, 1.0f, 1.0f }, .Position = { 2.2f, 0.5f, 0.0f },
                                              .Type = ludifex::BodyType::Static });
    glowing.SetColor(ludifex::Color::FromBytes(40, 40, 50));
    glowing.SetEmission(ludifex::Color::FromBytes(80, 190, 255), 2.5f);

    ludifex::Actor3D material = world.AddModel({ .Path = (directory / "sphere.gltf").string(),
                                                 .Position = { 4.4f, 0.8f, 0.0f },
                                                 .Type = ludifex::BodyType::Static });

    // A roof over the glowing box's neighbour, so emission is seen in shadow.
    world.AddBox({ .Scale = { 2.4f, 0.1f, 2.4f }, .Position = { 2.2f, 2.4f, -0.2f }, .Type = ludifex::BodyType::Static });

    world.ApplyPendingChanges();
    (void)material;

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Vertical;
    opane::Viewport* view = root->Add<opane::Viewport>();
    view->Flex = 1.0f;
    view->SetWorld(world);

    opane::Label* caption = root->Add<opane::Label>();
    caption->Text = "domes on a box   |   on a sphere   |   none   |   emission under a roof   |   glTF: metal-rough stripes, normal, occlusion, emissive band";
    caption->Muted = true;
    caption->Align = opane::TextAlign::Center;
    caption->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(30.0f) };

    float angle = 0.0f;
    bool orbit = true;
    app.Run([&](float deltaSeconds) {
        if (app.GetInput().WasKeyPressed(opane::Key::Space))
        {
            orbit = !orbit;
        }
        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
        if (orbit)
        {
            angle += deltaSeconds * 0.15f;
        }
        world.SetCamera({ .Position = { std::sin(angle) * 2.5f, 3.2f, 8.5f + std::cos(angle) * 0.5f },
                          .Target = { 0.0f, 0.7f, 0.0f } });
    });

    app.Shutdown();
    return 0;
}
