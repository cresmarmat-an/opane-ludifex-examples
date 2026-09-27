// Example 14: eight short programs.
//
// Eight small, complete programs, one function each, from the simplest 3D
// world to a hand-written frame loop. Each follows the documentation.
//
// Run one by passing its number:  14-eight-programs 6
// With no number it runs 6, which uses both libraries together.
//
// The assets the programs use are written to a temporary folder first, and
// that folder is added as an asset root, so "robot.gltf" and "click.wav" are
// found the same way as in a real program.

#include "../common/CrashReport.h"
#include "../common/Gltf.h"
#include "../common/Png.h"
#include "../common/Tone.h"

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

// Reports whether a shader the example uses compiled.
//
// A material that fails to compile leaves the object drawn with the built-in
// shader, and a post-process pass that fails is skipped, so the window still
// opens and the scene can look fine. This makes the failure visible.
void Require(bool valid, const char* what)
{
    if (!valid)
    {
        std::printf("  [FAIL] \"%s\" did not compile; the example is not running as written.\n",
                    what);
    }
    else
    {
        std::printf("  [pass] \"%s\" compiled.\n", what);
    }
}

// The files the examples ask for, written once into a directory that is then
// added as an asset root.
void WriteAssets(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);

    examples::WriteGltfSphere((directory / "robot.gltf").string(), 18, 0.9f);
    examples::WriteToneWav((directory / "click.wav").string(), 880.0f, 120, 0.7f, 18.0f);
    examples::WriteToneWav((directory / "hit.wav").string(), 220.0f, 250, 0.8f, 9.0f);

    // Looping music: a whole number of cycles in a second, so the seam is
    // silent.
    examples::WriteToneWav((directory / "background.ogg.wav").string(), 196.0f, 1000, 0.25f, 0.0f);
    std::filesystem::copy_file((directory / "background.ogg.wav").string(),
                               (directory / "background.ogg").string(),
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file((directory / "background.ogg.wav").string(),
                               (directory / "music.ogg").string(),
                               std::filesystem::copy_options::overwrite_existing);

    // A checkerboard for the sprite and the backdrop: two colours, written
    // straight into the pixels.
    auto Checkers = [&](const char* name, int size, int square, uint8_t red, uint8_t green,
                        uint8_t blue) {
        std::vector<uint8_t> pixels(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const bool light = ((x / square) + (y / square)) % 2 == 0;
                const size_t at = (static_cast<size_t>(y) * static_cast<size_t>(size) +
                                   static_cast<size_t>(x)) * 4;
                pixels[at + 0] = light ? red : static_cast<uint8_t>(red / 2);
                pixels[at + 1] = light ? green : static_cast<uint8_t>(green / 2);
                pixels[at + 2] = light ? blue : static_cast<uint8_t>(blue / 2);
                pixels[at + 3] = 255;
            }
        }
        examples::WritePng((directory / name).string(), size, size, pixels);
    };

    Checkers("player.png", 64, 8, 240, 190, 90);
    Checkers("sky.png", 128, 32, 90, 130, 220);

    // The shaders the material examples name.
    const std::string aurora = R"(#include "material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float2 uv = LocalUv(input);
    float wave = sin(uv.x * 6.0 + Seconds() * Params[2].x) * 0.5 + 0.5;
    float3 color = lerp(Params[0].rgb, Params[1].rgb, wave * uv.y);
    return Premultiply(color, ShapeCoverage(input) * input.Color.a);
}
)";

    const std::string toon = R"(#include "world_material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float3 tint = Param(input, 0).rgb;
    float bands = max(Params[1].x, 1.0);

    float3 shaded = Shade(input, tint);
    float level = floor(dot(shaded, float3(0.299, 0.587, 0.114)) * bands) / bands;
    return float4(tint * (0.25 + level), 1.0);
}
)";

    const std::string grade = R"(#include "world_post.hlsli"

float4 FragmentMain(PostInput input) : SV_Target
{
    float3 color = SampleScene(input.UV).rgb;
    float grey = dot(color, float3(0.299, 0.587, 0.114));
    return float4(lerp(float3(grey, grey, grey), color, 1.25), 1.0);
}
)";

    const std::string outline = R"(#include "world_post.hlsli"

