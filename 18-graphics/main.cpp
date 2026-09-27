// Example 18: graphics quality, ray tracing, and path tracing.
//
// One scene with something for each effect to show:
//
//   sky            the procedural sky, lighting the world and reflected in it
//   spheres        five metals from mirror to matte, for judging reflections
//   columns        a row forty metres long, whose shadows show the cascades
//                  and, when traced, soften with distance
//   corner         two walls and a crate, where ambient occlusion collects
//   neon           glowing bars, for bloom
//   glass          a translucent pane
//   dancer         a swaying skinned column, which the ray tracer poses on the
//                  CPU every frame. Path tracing stops it, because any movement
//                  restarts the path-traced image.
//
// Keys:
//   1-6     preset: Potato, Low, Medium, High, Ultra, Extreme
//   T O R   ray-traced shadows, occlusion, reflections
//   P       path tracing
//   K       sky    F  filmic curve    E  automatic exposure    B  bloom
//   Space   orbit the camera          Escape  quit
//
// --tour goes through every preset and tracing mode for a few seconds each,
// prints the frame time of each, and quits. --quality N, --shadows,
// --occlusion, --reflections, and --path start in that state.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/GltfSkinned.h"
#include "../common/Png.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

constexpr float Pi = 3.14159265358979323846f;

// A soft two-tone checker, 32 texels a cell.
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
            texel[0] = light ? 196 : 132;
            texel[1] = light ? 192 : 128;
            texel[2] = light ? 184 : 124;
            texel[3] = 255;
        }
    }
    examples::WritePng(path, Size, Size, pixels);
}

// Planks: long boards with a grain, for the crate.
void WritePlanks(const std::string& path)
{
    constexpr int Size = 256;
    std::vector<uint8_t> pixels(static_cast<size_t>(Size) * Size * 4);
    for (int y = 0; y < Size; ++y)
    {
        const int board = y / 32;
        for (int x = 0; x < Size; ++x)
        {
            const float grain = 0.5f + 0.5f * std::sin(static_cast<float>(x) * 0.09f + static_cast<float>(board) * 1.7f +
                                                       std::sin(static_cast<float>(y) * 0.3f) * 0.8f);
            const bool gap = (y % 32) < 2;
            uint8_t* texel = &pixels[(static_cast<size_t>(y) * Size + static_cast<size_t>(x)) * 4];
            const float shade = gap ? 0.35f : 0.75f + 0.25f * grain;
            texel[0] = static_cast<uint8_t>(170.0f * shade);
            texel[1] = static_cast<uint8_t>(118.0f * shade);
            texel[2] = static_cast<uint8_t>(72.0f * shade);
            texel[3] = 255;
        }
    }
    examples::WritePng(path, Size, Size, pixels);
}

struct Mode
{
    const char* Name;
    int Quality;
    bool Shadows;
    bool Occlusion;
    bool Reflections;
    bool Path;
    float Seconds;
};

} // namespace

