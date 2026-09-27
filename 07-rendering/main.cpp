// Example 07: the renderer.
//
// One scene with the main rendering features, arranged so each is easy to
// check by eye:
//
//   pillars        stand upright under a fixed sun, so the direction and
//                  softness of their shadows can be checked
//   floor          a small checker image tiled across forty metres, which
//                  flickers if mip maps or filtering are missing
//   brick crates   a texture on a primitive shape, falling and tumbling
//   glass          translucent boxes, sorted and blended
//   sphere grid    sixty-four spheres sharing one custom material, each with
//                  its own colour, drawn in a single call
//   lamps          three coloured point lights circling, and one attached to a
//                  moving ball
//   vignette       a full-screen material after tone mapping
//
// Keys:
//   1-5     anti-aliasing preset: Off, Fast, Balanced, High, Temporal
//   S       shadows on and off
//   F       fog on and off
//   P       the vignette on and off
//   D       physics debug drawing
//   O       orbit the camera
//   Space   pause physics
//   R       drop the crates again
//   Escape  quit

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/Png.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

constexpr float Pi = 3.14159265358979323846f;

// A two-tone checker for the floor. Each cell is 32 pixels, so the texture
// tiles into a regular grid of eight by eight cells.
void WriteChecker(const std::string& path)
{
    constexpr int Size = 256;
    std::vector<uint8_t> pixels(static_cast<size_t>(Size) * Size * 4);
    for (int y = 0; y < Size; ++y)
    {
        for (int x = 0; x < Size; ++x)
        {
            const bool light = ((x / 32) + (y / 32)) % 2 == 0;
            uint8_t* texel = &pixels[(static_cast<size_t>(y) * Size + static_cast<size_t>(x)) * 4];
            texel[0] = light ? 176 : 118;
            texel[1] = light ? 180 : 124;
            texel[2] = light ? 190 : 138;
            texel[3] = 255;
        }
    }
    examples::WritePng(path, Size, Size, pixels);
}

// Staggered bricks with mortar lines, the classic test of a texture's
// orientation: rows must run horizontally on every side face.
void WriteBricks(const std::string& path)
{
    constexpr int Size = 256;
    constexpr int BrickWidth = 64;
    constexpr int BrickHeight = 32;
    constexpr int Mortar = 3;

    std::vector<uint8_t> pixels(static_cast<size_t>(Size) * Size * 4);
    for (int y = 0; y < Size; ++y)
    {
        const int row = y / BrickHeight;
        const int shift = (row % 2) * (BrickWidth / 2);
        for (int x = 0; x < Size; ++x)
        {
            const int bx = (x + shift) % BrickWidth;
            const int by = y % BrickHeight;
            const bool mortar = bx < Mortar || by < Mortar;

            // A little variation per brick so the wall does not look printed.
            const int brick = ((x + shift) / BrickWidth) * 7 + row * 13;
            const int shade = (brick * 37) % 40;

            uint8_t* texel = &pixels[(static_cast<size_t>(y) * Size + static_cast<size_t>(x)) * 4];
            texel[0] = static_cast<uint8_t>(mortar ? 200 : 150 + shade);
            texel[1] = static_cast<uint8_t>(mortar ? 196 : 70 + shade / 2);
            texel[2] = static_cast<uint8_t>(mortar ? 186 : 52 + shade / 3);
            texel[3] = 255;
        }
    }
    examples::WritePng(path, Size, Size, pixels);
}

const char* PresetName(ludifex::AntiAliasing mode)
{
    switch (mode)
    {
        case ludifex::AntiAliasing::Off:      return "Off";
        case ludifex::AntiAliasing::Fast:     return "Fast (FXAA)";
        case ludifex::AntiAliasing::Balanced: return "Balanced (MSAA 4x)";
        case ludifex::AntiAliasing::High:     return "High (MSAA 8x + FXAA)";
        case ludifex::AntiAliasing::Temporal: return "Temporal (MSAA 2x + TAA)";
    }
    return "?";
}