float4 FragmentMain(PostInput input) : SV_Target
{
    float here = SceneDistance(input.UV);

    // One pixel, whatever the window is: TexelSize carries it, so this does
    // not have to be written again when the window is resized.
    float2 step = TexelSize.xy;

    float around = 0.0;
    around += abs(here - SceneDistance(input.UV + float2(step.x, 0.0)));
    around += abs(here - SceneDistance(input.UV - float2(step.x, 0.0)));
    around += abs(here - SceneDistance(input.UV + float2(0.0, step.y)));
    around += abs(here - SceneDistance(input.UV - float2(0.0, step.y)));

    float edge = saturate(around * 4.0);
    float3 color = SampleScene(input.UV).rgb;
    return float4(lerp(color, float3(0.02, 0.02, 0.03), edge), 1.0);
}
)";

    auto Write = [&](const char* name, const std::string& text) {
        std::FILE* file = std::fopen((directory / name).string().c_str(), "wb");
        if (file != nullptr)
        {
            std::fwrite(text.data(), 1, text.size(), file);
            std::fclose(file);
        }
    };

    Write("aurora.hlsl", aurora);
    Write("toon.hlsl", toon);
    Write("grade.hlsl", grade);
    Write("outline.hlsl", outline);
}

// ---------------------------------------------------------------------------
// 8.1 Simplest 3D program
// ---------------------------------------------------------------------------