int main(int argumentCount, char** arguments)
{
    int quality = static_cast<int>(ludifex::GraphicsQuality::High);
    bool tour = false;
    bool startShadows = false;
    bool startOcclusion = false;
    bool startReflections = false;
    bool startPath = false;
    for (int index = 1; index < argumentCount; ++index)
    {
        const std::string argument = arguments[index];
        if (argument == "--tour")
        {
            tour = true;
        }
        else if (argument == "--quality" && index + 1 < argumentCount)
        {
            quality = std::atoi(arguments[++index]);
        }
        else if (argument == "--shadows")
        {
            startShadows = true;
        }
        else if (argument == "--occlusion")
        {
            startOcclusion = true;
        }
        else if (argument == "--reflections")
        {
            startReflections = true;
        }
        else if (argument == "--path")
        {
            startPath = true;
        }
    }

    opane::App app = opane::StartApp({
        .Title = "ludifex: graphics quality",
        .Width = 1280,
        .Height = 720,
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

    const std::filesystem::path assets = std::filesystem::temp_directory_path() / "ludifex-graphics-assets";
    std::filesystem::create_directories(assets);
    WriteChecker((assets / "checker.png").string());
    WritePlanks((assets / "planks.png").string());
    const std::string dancerPath = (assets / "dancer.gltf").string();
    examples::WriteGltfSkinnedColumn(dancerPath, 7, 0.34f, 0.18f);
    ludifex::AddAssetRoot(assets.string());

    const ludifex::TextureId checker = ludifex::LoadTexture("checker.png");
    const ludifex::TextureId planks = ludifex::LoadTexture("planks.png");

    // --- the look: a sky, a low sun, and a little grading --------------------

    ludifex::RenderSettings& settings = world.GetRenderSettings();
    ludifex::ApplyGraphicsQuality(settings, static_cast<ludifex::GraphicsQuality>(quality));
    // The sun low on the left, so every shadow falls across the floor toward
    // the camera rather than hiding behind what casts it.
    settings.Light.Direction = { 0.75f, -0.5f, 0.3f };
    settings.Light.Tint = ludifex::Color::FromBytes(255, 240, 220);
    settings.Light.Intensity = 1.5f;
    settings.Sky.Enabled = true;
    settings.Grading.Curve = ludifex::ToneCurve::Filmic;
    settings.Exposure = 1.0f;
    settings.Grading.Saturation = 1.08f;
    settings.RayTracing.Shadows = startShadows;
    settings.RayTracing.AmbientOcclusion = startOcclusion;
    settings.RayTracing.Reflections = startReflections;
    settings.PathTracing.Enabled = startPath;

    ludifex::Camera3D& camera = world.GetCamera();
    camera.Position = { 2.5f, 4.2f, 13.0f };
    camera.Target = { 0.0f, 1.2f, 0.0f };
    camera.FarPlane = 300.0f;

    // --- the floor -------------------------------------------------------------

    ludifex::Actor3D ground = world.AddGround({ .Width = 120.0f, .Depth = 120.0f });
    ground.SetTexture(checker);
    ground.SetColor(ludifex::Color{ 1.0f, 1.0f, 1.0f, 1.0f });
    ground.SetTextureTiling({ 30.0f, 30.0f });
    ground.SetRoughness(0.5f);

    // --- five metals, mirror to matte ---------------------------------------------

    const ludifex::Color metals[5] = { ludifex::Color::FromBytes(236, 236, 240), ludifex::Color::FromBytes(255, 204, 120),
                                       ludifex::Color::FromBytes(236, 236, 240), ludifex::Color::FromBytes(232, 150, 110),
                                       ludifex::Color::FromBytes(236, 236, 240) };
    for (int index = 0; index < 5; ++index)
    {
        ludifex::Actor3D sphere = world.AddSphere({
            .Radius = 0.75f,
            .Position = { -5.0f + static_cast<float>(index) * 2.5f, 0.75f, 0.0f },
            .Type = ludifex::BodyType::Static,
        });
        sphere.SetColor(metals[index]);
        sphere.SetMetallic(1.0f);
        sphere.SetRoughness(0.04f + static_cast<float>(index) * 0.2f);
    }

    // --- columns receding, for the cascades ----------------------------------------

    for (int index = 0; index < 8; ++index)
    {
        ludifex::Actor3D column = world.AddBox({
            .Scale = { 0.6f, 4.0f, 0.6f },
            .Position = { 7.5f, 2.0f, 2.0f - static_cast<float>(index) * 6.0f },
            .Type = ludifex::BodyType::Static,
        });
        column.SetColor(ludifex::Color::FromBytes(214, 208, 198));
    }

    // --- a corner, for occlusion --------------------------------------------------------

    ludifex::Actor3D backWall = world.AddBox({
        .Scale = { 7.0f, 3.5f, 0.3f },
        .Position = { -4.5f, 1.75f, -4.0f },
        .Type = ludifex::BodyType::Static,
    });
    backWall.SetColor(ludifex::Color::FromBytes(226, 222, 214));
    ludifex::Actor3D sideWall = world.AddBox({
        .Scale = { 0.3f, 3.5f, 5.0f },
        .Position = { -8.0f, 1.75f, -1.35f },
        .Type = ludifex::BodyType::Static,
    });
    sideWall.SetColor(ludifex::Color::FromBytes(186, 204, 226));
    ludifex::Actor3D crate = world.AddBox({
        .Scale = { 1.3f, 1.3f, 1.3f },
        .Position = { -6.4f, 0.65f, -2.9f },
        .Rotation = ludifex::Quat::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, 0.35f),
        .Type = ludifex::BodyType::Static,
    });
    crate.SetTexture(planks);
    crate.SetColor(ludifex::Color{ 1.0f, 1.0f, 1.0f, 1.0f });

    // --- neon, for bloom -------------------------------------------------------------------

    const ludifex::Color neon[3] = { ludifex::Color::FromBytes(255, 70, 150), ludifex::Color::FromBytes(60, 220, 255),
                                     ludifex::Color::FromBytes(255, 190, 60) };
    for (int index = 0; index < 3; ++index)
    {
        ludifex::Actor3D bar = world.AddBox({
            .Scale = { 0.15f, 2.2f, 0.15f },
            .Position = { 2.0f + static_cast<float>(index) * 0.9f, 1.1f, -3.5f },
            .Type = ludifex::BodyType::Static,
        });
        bar.SetColor(neon[index]);
        bar.SetEmission(neon[index], 5.0f);
    }

    // --- glass --------------------------------------------------------------------------------

    ludifex::Actor3D glass = world.AddBox({
        .Scale = { 2.2f, 2.6f, 0.08f },
        .Position = { 4.2f, 1.3f, 6.0f },
        .Rotation = ludifex::Quat::FromAxisAngle({ 0.0f, 1.0f, 0.0f }, -0.5f),
        .Type = ludifex::BodyType::Static,
    });
    glass.SetColor(ludifex::Color::FromBytes(170, 220, 255, 70));
    glass.SetRoughness(0.05f);

    // --- the dancer, skinned and swaying --------------------------------------------------------

    ludifex::Actor3D dancer = world.AddModel({
        .Path = dancerPath,
        .Position = { -1.5f, 0.0f, -2.2f },
        .Scale = 1.4f,
        .Type = ludifex::BodyType::Static,
    });
    dancer.SetColor(ludifex::Color::FromBytes(120, 200, 150));
    dancer.SetRoughness(0.4f);
    dancer.Play({ .Name = "sway", .Loop = true });

    // --- two lamps --------------------------------------------------------------------------------

    world.AddPointLight({ .Position = { -5.5f, 2.6f, -1.5f }, .Tint = ludifex::Color::FromBytes(255, 170, 90),
                          .Intensity = 5.0f, .Range = 7.0f });
    world.AddPointLight({ .Position = { 3.0f, 0.8f, -2.0f }, .Tint = ludifex::Color::FromBytes(120, 200, 255),
                          .Intensity = 4.0f, .Range = 5.0f });

    // --- interface ---------------------------------------------------------------------------------

    opane::Element* root = app.GetRoot();

    opane::Panel* panel = root->Add<opane::Panel>();
    panel->Size = opane::Size2::FromOffset(300.0f, 560.0f);
    panel->Position = opane::Position2{ opane::Dim{ 1.0f, -320.0f }, opane::Dim::FromOffset(20.0f) };
    panel->ChildLayout = opane::LayoutMode::Vertical;
    panel->Padding = 14.0f;
    panel->Spacing = 6.0f;

    auto Line = [&](const char* text, bool muted) {
        opane::Label* label = panel->Add<opane::Label>();
        label->Text = text;
        label->Muted = muted;
        label->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };
        return label;
    };

    Line("Graphics quality", false);
    opane::Dropdown* preset = panel->Add<opane::Dropdown>();
    preset->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(32.0f) };
    for (int index = 0; index < 6; ++index)
    {
        preset->Options.push_back(ludifex::GetGraphicsQualityName(static_cast<ludifex::GraphicsQuality>(index)));
    }
    preset->Selected = quality;
    preset->OnChanged = [&](int index) {
        quality = index;
        world.SetGraphicsQuality(static_cast<ludifex::GraphicsQuality>(index));
    };

    auto Switch = [&](const char* text, bool* value) {
        opane::Toggle* toggle = panel->Add<opane::Toggle>();
        toggle->Text = text;
        toggle->On = *value;
        toggle->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
        toggle->OnChanged = [value](bool on) { *value = on; };
        return toggle;
    };

    Line("Ray tracing", false);
    opane::Toggle* shadowsToggle = Switch("Traced shadows", &settings.RayTracing.Shadows);
    opane::Toggle* occlusionToggle = Switch("Traced occlusion", &settings.RayTracing.AmbientOcclusion);
    opane::Toggle* reflectionsToggle = Switch("Traced reflections", &settings.RayTracing.Reflections);
    opane::Toggle* pathToggle = Switch("Path tracing", &settings.PathTracing.Enabled);

    Line("Look", false);
    opane::Toggle* skyToggle = Switch("Sky", &settings.Sky.Enabled);
    bool filmic = settings.Grading.Curve == ludifex::ToneCurve::Filmic;
    opane::Toggle* filmicToggle = Switch("Filmic curve", &filmic);
    opane::Toggle* exposureToggle = Switch("Automatic exposure", &settings.AutoExposure.Enabled);
    opane::Toggle* bloomToggle = Switch("Bloom", &settings.Bloom.Enabled);

    opane::Label* timing = Line("", true);
    opane::Label* resolution = Line("", true);
    opane::Label* tracing = Line("", true);

    opane::Label* keys = root->Add<opane::Label>();
    keys->Text = "1-6 preset   T O R traced shadows, occlusion, reflections   P path tracing   K sky   F filmic   "
                 "E exposure   B bloom   Space orbit";
    keys->Muted = true;
    keys->Align = opane::TextAlign::Center;
    keys->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };
    keys->Position = opane::Position2{ opane::Dim::FromScale(0.0f), opane::Dim{ 1.0f, -34.0f } };

    // --- the tour ------------------------------------------------------------------------------------

    const Mode modes[] = {
        { "Potato", 0, false, false, false, false, 6.0f },
        { "Low", 1, false, false, false, false, 6.0f },
        { "Medium", 2, false, false, false, false, 6.0f },
        { "High", 3, false, false, false, false, 6.0f },
        { "Ultra", 4, false, false, false, false, 6.0f },
        { "Extreme", 5, false, false, false, false, 6.0f },
        { "High + traced shadows", 3, true, false, false, false, 6.0f },
        { "High + traced occlusion", 3, false, true, false, false, 6.0f },
        { "High + traced reflections", 3, false, false, true, false, 6.0f },
        { "High + all traced", 3, true, true, true, false, 6.0f },
        { "Path tracing", 3, false, false, false, true, 10.0f },
    };
    size_t modeIndex = 0;
    float modeTime = 0.0f;
    float modeMilliseconds = 0.0f;
    int modeFrames = 0;
    auto StartMode = [&](size_t index) {
        const Mode& mode = modes[index];
        quality = mode.Quality;
        world.SetGraphicsQuality(static_cast<ludifex::GraphicsQuality>(mode.Quality));
        settings.RayTracing.Shadows = mode.Shadows;
        settings.RayTracing.AmbientOcclusion = mode.Occlusion;
        settings.RayTracing.Reflections = mode.Reflections;
        settings.PathTracing.Enabled = mode.Path;
        modeTime = 0.0f;
        modeMilliseconds = 0.0f;
        modeFrames = 0;
    };
    if (tour)
    {
        StartMode(0);
        std::printf("Touring %zu modes.\n", sizeof(modes) / sizeof(modes[0]));
        std::fflush(stdout);
    }

    bool dancerHeld = false;

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;
    bool orbiting = false;
    float orbitAngle = std::atan2(camera.Position.X, camera.Position.Z);
    float frameMilliseconds = 0.0f;

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        frameMilliseconds = frameMilliseconds * 0.9f + deltaSeconds * 1000.0f * 0.1f;

        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        const opane::Key presetKeys[6] = { opane::Key::Num1, opane::Key::Num2, opane::Key::Num3,
                                           opane::Key::Num4, opane::Key::Num5, opane::Key::Num6 };
        for (int index = 0; index < 6; ++index)
        {
            if (input.WasKeyPressed(presetKeys[index]))
            {
                quality = index;
                world.SetGraphicsQuality(static_cast<ludifex::GraphicsQuality>(index));
            }
        }
        auto Flip = [&](opane::Key key, bool& value) {
            if (input.WasKeyPressed(key))
            {
                value = !value;
            }
        };
        Flip(opane::Key::T, settings.RayTracing.Shadows);
        Flip(opane::Key::O, settings.RayTracing.AmbientOcclusion);
        Flip(opane::Key::R, settings.RayTracing.Reflections);
        Flip(opane::Key::P, settings.PathTracing.Enabled);
        Flip(opane::Key::K, settings.Sky.Enabled);
        Flip(opane::Key::F, filmic);
        Flip(opane::Key::E, settings.AutoExposure.Enabled);
        Flip(opane::Key::B, settings.Bloom.Enabled);
        Flip(opane::Key::Space, orbiting);

        settings.Grading.Curve = filmic ? ludifex::ToneCurve::Filmic : ludifex::ToneCurve::Neutral;
        preset->Selected = quality;
        shadowsToggle->On = settings.RayTracing.Shadows;
        occlusionToggle->On = settings.RayTracing.AmbientOcclusion;
        reflectionsToggle->On = settings.RayTracing.Reflections;
        pathToggle->On = settings.PathTracing.Enabled;
        skyToggle->On = settings.Sky.Enabled;
        filmicToggle->On = filmic;
        exposureToggle->On = settings.AutoExposure.Enabled;
        bloomToggle->On = settings.Bloom.Enabled;

        // Held where it is while the image is path traced, and let go after.
        if (settings.PathTracing.Enabled != dancerHeld)
        {
            dancerHeld = settings.PathTracing.Enabled;
            dancer.Play({ .Name = "sway",
                          .Loop = true,
                          .Speed = dancerHeld ? 0.0f : 1.0f,
                          .StartTime = dancer.GetAnimationTime(),
                          .Fade = 0.0f });
        }

        if (orbiting)
        {
            orbitAngle += deltaSeconds * 0.2f;
            const float distance = std::sqrt(2.5f * 2.5f + 13.0f * 13.0f);
            camera.Position = { std::sin(orbitAngle) * distance, 4.2f, std::cos(orbitAngle) * distance };
        }

        if (tour)
        {
            modeTime += deltaSeconds;
            // The first two seconds of each mode are left out of its timing,
            // while render targets, pipelines, and histories settle.
            if (modeTime > 2.0f)
            {
                modeMilliseconds += deltaSeconds * 1000.0f;
                ++modeFrames;
            }
            if (modeTime >= modes[modeIndex].Seconds)
            {
                std::printf("%-28s %7.2f ms a frame%s%s\n", modes[modeIndex].Name,
                            static_cast<double>(modeMilliseconds / static_cast<float>(std::max(modeFrames, 1))),
                            world.IsRayTracingActive() ? ", traced" : "",
                            settings.PathTracing.Enabled
                                ? (", " + std::to_string(world.GetPathTracedSampleCount()) + " samples gathered").c_str()
                                : "");
                std::fflush(stdout);
                if (++modeIndex >= sizeof(modes) / sizeof(modes[0]))
                {
                    app.Close();
                }
                else
                {
                    StartMode(modeIndex);
                }
            }
        }

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

        char text[160];
        std::snprintf(text, sizeof(text), "%.2f ms a frame   %u draw calls", static_cast<double>(frameMilliseconds),
                      world.GetLastDrawCallCount());
        timing->Text = text;
        const float scale = settings.RenderScale;
        std::snprintf(text, sizeof(text), "drawn at %dx%d (%d%%)", static_cast<int>(std::lround(width * scale)),
                      static_cast<int>(std::lround(height * scale)), static_cast<int>(std::lround(scale * 100.0f)));
        resolution->Text = text;
        if (settings.PathTracing.Enabled)
        {
            std::snprintf(text, sizeof(text), "path traced, %u samples", world.GetPathTracedSampleCount());
        }
        else
        {
            std::snprintf(text, sizeof(text), "%s", world.IsRayTracingActive() ? "rays traced this frame" : "rasterized");
        }
        tracing->Text = text;
    });

    app.Shutdown();
    return 0;
}
