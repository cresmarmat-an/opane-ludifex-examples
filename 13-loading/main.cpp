// Example 13: loading in the background.
//
// Creates sixty model actors from twelve generated glTF files, either in the
// foreground or in the background, and shows how long the call took and the
// longest frame since.
//
//   In the foreground, the call returns when every file has been read, and the
//   window stops responding until then.
//
//   In the background, every actor appears at once as a plain box and takes
//   its model's shape when the file has been read, while the window keeps
//   running.
//
// The model files are generated at startup.

#include "../common/Gltf.h"

#include <SDL3/SDL.h>

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

constexpr int Models = 60;

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane + ludifex: loading",
        .Width = 1320,
        .Height = 800,
    });
    if (!app.IsValid())
    {
        return 1;
    }

    // --- the files -------------------------------------------------------------
    //
    // Twelve distinct models, each a few thousand triangles, written once and
    // then loaded again and again from disk.

    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "ludifex-loading";
    std::filesystem::create_directories(directory);

    std::vector<std::string> paths;
    for (int index = 0; index < 12; ++index)
    {
        const std::string path = (directory / ("shape-" + std::to_string(index) + ".gltf")).string();
        examples::WriteGltfSphere(path, 70 + index * 2, 0.9f);
        paths.push_back(path);
    }

    ludifex::World3D world = ludifex::CreateWorld3D({ .Gravity = { 0.0f, -10.0f, 0.0f } });
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 120.0f, .Depth = 120.0f });

    std::vector<ludifex::Actor3D> loaded;
    float lastLoadMilliseconds = 0.0f;
    bool lastWasBackground = false;

    auto Clear = [&] {
        for (ludifex::Actor3D& actor : loaded)
        {
            actor.Destroy();
        }
        loaded.clear();
        world.ApplyPendingChanges();
    };

    auto Build = [&](bool background) {
        Clear();

        const uint64_t started = SDL_GetPerformanceCounter();

        for (int index = 0; index < Models; ++index)
        {
            const int row = index / 10;
            const int column = index % 10;

            loaded.push_back(world.AddModel({
                .Path = paths[static_cast<size_t>(index) % paths.size()],
                .LoadInBackground = background,
                .Position = { static_cast<float>(column) * 4.0f - 18.0f, 1.2f,
                              static_cast<float>(row) * 4.0f - 10.0f },
                .Scale = 1.0f,
                .Type = ludifex::BodyType::Static,
                .Name = "Model " + std::to_string(index),
            }));
        }
        world.ApplyPendingChanges();

        const uint64_t ended = SDL_GetPerformanceCounter();
        lastLoadMilliseconds = static_cast<float>(static_cast<double>(ended - started) * 1000.0 /
                                                  static_cast<double>(SDL_GetPerformanceFrequency()));
        lastWasBackground = background;
    };

    // --- interface -------------------------------------------------------------

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;

    opane::Panel* sidebar = root->Add<opane::Panel>();
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(320.0f), opane::Dim::FromScale(1.0f) };
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

    AddLabel("Sixty models", false, 28.0f);
    opane::Label* call = AddLabel("Nothing loaded yet", false);
    opane::Label* pending = AddLabel("", true);
    opane::Label* frame = AddLabel("", true);
    opane::Label* worstLabel = AddLabel("", true);

    opane::Button* foreground = sidebar->Add<opane::Button>();
    foreground->Text = "Load in the foreground";
    foreground->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };

    opane::Button* background = sidebar->Add<opane::Button>();
    background->Text = "Load in the background";
    background->Accent = true;
    background->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };

    AddLabel("The worst frame since loading began is what tells the two apart.", true, 48.0f);

    opane::Viewport* viewport = root->Add<opane::Viewport>();
    viewport->Size = opane::Size2{ opane::Dim{ 1.0f, -320.0f }, opane::Dim::FromScale(1.0f) };
    viewport->SetWorld(world);

    float worst = 0.0f;
    bool measuring = false;
    float clock = 0.0f;

    foreground->OnClick = [&] {
        worst = 0.0f;
        measuring = true;
        Build(false);
    };
    background->OnClick = [&] {
        worst = 0.0f;
        measuring = true;
        Build(true);
    };

    app.Run([&](float deltaSeconds) {
        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        clock += deltaSeconds;

        ludifex::Camera3D& camera = world.GetCamera();
        camera.Position = { std::sin(clock * 0.25f) * 34.0f, 16.0f, std::cos(clock * 0.25f) * 34.0f };
        camera.Target = { 0.0f, 1.0f, 0.0f };

        const float milliseconds = deltaSeconds * 1000.0f;
        if (measuring)
        {
            worst = std::max(worst, milliseconds);
        }

        char line[128];
        if (lastLoadMilliseconds > 0.0f)
        {
            std::snprintf(line, sizeof(line), "%s: the call took %.1f ms",
                          lastWasBackground ? "In the background" : "In the foreground",
                          static_cast<double>(lastLoadMilliseconds));
            call->Text = line;
        }

        std::snprintf(line, sizeof(line), "%zu still parsing", world.GetPendingLoadCount());
        pending->Text = line;
        std::snprintf(line, sizeof(line), "this frame %.1f ms", static_cast<double>(milliseconds));
        frame->Text = line;
        std::snprintf(line, sizeof(line), "worst frame since %.1f ms", static_cast<double>(worst));
        worstLabel->Text = line;

    });

    app.Shutdown();
    return 0;
}