int Example1()
{
    opane::App app = opane::StartApp({ .Title = "My First 3D World", .Width = 1280, .Height = 720 });

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddModel("robot.gltf");
    world.AddGround();
    world.PlayMusic("background.ogg");

    world.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.2 Simplest 2D program
// ---------------------------------------------------------------------------

int Example2()
{
    opane::App app = opane::StartApp({ .Title = "My First 2D Game", .Width = 800, .Height = 600 });

    ludifex::World2D world = ludifex::CreateWorld2D();
    world.SetBackground("sky.png");

    ludifex::Actor2D player = world.AddSprite({ .Path = "player.png", .Position = { 0.0f, 3.0f } });

    world.AddRectangle({
        .Width = 16.0f,
        .Height = 1.0f,
        .Position = { 0.0f, -0.5f },
        .Type = ludifex::BodyType::Static,
    });

    world.PlayMusic("music.ogg");
    world.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.3 Interface with a custom-shaded button
// ---------------------------------------------------------------------------

int Example3()
{
    opane::App app = opane::StartApp({ .Title = "Menu", .Width = 1280, .Height = 720 });

    opane::MaterialId aurora = app.CreateMaterial({
        .ShaderPath = "aurora.hlsl",
        .Uniforms = {
            { "ColorA", opane::Color::FromBytes(51, 102, 255) },
            { "ColorB", opane::Color::FromBytes(230, 51, 153) },
            { "Speed", 0.5f },
        },
    });
    Require(aurora.IsValid(), "aurora.hlsl");

    opane::SoundId click = app.LoadSound("click.wav");

    opane::Button* start = app.GetRoot()->Add<opane::Button>();
    start->Text = "Start Game";
    start->Size = opane::Size2::FromOffset(220.0f, 64.0f);
    start->PlaceCentered();
    start->Material = aurora;

    start->OnHoverStart = [&] { start->AnimateUniform("Speed", 2.0f, 0.0f, 0.0f, 0.0f, 0.25f); };
    start->OnHoverEnd = [&] { start->AnimateUniform("Speed", 0.5f, 0.0f, 0.0f, 0.0f, 0.25f); };
    start->OnClick = [&] { app.PlaySound(click); };

    app.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.4 A fully custom element
// ---------------------------------------------------------------------------

class RadialGauge : public opane::Element
{
public:
    float Value = 0.0f;
    opane::MaterialId GaugeMaterial;

    opane::Vec2 Measure(opane::Vec2 available) override
    {
        return { 120.0f, 120.0f };
    }

    void Paint(opane::DrawList& drawList) override
    {
        const opane::Rect bounds = GetBounds();

        drawList.SetMaterial(GaugeMaterial);
        drawList.FillRect(bounds, opane::Color{ 1.0f, 1.0f, 1.0f, 1.0f });
        drawList.SetMaterial({});

        char label[16];
        std::snprintf(label, sizeof(label), "%.0f%%", static_cast<double>(Value * 100.0f));
        drawList.DrawTextInRect(label, bounds, GetTheme().Font, GetTheme().Text,
                                opane::TextAlign::Center);
    }

    bool HitTest(opane::Vec2 point) const override
    {
        const opane::Rect bounds = GetBounds();
        const float dx = point.X - (bounds.X + bounds.Width * 0.5f);
        const float dy = point.Y - (bounds.Y + bounds.Height * 0.5f);
        return dx * dx + dy * dy <= 60.0f * 60.0f;
    }

    void OnEvent(opane::Event& event) override
    {
        if (event.Type == opane::EventType::PointerDown)
        {
            const opane::Rect bounds = GetBounds();
            Value = std::clamp((event.Position.X - bounds.X) / bounds.Width, 0.0f, 1.0f);
            event.Handled = true;
        }
    }
};

int Example4()
{
    opane::App app = opane::StartApp({ .Title = "A custom element", .Width = 900, .Height = 600 });

    RadialGauge* gauge = app.GetRoot()->Add<RadialGauge>();
    gauge->GaugeMaterial = app.CreateMaterial({
        .ShaderPath = "aurora.hlsl",
        .Uniforms = {
            { "ColorA", opane::Color::FromBytes(51, 102, 255) },
            { "ColorB", opane::Color::FromBytes(230, 51, 153) },
            { "Speed", 1.0f },
        },
    });
    Require(gauge->GaugeMaterial.IsValid(), "aurora.hlsl");
    gauge->PlaceCentered();

    app.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.5 A custom world material and post-process chain
// ---------------------------------------------------------------------------

int Example5()
{
    opane::App app = opane::StartApp({ .Title = "Toon", .Width = 1280, .Height = 720 });

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddGround();

    ludifex::MaterialId toon = ludifex::CreateMaterial({
        .ShaderPath = "toon.hlsl",
        .Uniforms = {
            { "Tint", ludifex::Color{ 1.0f, 0.85f, 0.7f, 1.0f } },
            { "BandCount", 4.0f },
            { "RimPower", 3.0f },
        },
    });

    ludifex::Actor3D robot = world.AddModel("robot.gltf");
    robot.SetMaterial(toon);
    robot.SetUniform("Tint", ludifex::Color{ 0.7f, 0.9f, 1.0f, 1.0f });

    const ludifex::MaterialId grade = ludifex::CreateMaterial({ .ShaderPath = "grade.hlsl" });
    const ludifex::MaterialId outline = ludifex::CreateMaterial({ .ShaderPath = "outline.hlsl" });

    // Checked, because a post-process shader that fails to compile leaves a
    // scene that looks almost right, and the failure is easy to miss.
    Require(toon.IsValid(), "toon.hlsl");
    Require(grade.IsValid(), "grade.hlsl");
    Require(outline.IsValid(), "outline.hlsl");

    world.AddPostProcess(grade, ludifex::PassPoint::BeforeToneMap);
    world.AddPostProcess(outline, ludifex::PassPoint::AfterToneMap);

    world.SetAntiAliasing(ludifex::AntiAliasing::High);

    world.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.6 Interface driving physics
// ---------------------------------------------------------------------------

int Example6()
{
    opane::App app = opane::StartApp({ .Title = "Physics Control", .Width = 1280, .Height = 720 });

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddGround({ .Width = 50.0f, .Depth = 50.0f });

    ludifex::Actor3D crate = world.AddBox({
        .Scale = { 1.0f, 1.0f, 1.0f },
        .Position = { 0.0f, 5.0f, 0.0f },
    });

    opane::Button* pause = app.GetRoot()->Add<opane::Button>();
    pause->Text = "Pause Physics";
    pause->Size = opane::Size2::FromOffset(180.0f, 40.0f);
    pause->PlaceAtCorner(opane::Corner::TopLeft, 10.0f);
    pause->OnClick = [&] {
        if (world.IsPhysicsRunning())
        {
            world.StopPhysics();
            pause->Text = "Start Physics";
        }
        else
        {
            world.StartPhysics();
            pause->Text = "Pause Physics";
        }
    };

    opane::Slider* gravity = app.GetRoot()->Add<opane::Slider>();
    gravity->Minimum = -30.0f;
    gravity->Maximum = 0.0f;
    gravity->Value = -10.0f;
    gravity->Size = opane::Size2::FromOffset(200.0f, 28.0f);
    gravity->PlaceAtCorner(opane::Corner::TopRight, 10.0f);
    gravity->OnChanged = [&](float y) { world.SetGravity({ 0.0f, y, 0.0f }); };

    app.SetMainWorld(world);
    app.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.7 Mutating a world while it runs
// ---------------------------------------------------------------------------

int Example7()
{
    opane::App app = opane::StartApp({ .Title = "Mutation", .Width = 1280, .Height = 720 });

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddGround({ .Width = 40.0f, .Depth = 40.0f });

    // Something to knock into: a tower the ball arrives at.
    for (int index = 0; index < 6; ++index)
    {
        world.AddBox({
            .Scale = { 0.8f, 0.8f, 0.8f },
            .Position = { 0.0f, 0.4f + static_cast<float>(index) * 0.85f, 0.0f },
            .Name = "Block",
        });
    }

    ludifex::Actor3D ball = world.AddSphere({
        .Radius = 0.6f,
        .Position = { -9.0f, 3.0f, 0.0f },
        .Restitution = 0.3f,
        .Name = "Ball",
    });
    ball.SetLinearVelocity({ 14.0f, 0.0f, 0.0f });

    ludifex::SoundId hit = ludifex::LoadSound("hit.wav");

    ball.WhenCollided([&](const ludifex::CollisionInfo& info) {
        if (info.ImpactSpeed > 5.0f)
        {
            world.PlaySoundAt(hit, ball);

            // Both are safe inside a callback: destruction defers to the next
            // sync point, and creation is queued in submission order.
            ludifex::Actor3D other = info.Other;
            other.Destroy();

            for (int index = 0; index < 8; ++index)
            {
                world.AddSphere({
                    .Radius = 0.1f,
                    .Position = info.Point,
                });
            }
        }
    });

    app.SetMainWorld(world);
    app.Run();

    app.Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// 8.8 A hand-written loop
// ---------------------------------------------------------------------------

int Example8()
{
    opane::App app = opane::StartApp({ .Title = "Manual Loop" });

    ludifex::AdoptHost({ app.GetGpuDevice(), app.GetWindow() });

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.SetPhysicsMode(ludifex::PhysicsMode::Manual);

    world.AddGround({ .Width = 40.0f, .Depth = 40.0f });
    for (int index = 0; index < 10; ++index)
    {
        world.AddBox({
            .Scale = { 0.7f, 0.7f, 0.7f },
            .Position = { static_cast<float>(index % 3) * 0.8f - 0.8f,
                          1.0f + static_cast<float>(index) * 0.9f, 0.0f },
        });
    }

    constexpr float FixedStep = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (app.IsOpen())
    {
        const float delta = app.PollEvents();
        accumulator += std::min(delta, 0.25f);

        while (accumulator >= FixedStep)
        {
            world.StepPhysics(FixedStep);
            accumulator -= FixedStep;
        }

        world.SetInterpolationAlpha(accumulator / FixedStep);
        world.Update(delta);
        app.UpdateInterface(delta);

        app.BeginFrame();
        app.RenderWorld(world);
        app.EndFrame();
    }

    app.Shutdown();
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    examples::InstallCrashReport();

    const std::filesystem::path assets = std::filesystem::temp_directory_path() / "ludifex-examples";
    WriteAssets(assets);

    // Both libraries look for files through their own asset roots, so the
    // examples name "robot.gltf" and nothing else.
    ludifex::AddAssetRoot(assets.string());
    opane::AddAssetRoot(assets.string());

    const int which = argc > 1 ? std::atoi(argv[1]) : 6;

    switch (which)
    {
        case 1: return Example1();
        case 2: return Example2();
        case 3: return Example3();
        case 4: return Example4();
        case 5: return Example5();
        case 7: return Example7();
        case 8: return Example8();
        case 6:
        default: return Example6();
    }
}
