// Example 03: a world inside an interface.
//
// A sidebar of widgets on the left and a 3D world in a viewport on the right.
// Neither library includes the other's headers to do this.
//
// The viewport does not know what a world is. ludifex renders into a texture,
// opane draws that texture as an element, and the viewport converts pointer
// positions into the view coordinates PickFromView expects.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane + ludifex: viewport",
        .Width = 1280,
        .Height = 760,
    });

    if (!app.IsValid())
    {
        return 1;
    }

    // No AdoptHost: the world finds opane's device through the shared handshake.

    ludifex::World3D world = ludifex::CreateWorld3D();
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 40.0f, .Depth = 40.0f, .Name = "Ground" });

    std::vector<ludifex::Actor3D> objects;

    auto SpawnScene = [&] {
        for (ludifex::Actor3D& actor : objects)
        {
            actor.Destroy();
        }
        objects.clear();
        world.ApplyPendingChanges();

        for (int index = 0; index < 6; ++index)
        {
            const float angle = static_cast<float>(index) * 1.047f;
            const float radius = 2.2f;

            ludifex::Actor3D box = world.AddBox({
                .Scale = { 0.9f, 0.9f, 0.9f },
                .Position = { std::cos(angle) * radius, 3.0f + static_cast<float>(index) * 1.1f,
                              std::sin(angle) * radius },
                .Restitution = 0.2f,
                .Name = "Box " + std::to_string(index),
            });
            objects.push_back(box);
        }

        ludifex::Actor3D ball = world.AddSphere({
            .Radius = 0.7f,
            .Position = { 0.0f, 9.0f, 0.0f },
            .Restitution = 0.5f,
            .Name = "Ball",
        });
        ball.SetColor(ludifex::Color::FromBytes(120, 200, 255));
        objects.push_back(ball);
    };

    SpawnScene();

    // --- interface ---------------------------------------------------------

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;

    opane::Panel* sidebar = root->Add<opane::Panel>();
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(280.0f), opane::Dim::FromScale(1.0f) };
    sidebar->ChildLayout = opane::LayoutMode::Vertical;
    sidebar->Padding = 18.0f;
    sidebar->Spacing = 10.0f;
    sidebar->CornerRadius = 0.0f;
    sidebar->DrawBorder = false;

    opane::Label* title = sidebar->Add<opane::Label>();
    title->Text = "World";
    title->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };

    opane::Label* status = sidebar->Add<opane::Label>();
    status->Text = "Click an object in the view";
    status->Muted = true;
    status->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    opane::Button* pause = sidebar->Add<opane::Button>();
    pause->Text = "Pause physics";
    pause->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };
    pause->OnClick = [&] {
        if (world.IsPhysicsRunning())
        {
            world.StopPhysics();
            pause->Text = "Resume physics";
        }
        else
        {
            world.StartPhysics();
            pause->Text = "Pause physics";
        }
    };

    opane::Button* respawn = sidebar->Add<opane::Button>();
    respawn->Text = "Respawn";
    respawn->Accent = true;
    respawn->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };
    respawn->OnClick = [&] { SpawnScene(); };

    opane::Label* gravityLabel = sidebar->Add<opane::Label>();
    gravityLabel->Text = "Gravity";
    gravityLabel->Muted = true;
    gravityLabel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };

    opane::Slider* gravity = sidebar->Add<opane::Slider>();
    gravity->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    gravity->Minimum = -25.0f;
    gravity->Maximum = 0.0f;
    gravity->Value = -10.0f;
    gravity->OnChanged = [&](float value) { world.SetGravity({ 0.0f, value, 0.0f }); };

    opane::Label* aaLabel = sidebar->Add<opane::Label>();
    aaLabel->Text = "Anti-aliasing: 4x";
    aaLabel->Muted = true;
    aaLabel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };

    opane::Checkbox* multisample = sidebar->Add<opane::Checkbox>();
    multisample->Text = "Multisampling";
    multisample->Checked = true;
    multisample->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    multisample->OnChanged = [&](bool enabled) {
        world.GetRenderSettings().Mode =
            enabled ? ludifex::AntiAliasing::Balanced : ludifex::AntiAliasing::Off;
        aaLabel->Text = enabled ? "Anti-aliasing: 4x" : "Anti-aliasing: off";
    };

    opane::Checkbox* orbit = sidebar->Add<opane::Checkbox>();
    orbit->Text = "Orbit camera";
    orbit->Checked = true;
    orbit->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    // The viewport fills whatever the sidebar leaves, and SetWorld hands it the
    // world: from here on it sizes the world's image to itself, steps the world
    // every frame, draws it, and shows the result. Resizing the window or the
    // sidebar resizes the world's image with it.
    opane::Viewport* viewport = root->Add<opane::Viewport>();
    viewport->Size = opane::Size2{ opane::Dim{ 1.0f, -280.0f }, opane::Dim::FromScale(1.0f) };
    viewport->SetWorld(world);

    float cameraAngle = 0.6f;

    // Pointer events arrive already converted to 0..1 across the viewport,
    // the range PickFromView expects.
    viewport->OnViewEvent = [&](const opane::Event& viewEvent) {
        if (viewEvent.Type != opane::EventType::PointerDown)
        {
            return;
        }

        const ludifex::RayHit hit = world.PickFromView(viewEvent.Position.X, viewEvent.Position.Y);
        if (!hit.Hit)
        {
            status->Text = "Nothing there";
            return;
        }

        ludifex::Actor3D picked = hit.Actor;
        picked.ApplyImpulse({ 0.0f, 6.0f, 0.0f });
        picked.SetColor(ludifex::Color::FromBytes(255, 240, 190));

        char line[96];
        std::snprintf(line, sizeof(line), "%s at %.1f m", picked.GetName().c_str(), hit.Distance);
        status->Text = line;
    };

    app.Run([&](float deltaSeconds) {
        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        if (orbit->Checked)
        {
            cameraAngle += deltaSeconds * 0.25f;
        }

        ludifex::Camera3D& camera = world.GetCamera();
        camera.Position = ludifex::Vec3{ std::sin(cameraAngle) * 13.0f, 7.0f,
                                         std::cos(cameraAngle) * 13.0f };
        camera.Target = ludifex::Vec3{ 0.0f, 1.5f, 0.0f };

        // Ease impact flashes back toward each actor's resting tint.
        world.ForEachActor([&](ludifex::Actor3D& actor) {
            if (actor.GetName() == "Ground" || actor.GetName() == "Ball")
            {
                return;
            }
            const ludifex::Color current = actor.GetColor();
            const ludifex::Color resting{ 0.89f, 0.64f, 0.31f, 1.0f };
            const float rate = std::min(1.0f, deltaSeconds * 3.0f);
            actor.SetColor(ludifex::Color{ current.R + (resting.R - current.R) * rate,
                                           current.G + (resting.G - current.G) * rate,
                                           current.B + (resting.B - current.B) * rate, 1.0f });
        });
    });

    app.Shutdown();
    return 0;
}
