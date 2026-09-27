// Example 05: custom shaders on world actors.
//
// Three materials, each written a different way:
//
//   stripes.hlsl   chooses the colour and keeps the built-in lighting (Shade)
//   hologram.hlsl  ignores the built-in lighting
//   dissolve.hlsl  cuts the object away; alpha becomes coverage, so the edge
//                  is smoothed by multisampling
//
// Each is compiled at startup and recompiled when you save it, so you can edit
// a colour or a constant while the program runs.
//
// Actors with materials still instance, cull, and move with physics as before,
// because only the fragment shader is replaced.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: world shaders",
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

    const std::string directory = EXAMPLE_DIR;

    const ludifex::MaterialId stripes = ludifex::CreateMaterial({
        .ShaderPath = directory + "/stripes.hlsl",
        .Uniforms = {
            { "ColorA", ludifex::Color::FromBytes(236, 170, 70) },
            { "ColorB", ludifex::Color::FromBytes(40, 44, 58) },
            { "Pattern", 6.0f, 0.35f },
        },
    });

    const ludifex::MaterialId hologram = ludifex::CreateMaterial({
        .ShaderPath = directory + "/hologram.hlsl",
        .Uniforms = {
            { "Tint", ludifex::Color::FromBytes(90, 210, 255) },
            { "ScanDensity", 9.0f },
        },
    });

    const ludifex::MaterialId dissolve = ludifex::CreateMaterial({
        .ShaderPath = directory + "/dissolve.hlsl",
        .Uniforms = {
            { "EdgeColor", ludifex::Color::FromBytes(255, 120, 40) },
            { "Amount", 0.35f },
        },
    });

    world.AddGround({ .Width = 30.0f, .Depth = 30.0f, .Name = "Ground" });

    // Three rows, one material each. Kinematic bodies turn at a steady rate so
    // the shaders are seen from every side, and still interpolate smoothly.
    std::vector<ludifex::Actor3D> showcase;

    for (int index = 0; index < 3; ++index)
    {
        const float z = static_cast<float>(index - 1) * 2.6f;

        ludifex::Actor3D box = world.AddBox({
            .Scale = { 1.4f, 1.4f, 1.4f },
            .Position = { -3.2f, 1.6f, z },
            .Type = ludifex::BodyType::Kinematic,
        });
        box.SetMaterial(stripes);
        box.SetAngularVelocity({ 0.0f, 0.6f + index * 0.2f, 0.0f });
        showcase.push_back(box);

        ludifex::Actor3D sphere = world.AddSphere({
            .Radius = 0.8f,
            .Position = { 0.0f, 1.6f, z },
            .Type = ludifex::BodyType::Kinematic,
        });
        sphere.SetMaterial(hologram);
        showcase.push_back(sphere);

        ludifex::Actor3D capsule = world.AddCapsule({
            .Radius = 0.6f,
            .Height = 2.2f,
            .Position = { 3.2f, 1.6f, z },
            .Type = ludifex::BodyType::Kinematic,
        });
        capsule.SetMaterial(dissolve);
        capsule.SetColor(ludifex::Color::FromBytes(120, 128, 150));
        capsule.SetAngularVelocity({ 0.4f, 0.0f, 0.3f });
        showcase.push_back(capsule);
    }

    // A few ordinary actors, to show material and default draws side by side.
    for (int index = 0; index < 6; ++index)
    {
        world.AddBox({
            .Scale = { 0.6f, 0.6f, 0.6f },
            .Position = { -1.5f + static_cast<float>(index) * 0.6f, 5.0f + index * 0.7f, 4.4f },
        });
    }

    // --- interface ---------------------------------------------------------

    opane::Element* root = app.GetRoot();

    opane::Panel* panel = root->Add<opane::Panel>();
    panel->Size = opane::Size2::FromOffset(270.0f, 214.0f);
    panel->Position = opane::Position2::FromOffset(22.0f, 22.0f);
    panel->ChildLayout = opane::LayoutMode::Vertical;
    panel->Padding = 16.0f;
    panel->Spacing = 8.0f;

    opane::Label* title = panel->Add<opane::Label>();
    title->Text = "World shaders";
    title->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    opane::Label* stats = panel->Add<opane::Label>();
    stats->Muted = true;
    stats->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };

    opane::Label* amountLabel = panel->Add<opane::Label>();
    amountLabel->Text = "Dissolve";
    amountLabel->Muted = true;
    amountLabel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(20.0f) };

    opane::Slider* amount = panel->Add<opane::Slider>();
    amount->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    amount->Value = 0.35f;
    amount->OnChanged = [&](float value) {
        ludifex::SetMaterialUniform(dissolve, "Amount", value);
    };

    opane::Checkbox* animate = panel->Add<opane::Checkbox>();
    animate->Text = "Animate dissolve";
    animate->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    opane::Checkbox* materials = panel->Add<opane::Checkbox>();
    materials->Text = "Materials on";
    materials->Checked = true;
    materials->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };
    materials->OnChanged = [&](bool enabled) {
        // Clearing a material returns an actor to the default shader; nothing
        // else about it changes.
        for (size_t index = 0; index < showcase.size(); ++index)
        {
            const ludifex::MaterialId chosen =
                (index % 3 == 0) ? stripes : (index % 3 == 1) ? hologram : dissolve;
            showcase[index].SetMaterial(enabled ? chosen : ludifex::MaterialId{});
        }
    };

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;
    float orbit = 0.4f;
    float animatedTime = 0.0f;

    std::printf("\n  Edit stripes.hlsl, hologram.hlsl, or dissolve.hlsl and save:\n"
                "  the change appears without restarting.\n\n");

    app.Run([&](float deltaSeconds) {
        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
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

        if (animate->Checked)
        {
            animatedTime += deltaSeconds;
            const float value = 0.5f + 0.5f * std::sin(animatedTime * 0.9f);
            amount->Value = value;
            ludifex::SetMaterialUniform(dissolve, "Amount", value);
        }

        orbit += deltaSeconds * 0.12f;

        ludifex::Camera3D& camera = world.GetCamera();
        camera.Position = ludifex::Vec3{ std::sin(orbit) * 12.0f, 6.0f, std::cos(orbit) * 12.0f };
        camera.Target = ludifex::Vec3{ 0.0f, 1.4f, 0.0f };

        world.Update(deltaSeconds);
        world.Render();

        char line[96];
        std::snprintf(line, sizeof(line), "%zu actors   %u draws", world.GetActorCount(),
                      world.GetLastDrawCallCount());
        stats->Text = line;

        if (worldView.IsValid())
        {
            app.GetDrawList().DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }
    });

    app.Shutdown();
    return 0;
}