ludifex::Color Rainbow(float t)
{
    // A hue wheel, bright and saturated.
    const float r = 0.5f + 0.5f * std::cos(2.0f * Pi * (t + 0.0f));
    const float g = 0.5f + 0.5f * std::cos(2.0f * Pi * (t + 0.667f));
    const float b = 0.5f + 0.5f * std::cos(2.0f * Pi * (t + 0.333f));
    return ludifex::Color{ r, g, b, 1.0f };
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: rendering",
        .Width = 1280,
        .Height = 760,
    });

    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::AdoptHost({ app.GetGpuDevice(), app.GetWindow() });

    ludifex::World3D world = ludifex::CreateWorld3D();
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    // Assets are generated into a folder that is then added as an asset root,
    // so they load by bare name exactly as shipped files would.
    const std::filesystem::path assets = std::filesystem::temp_directory_path() / "ludifex-rendering-assets";
    std::filesystem::create_directories(assets);
    WriteChecker((assets / "checker.png").string());
    WriteBricks((assets / "bricks.png").string());

    ludifex::AddAssetRoot(assets.string());
    ludifex::AddAssetRoot(EXAMPLE_DIR);

    const ludifex::TextureId checker = ludifex::LoadTexture("checker.png");
    const ludifex::TextureId bricks = ludifex::LoadTexture("bricks.png");

    const ludifex::MaterialId tinted = ludifex::CreateMaterial({
        .ShaderPath = "tinted.hlsl",
        .Uniforms = {
            { "Tint", ludifex::Color::FromBytes(255, 255, 255) },
            { "Glow", 0.0f },
        },
    });

    const ludifex::MaterialId vignette = ludifex::CreateMaterial({
        .ShaderPath = "vignette.hlsl",
        .Uniforms = { { "Strength", 0.55f } },
    });

    ludifex::RenderSettings& settings = world.GetRenderSettings();
    settings.Light.Direction = { -0.55f, -1.0f, -0.35f };
    settings.SkyColor = ludifex::Color::FromBytes(34, 40, 54);
    settings.Fog.Tint = settings.SkyColor;
    settings.Fog.Start = 12.0f;
    settings.Fog.End = 45.0f;

    ludifex::Camera3D& camera = world.GetCamera();
    camera.Position = { 2.0f, 9.0f, 17.0f };
    camera.Target = { 0.0f, 1.0f, 0.0f };

    // --- the floor ---------------------------------------------------------

    ludifex::Actor3D ground = world.AddGround({ .Width = 40.0f, .Depth = 40.0f });
    ground.SetTexture(checker);
    ground.SetColor(ludifex::Color{ 1.0f, 1.0f, 1.0f, 1.0f });
    ground.SetTextureTiling({ 10.0f, 10.0f });

    // --- pillars, upright, to read the shadows by --------------------------

    for (int index = 0; index < 3; ++index)
    {
        ludifex::Actor3D pillar = world.AddBox({
            .Scale = { 0.8f, 5.0f, 0.8f },
            .Position = { -7.0f + static_cast<float>(index) * 2.5f, 2.5f, -2.0f },
            .Type = ludifex::BodyType::Static,
            .Name = "pillar",
        });
        pillar.SetColor(ludifex::Color::FromBytes(210, 206, 196));
    }

    // --- brick crates --------------------------------------------------------

    std::vector<ludifex::Actor3D> crates;
    auto DropCrates = [&]() {
        for (ludifex::Actor3D& crate : crates)
        {
            crate.Destroy();
        }
        crates.clear();

        for (int index = 0; index < 4; ++index)
        {
            ludifex::Actor3D crate = world.AddBox({
                .Scale = { 1.2f, 1.2f, 1.2f },
                .Position = { -2.0f + static_cast<float>(index) * 0.9f, 3.0f + static_cast<float>(index) * 1.6f,
                              3.0f },
                .Rotation = { 0.1f * static_cast<float>(index), 0.3f, 0.0f, 0.95f },
            });
            crate.SetTexture(bricks);
            crate.SetColor(ludifex::Color{ 1.0f, 1.0f, 1.0f, 1.0f });
            crates.push_back(crate);
        }
    };
    DropCrates();

    // --- glass ---------------------------------------------------------------

    for (int index = 0; index < 2; ++index)
    {
        ludifex::Actor3D glass = world.AddBox({
            .Scale = { 1.6f, 2.4f, 0.3f },
            .Position = { 4.5f + static_cast<float>(index) * 1.2f, 1.2f, 1.0f + static_cast<float>(index) * 1.4f },
            .Type = ludifex::BodyType::Static,
            .Name = "glass",
        });
        glass.SetColor(ludifex::Color::FromBytes(150, 210, 255, 80));
        glass.SetRoughness(0.1f);
    }

    // --- sixty-four spheres, one material, sixty-four colours ----------------

    for (int row = 0; row < 8; ++row)
    {
        for (int column = 0; column < 8; ++column)
        {
            ludifex::Actor3D sphere = world.AddSphere({
                .Radius = 0.32f,
                .Position = { 1.5f + static_cast<float>(column) * 0.8f, 0.32f,
                              -9.0f + static_cast<float>(row) * 0.8f },
                .Type = ludifex::BodyType::Static,
            });
            sphere.SetMaterial(tinted);
            sphere.SetUniform("Tint", Rainbow(static_cast<float>(row * 8 + column) / 64.0f));
            sphere.SetUniform("Glow", (row + column) % 3 == 0 ? 0.6f : 0.0f);
            sphere.SetRoughness(0.25f);
        }
    }

    // --- lamps ---------------------------------------------------------------

    const ludifex::Color lampColors[3] = { ludifex::Color::FromBytes(255, 80, 60),
                                           ludifex::Color::FromBytes(80, 255, 120),
                                           ludifex::Color::FromBytes(90, 140, 255) };
    std::vector<ludifex::Light3D> lamps;
    for (const ludifex::Color& color : lampColors)
    {
        lamps.push_back(world.AddPointLight({ .Tint = color, .Intensity = 6.0f, .Range = 7.0f }));
    }

    // A lantern on a ball: the light follows the ball wherever physics takes it.
    ludifex::Actor3D lanternBall = world.AddSphere({ .Radius = 0.3f, .Position = { -3.0f, 6.0f, 6.0f } });
    lanternBall.SetColor(ludifex::Color::FromBytes(255, 214, 140));
    ludifex::Light3D lantern = world.AddPointLight({
        .Tint = ludifex::Color::FromBytes(255, 190, 110),
        .Intensity = 5.0f,
        .Range = 6.0f,
    });
    lantern.AttachTo(lanternBall, { 0.0f, 0.6f, 0.0f });

    world.AddPostProcess(vignette, ludifex::PassPoint::AfterToneMap);
    // --- interface -----------------------------------------------------------

    opane::Element* root = app.GetRoot();

    opane::Panel* panel = root->Add<opane::Panel>();
    panel->Size = opane::Size2::FromOffset(330.0f, 178.0f);
    panel->Position = opane::Position2::FromOffset(20.0f, 20.0f);
    panel->ChildLayout = opane::LayoutMode::Vertical;
    panel->Padding = 14.0f;
    panel->Spacing = 4.0f;

    std::vector<opane::Label*> lines;
    for (int index = 0; index < 6; ++index)
    {
        opane::Label* line = panel->Add<opane::Label>();
        line->Muted = index > 0;
        line->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };
        lines.push_back(line);
    }

    opane::Label* keys = root->Add<opane::Label>();
    keys->Text = "1-5 anti-aliasing   S shadows   F fog   P vignette   D debug   O orbit   R drop";
    keys->Muted = true;
    keys->Align = opane::TextAlign::Center;
    keys->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };
    keys->Position = opane::Position2{ opane::Dim::FromScale(0.0f), opane::Dim{ 1.0f, -34.0f } };

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;
    bool orbiting = false;
    bool postOn = true;
    bool debugOn = false;
    float orbitAngle = std::atan2(camera.Position.X, camera.Position.Z);
    float time = 0.0f;
    float frameMilliseconds = 0.0f;

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        time += deltaSeconds;
        frameMilliseconds = frameMilliseconds * 0.9f + deltaSeconds * 1000.0f * 0.1f;

        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        const opane::Key presetKeys[5] = { opane::Key::Num1, opane::Key::Num2, opane::Key::Num3, opane::Key::Num4,
                                           opane::Key::Num5 };
        for (int index = 0; index < 5; ++index)
        {
            if (input.WasKeyPressed(presetKeys[index]))
            {
                world.SetAntiAliasing(static_cast<ludifex::AntiAliasing>(index));
            }
        }

        if (input.WasKeyPressed(opane::Key::S))
        {
            settings.Shadows.Enabled = !settings.Shadows.Enabled;
        }
        if (input.WasKeyPressed(opane::Key::F))
        {
            settings.Fog.Enabled = !settings.Fog.Enabled;
        }
        if (input.WasKeyPressed(opane::Key::P))
        {
            postOn = !postOn;
            if (postOn)
            {
                world.AddPostProcess(vignette, ludifex::PassPoint::AfterToneMap);
            }
            else
            {
                world.RemovePostProcess(vignette);
            }
        }
        if (input.WasKeyPressed(opane::Key::D))
        {
            debugOn = !debugOn;
            world.SetPhysicsDebugDraw(debugOn);
        }
        if (input.WasKeyPressed(opane::Key::O))
        {
            orbiting = !orbiting;
        }
        if (input.WasKeyPressed(opane::Key::R))
        {
            DropCrates();
        }
        if (input.WasKeyPressed(opane::Key::Space))
        {
            world.IsPhysicsRunning() ? world.StopPhysics() : world.StartPhysics();
        }

        // Three lamps circling the sphere grid at different heights.
        for (size_t index = 0; index < lamps.size(); ++index)
        {
            const float angle = time * 0.7f + static_cast<float>(index) * 2.0f * Pi / 3.0f;
            lamps[index].SetPosition({ 4.3f + std::cos(angle) * 3.4f, 1.1f, -6.2f + std::sin(angle) * 3.4f });
        }

        if (orbiting)
        {
            orbitAngle += deltaSeconds * 0.25f;
            camera.Position = { std::sin(orbitAngle) * 17.1f, 9.0f, std::cos(orbitAngle) * 17.1f };
        }

        // The world's own axes at the origin, drawn every frame.
        world.DrawLine({ 0.0f, 0.02f, 0.0f }, { 2.0f, 0.02f, 0.0f }, ludifex::Color{ 1.0f, 0.2f, 0.2f, 1.0f });
        world.DrawLine({ 0.0f, 0.02f, 0.0f }, { 0.0f, 2.0f, 0.0f }, ludifex::Color{ 0.2f, 1.0f, 0.2f, 1.0f });
        world.DrawLine({ 0.0f, 0.02f, 0.0f }, { 0.0f, 0.02f, 2.0f }, ludifex::Color{ 0.3f, 0.5f, 1.0f, 1.0f });

        const opane::Vec2 windowSize = app.GetWindowSize();
        const int width = static_cast<int>(windowSize.X);
        const int height = static_cast<int>(windowSize.Y);
        if (width != lastWidth || height != lastHeight)
        {
            lastWidth = width;
            lastHeight = height;
            world.SetRenderSize(width, height);
            if (worldView.IsValid())
            {
                app.DestroyTexture(worldView);
            }
            worldView = app.WrapExternalTexture(world.GetRenderTarget(), width, height);
        }

        world.Update(deltaSeconds);
        world.Render();

        if (worldView.IsValid())
        {
            app.GetDrawList().DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }

        char text[128];
        lines[0]->Text = std::string("Anti-aliasing: ") + PresetName(settings.Mode);
        std::snprintf(text, sizeof(text), "%u draw calls   %u objects   %u culled", world.GetLastDrawCallCount(),
                      world.GetDrawnInstanceCount(), world.GetCulledCount());
        lines[1]->Text = text;
        std::snprintf(text, sizeof(text), "%zu lights   %zu actors", world.GetLightCount(), world.GetActorCount());
        lines[2]->Text = text;
        std::snprintf(text, sizeof(text), "shadows %s   fog %s   vignette %s", settings.Shadows.Enabled ? "on" : "off",
                      settings.Fog.Enabled ? "on" : "off", postOn ? "on" : "off");
        lines[3]->Text = text;
        std::snprintf(text, sizeof(text), "%.2f ms per frame", static_cast<double>(frameMilliseconds));
        lines[4]->Text = text;
        lines[5]->Text = world.IsPhysicsRunning() ? "physics running" : "physics paused";
    });

    app.Shutdown();
    return 0;
}
