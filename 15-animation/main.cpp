// Example 15: skeletons and blending between clips.
//
// A row of columns bound to a chain of joints, each playing the same clips from
// a different starting point. One clip sways and loops; the other curls forward
// once and stops. Choosing a clip crossfades every column into it.
//
// The slider sets the crossfade time. Set it to zero to see the hard cut that a
// crossfade avoids.
//
// Behind the row are two more models. A pair of columns share one skeleton
// through two skins that list the joints in opposite orders; they only bend
// together if each is posed through its own skin. And two panels with morph
// targets: the left one is driven by a clip, and the right one by a slider.
//
// The models are generated at startup.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/GltfMorph.h"
#include "../common/GltfSkinned.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // --still keeps the camera in front of everything instead of circling, for
    // taking comparable screenshots.
    bool still = false;
    for (int index = 1; index < argc; ++index)
    {
        still = still || std::string(argv[index]) == "--still";
    }

    opane::App app = opane::StartApp({
        .Title = "ludifex: skeletons and blending",
        .Width = 1280,
        .Height = 800,
    });
    if (!app.IsValid())
    {
        return 1;
    }

    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::string modelPath = (directory / "ludifex-column.gltf").string();
    const std::string pairPath = (directory / "ludifex-column-pair.gltf").string();
    const std::string panelPath = (directory / "ludifex-panels.gltf").string();
    if (!examples::WriteGltfSkinnedColumn(modelPath, 7, 0.34f, 0.15f) ||
        !examples::WriteGltfSkinnedColumn(pairPath, 7, 0.34f, 0.15f, true) ||
        !examples::WriteGltfMorphPanels(panelPath))
    {
        app.Shutdown();
        return 1;
    }

    ludifex::World3D world = ludifex::CreateWorld3D({ .Gravity = { 0.0f, 0.0f, 0.0f } });
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 40.0f, .Depth = 40.0f });

    ludifex::RenderSettings settings = world.GetRenderSettings();
    settings.Light.Direction = { -0.4f, -0.8f, -0.45f };
    settings.Light.Intensity = 2.6f;
    world.SetRenderSettings(settings);

    // --- a row of them, each at a different point in the same clip -----------
    //
    // One model, one skeleton, and one set of clips; each column only stores
    // where it is in them, so many characters cost one pose each, not a copy
    // of the animation each.

    constexpr int Columns = 7;
    std::vector<ludifex::Actor3D> columns;

    for (int index = 0; index < Columns; ++index)
    {
        const float across = (static_cast<float>(index) - (Columns - 1) * 0.5f) * 0.9f;
        const float shade = 0.35f + 0.09f * static_cast<float>(index);

        ludifex::Actor3D column = world.AddModel({
            .Path = modelPath,
            .Position = { across, 0.0f, 0.0f },
            .Type = ludifex::BodyType::Static,
        });
        column.SetColor({ shade, 0.62f, 0.86f - shade * 0.4f, 1.0f });
        columns.push_back(column);
    }

    // The pair on two skins, in front of the row on the left, playing along
    // with the rest.
    ludifex::Actor3D pair = world.AddModel({
        .Path = pairPath,
        .Position = { -3.6f, 0.0f, 2.0f },
        .Type = ludifex::BodyType::Static,
    });
    pair.SetColor({ 0.92f, 0.5f, 0.42f, 1.0f });

    // The panels, in front of the row on the right, facing the camera's
    // starting side.
    ludifex::Actor3D panels = world.AddModel({
        .Path = panelPath,
        .Position = { 3.4f, 0.1f, 2.0f },
        .Type = ludifex::BodyType::Static,
    });
    panels.SetColor({ 0.85f, 0.85f, 0.9f, 1.0f });
    panels.SetRoughness(0.45f);

    // A marker parented to nothing, put on the top joint of the middle column
    // every frame: a sword in a hand, in the simplest form it takes.
    ludifex::Actor3D marker = world.AddSphere({
        .Radius = 0.09f,
        .Type = ludifex::BodyType::Static,
    });
    marker.SetColor({ 1.0f, 0.78f, 0.25f, 1.0f });

    world.ApplyPendingChanges();

    // Started at staggered points so the row moves like a wave instead of in
    // unison.
    for (int index = 0; index < Columns; ++index)
    {
        columns[static_cast<size_t>(index)].Play({
            .Name = "sway",
            .StartTime = static_cast<float>(index) * 0.12f,
            .Fade = 0.0f,
        });
    }

    const int topJoint = columns[0].GetJointCount() - 1;

    // Joined to the row after it has started, so it plays whatever the row
    // is told to from here on.
    columns.push_back(pair);
    pair.Play({ .Name = "sway", .Fade = 0.0f });

    panels.Play({ .Name = "breathe", .Loop = true, .Fade = 0.0f });

    // --- the interface -------------------------------------------------------

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;

    opane::Panel* sidebar = root->Add<opane::Panel>();
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(300.0f), opane::Dim::FromScale(1.0f) };
    sidebar->ChildLayout = opane::LayoutMode::Vertical;
    sidebar->Padding = 14.0f;
    sidebar->Spacing = 8.0f;

    auto AddLabel = [&](const std::string& text, bool quiet) {
        opane::Label* label = sidebar->Add<opane::Label>();
        label->Text = text;
        label->Muted = quiet;
        label->Size = opane::Size2{ opane::Dim::FromScale(1.0f),
                                    opane::Dim::FromOffset(quiet ? 22.0f : 28.0f) };
        return label;
    };

    AddLabel("Skeletons", false);
    AddLabel(std::to_string(columns[0].GetJointCount()) + " joints, " +
                 std::to_string(columns[0].GetAnimationCount()) + " clips",
             true);

    opane::Label* playing = AddLabel("", true);
    opane::Label* where = AddLabel("", true);
    opane::Label* grip = AddLabel("", true);

    float fadeSeconds = 0.25f;

    opane::Label* fadeLabel = AddLabel("", true);
    opane::Slider* fade = sidebar->Add<opane::Slider>();
    fade->Minimum = 0.0f;
    fade->Maximum = 1.0f;
    fade->Value = fadeSeconds;
    fade->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    fade->OnChanged = [&](float value) { fadeSeconds = value; };

    std::string current = "sway";

    auto PlayEverywhere = [&](const char* name, bool loop) {
        current = name;
        for (int index = 0; index < static_cast<int>(columns.size()); ++index)
        {
            columns[static_cast<size_t>(index)].Play({
                .Name = name,
                .Loop = loop,
                .StartTime = static_cast<float>(index) * 0.12f,
                .Fade = fadeSeconds,
            });
        }
    };

    opane::Button* swayButton = sidebar->Add<opane::Button>();
    swayButton->Text = "Sway (loops)";
    swayButton->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };
    swayButton->OnClick = [&]() { PlayEverywhere("sway", true); };

    opane::Button* curlButton = sidebar->Add<opane::Button>();
    curlButton->Text = "Curl (once)";
    curlButton->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };
    curlButton->OnClick = [&]() { PlayEverywhere("curl", false); };

    opane::Button* stopButton = sidebar->Add<opane::Button>();
    stopButton->Text = "Stop";
    stopButton->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };
    stopButton->OnClick = [&]() {
        current = "nothing";
        for (ludifex::Actor3D& column : columns)
        {
            column.StopAnimation(fadeSeconds);
        }
    };

    AddLabel("Morph targets", false);
    AddLabel(std::to_string(panels.GetMorphTargetCount()) + " shapes on two panels", true);
    opane::Label* shapes = AddLabel("", true);

    opane::Label* waveLabel = AddLabel("Wave", true);
    opane::Slider* wave = sidebar->Add<opane::Slider>();
    wave->Minimum = -1.0f;
    wave->Maximum = 1.5f;
    wave->Value = 0.6f;
    wave->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    wave->OnChanged = [&](float value) { panels.SetMorphWeight("Wave", value); };
    panels.SetMorphWeight("Wave", wave->Value);

    AddLabel("", true);
    AddLabel("1 sways, 2 curls, 3 stops", true);
    AddLabel("", true);
    AddLabel("Set the fade to zero and press", true);
    AddLabel("a clip: the cut is the thing", true);
    AddLabel("a fade exists to avoid.", true);

    opane::Viewport* viewport = root->Add<opane::Viewport>();
    viewport->Size = opane::Size2{ opane::Dim{ 1.0f, -300.0f }, opane::Dim::FromScale(1.0f) };
    viewport->SetWorld(world);

    float angle = 0.7f;

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
        if (input.WasKeyPressed(opane::Key::Num1))
        {
            PlayEverywhere("sway", true);
        }
        if (input.WasKeyPressed(opane::Key::Num2))
        {
            PlayEverywhere("curl", false);
        }
        if (input.WasKeyPressed(opane::Key::Num3))
        {
            stopButton->OnClick();
        }

        if (still)
        {
            world.SetCamera({ .Position = { 0.0f, 3.2f, 10.5f }, .Target = { 0.0f, 1.0f, 0.8f } });
        }
        else
        {
            angle += deltaSeconds * 0.22f;
            world.SetCamera({
                .Position = { std::sin(angle) * 7.5f, 2.8f, std::cos(angle) * 7.5f },
                .Target = { 0.0f, 1.0f, 0.5f },
            });
        }

        world.Update(deltaSeconds);

        // The joint is read after the update, so the marker sits where the
        // hand is this frame rather than where it was last frame.
        const ludifex::Transform3 top = columns[Columns / 2].GetJointTransform(topJoint);
        marker.SetPosition(top.Position);
        marker.SetRotation(top.Rotation);

        char line[128];

        playing->Text = "Playing: " + current;

        std::snprintf(line, sizeof(line), "%.2fs into the clip",
                      static_cast<double>(columns[0].GetAnimationTime()));
        where->Text = columns[0].IsAnimating() ? line : "not playing";

        std::snprintf(line, sizeof(line), "Top joint at %.2f, %.2f, %.2f",
                      static_cast<double>(top.Position.X), static_cast<double>(top.Position.Y),
                      static_cast<double>(top.Position.Z));
        grip->Text = line;

        std::snprintf(line, sizeof(line), "Crossfade %.2fs", static_cast<double>(fadeSeconds));
        fadeLabel->Text = line;

        std::snprintf(line, sizeof(line), "Bulge %.2f, stretch %.2f", static_cast<double>(panels.GetMorphWeight(0)),
                      static_cast<double>(panels.GetMorphWeight(1)));
        shapes->Text = line;

        std::snprintf(line, sizeof(line), "Wave %.2f", static_cast<double>(panels.GetMorphWeight(2)));
        waveLabel->Text = line;
    });

    app.Shutdown();
    return 0;
}
