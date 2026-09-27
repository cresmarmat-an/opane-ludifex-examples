// Example 12: scale.
//
// Three scenes with large numbers of things:
//
//   Ten thousand objects sharing two meshes, drawn in a handful of calls.
//   Objects behind the camera are not drawn. The readout shows how many were
//   drawn and how many were culled.
//
//   A thousand objects with one custom shader and different values on each.
//   The per-actor values are stored with the transforms, so they are still
//   drawn in one call.
//
//   256 point lights. With clustered lighting, each pixel is shaded only by the
//   lights near it.
//
// Press 1, 2, and 3 to switch scenes. Escape quits.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <functional>
#include <string>
#include <vector>

namespace
{

enum class Scene
{
    TenThousand,
    OneShader,
    ManyLights
};

constexpr int Field = 10000;
constexpr int Shaded = 1000;
constexpr int Lights = 256;

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane + ludifex: scale",
        .Width = 1320,
        .Height = 800,
    });
    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::World3D world = ludifex::CreateWorld3D({
        // Nothing here needs simulating: this is about what the renderer can
        // put on screen, not about what the solver can hold up.
        .PhysicsEnabled = false,
    });
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 400.0f, .Depth = 400.0f });
    world.GetRenderSettings().Fog.Enabled = true;
    world.GetRenderSettings().Fog.Start = 60.0f;
    world.GetRenderSettings().Fog.End = 240.0f;

    const ludifex::MaterialId field = ludifex::CreateMaterial({
        .ShaderPath = std::string(EXAMPLE_DIR) + "/field.hlsl",
        .Uniforms = {
            { "Tint", ludifex::Color::FromBytes(220, 180, 120) },
            { "Phase", 0.0f },
            { "Speed", 2.0f },
        },
    });

    std::vector<ludifex::Actor3D> actors;
    std::vector<ludifex::Light3D> lights;
    Scene scene = Scene::TenThousand;

    auto Clear = [&] {
        for (ludifex::Actor3D& actor : actors)
        {
            actor.Destroy();
        }
        actors.clear();
        for (ludifex::Light3D& light : lights)
        {
            light.Destroy();
        }
        lights.clear();
        world.ApplyPendingChanges();
    };

    // --- the three scenes ------------------------------------------------------

    auto BuildTenThousand = [&] {
        Clear();
        std::mt19937 random(5);
        std::uniform_real_distribution<float> place(-150.0f, 150.0f);
        std::uniform_real_distribution<float> size(0.5f, 2.0f);

        actors.reserve(Field);
        for (int index = 0; index < Field; ++index)
        {
            const float scale = size(random);
            const bool round = (index % 3) == 0;

            ludifex::Actor3D actor =
                round ? world.AddSphere({ .Radius = scale * 0.5f,
                                          .Position = { place(random), scale * 0.5f, place(random) },
                                          .Type = ludifex::BodyType::Static })
                      : world.AddBox({ .Scale = { scale, scale, scale },
                                       .Position = { place(random), scale * 0.5f, place(random) },
                                       .Type = ludifex::BodyType::Static });

            // A colour per object. It is stored in the same instance record as
            // the transform, so it adds no draw calls.
            actor.SetColor(ludifex::Color::FromBytes(static_cast<uint8_t>(90 + (index * 37) % 160),
                                                     static_cast<uint8_t>(110 + (index * 61) % 130),
                                                     static_cast<uint8_t>(130 + (index * 17) % 120)));
            actors.push_back(actor);
        }
        world.ApplyPendingChanges();
    };

    auto BuildOneShader = [&] {
        Clear();
        actors.reserve(Shaded);

        for (int index = 0; index < Shaded; ++index)
        {
            const int row = index / 40;
            const int column = index % 40;

            ludifex::Actor3D actor = world.AddCapsule({
                .Radius = 0.35f,
                .Height = 1.6f,
                .Position = { static_cast<float>(column) * 1.6f - 31.0f, 0.8f,
                              static_cast<float>(row) * 1.6f - 19.0f },
                .Type = ludifex::BodyType::Static,
            });

            actor.SetMaterial(field);

            // Each one its own colour and its own place in the wave. The
            // material is shared; only these values differ.
            const float hue = static_cast<float>(index) / static_cast<float>(Shaded);
            actor.SetUniform("Tint", ludifex::Color{ 0.5f + 0.5f * std::sin(hue * 6.2831853f),
                                                     0.5f + 0.5f * std::sin(hue * 6.2831853f + 2.094f),
                                                     0.5f + 0.5f * std::sin(hue * 6.2831853f + 4.188f),
                                                     1.0f });
            actor.SetUniform("Phase", hue);
            actors.push_back(actor);
        }
        world.ApplyPendingChanges();
    };

    auto BuildManyLights = [&] {
        Clear();

        // A hall of pillars, so the lights have something to fall on.
        for (int index = 0; index < 400; ++index)
        {
            const int row = index / 20;
            const int column = index % 20;
            ludifex::Actor3D pillar = world.AddBox({
                .Scale = { 1.2f, 6.0f, 1.2f },
                .Position = { static_cast<float>(column) * 6.0f - 57.0f, 3.0f,
                              static_cast<float>(row) * 6.0f - 57.0f },
                .Type = ludifex::BodyType::Static,
            });
            pillar.SetColor(ludifex::Color::FromBytes(150, 150, 158));
            actors.push_back(pillar);
        }

        std::mt19937 random(9);
        std::uniform_real_distribution<float> place(-58.0f, 58.0f);
        std::uniform_real_distribution<float> hue(0.0f, 6.2831853f);

        for (int index = 0; index < Lights; ++index)
        {
            const float angle = hue(random);
            lights.push_back(world.AddPointLight({
                .Position = { place(random), 2.5f, place(random) },
                .Tint = ludifex::Color{ 0.5f + 0.5f * std::sin(angle),
                                        0.5f + 0.5f * std::sin(angle + 2.094f),
                                        0.5f + 0.5f * std::sin(angle + 4.188f), 1.0f },
                .Intensity = 8.0f,
                .Range = 9.0f,
            }));
        }
        world.ApplyPendingChanges();
    };

    BuildTenThousand();

    // --- interface -------------------------------------------------------------

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;

    opane::Panel* sidebar = root->Add<opane::Panel>();
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(300.0f), opane::Dim::FromScale(1.0f) };
    sidebar->ChildLayout = opane::LayoutMode::Vertical;
    sidebar->Padding = 18.0f;
    sidebar->Spacing = 8.0f;
    sidebar->CornerRadius = 0.0f;
    sidebar->DrawBorder = false;

    auto AddLabel = [&](const std::string& text, bool muted, float height = 24.0f) {
        opane::Label* label = sidebar->Add<opane::Label>();
        label->Text = text;
        label->Muted = muted;
        label->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(height) };
        return label;
    };

    opane::Label* title = AddLabel("Ten thousand objects", false, 28.0f);
    opane::Label* drawn = AddLabel("", false);
    opane::Label* calls = AddLabel("", true);
    opane::Label* culled = AddLabel("", true);
    opane::Label* timing = AddLabel("", true);
    opane::Label* detail = AddLabel("", true);

    auto AddSceneButton = [&](const char* text, Scene which, const std::function<void()>& build) {
        opane::Button* button = sidebar->Add<opane::Button>();
        button->Text = text;
        button->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };
        button->OnClick = [&, which, build] {
            scene = which;
            build();
        };
        return button;
    };

    AddSceneButton("1. ten thousand objects", Scene::TenThousand, BuildTenThousand);
    AddSceneButton("2. one shader, a thousand actors", Scene::OneShader, BuildOneShader);
    AddSceneButton("3. two hundred and fifty-six lights", Scene::ManyLights, BuildManyLights);

    opane::Checkbox* useDetail = sidebar->Add<opane::Checkbox>();
    useDetail->Text = "Simplify distant shapes";
    useDetail->Checked = true;
    useDetail->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    useDetail->OnChanged = [&](bool enabled) { world.GetRenderSettings().Detail.Enabled = enabled; };

    opane::Checkbox* orbit = sidebar->Add<opane::Checkbox>();
    orbit->Text = "Fly the camera";
    orbit->Checked = true;
    orbit->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    AddLabel("1 / 2 / 3 switch scenes", true);

    opane::Viewport* viewport = root->Add<opane::Viewport>();
    viewport->Size = opane::Size2{ opane::Dim{ 1.0f, -300.0f }, opane::Dim::FromScale(1.0f) };
    viewport->SetWorld(world);

    float clock = 0.0f;

    // A running average, because a single frame's time varies too much to
    // read.
    float average = 16.0f;
    float worst = 0.0f;
    float sinceWorstReset = 0.0f;

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
        if (input.WasKeyPressed(opane::Key::Num1))
        {
            scene = Scene::TenThousand;
            BuildTenThousand();
        }
        if (input.WasKeyPressed(opane::Key::Num2))
        {
            scene = Scene::OneShader;
            BuildOneShader();
        }
        if (input.WasKeyPressed(opane::Key::Num3))
        {
            scene = Scene::ManyLights;
            BuildManyLights();
        }

        if (orbit->Checked)
        {
            clock += deltaSeconds;
        }

        ludifex::Camera3D& camera = world.GetCamera();
        switch (scene)
        {
            case Scene::TenThousand:
                camera.Position = { std::sin(clock * 0.15f) * 90.0f, 22.0f,
                                    std::cos(clock * 0.15f) * 90.0f };
                camera.Target = { 0.0f, 2.0f, 0.0f };
                title->Text = "Ten thousand objects";
                break;

            case Scene::OneShader:
                camera.Position = { std::sin(clock * 0.2f) * 44.0f, 16.0f,
                                    std::cos(clock * 0.2f) * 44.0f };
                camera.Target = { 0.0f, 1.0f, 0.0f };
                title->Text = "One shader, a thousand actors";
                break;

            case Scene::ManyLights:
                camera.Position = { std::sin(clock * 0.12f) * 52.0f, 9.0f,
                                    std::cos(clock * 0.12f) * 52.0f };
                camera.Target = { 0.0f, 2.0f, 0.0f };
                title->Text = "Two hundred and fifty-six lights";
                break;
        }

        // Timing, smoothed, with the worst frame of the last two seconds kept
        // beside it: a steady frame rate is one where those two stay close.
        const float milliseconds = deltaSeconds * 1000.0f;
        average += (milliseconds - average) * 0.05f;
        worst = std::max(worst, milliseconds);
        sinceWorstReset += deltaSeconds;
        if (sinceWorstReset > 2.0f)
        {
            sinceWorstReset = 0.0f;
            worst = milliseconds;
        }

        char line[128];
        std::snprintf(line, sizeof(line), "%zu actors, %u drawn", world.GetActorCount(),
                      world.GetDrawnInstanceCount());
        drawn->Text = line;
        std::snprintf(line, sizeof(line), "%u draw calls, biggest %u objects in one",
                      world.GetLastDrawCallCount(), world.GetLargestBatchSize());
        calls->Text = line;
        std::snprintf(line, sizeof(line), "%u culled before submission", world.GetCulledCount());
        culled->Text = line;
        std::snprintf(line, sizeof(line), "%.2f ms average, %.2f ms worst", static_cast<double>(average),
                      static_cast<double>(worst));
        timing->Text = line;
        std::snprintf(line, sizeof(line), "%d lights in the world", static_cast<int>(lights.size()));
        detail->Text = line;
    });

    app.Shutdown();
    return 0;
}
