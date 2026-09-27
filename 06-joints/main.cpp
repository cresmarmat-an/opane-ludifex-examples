// Example 06: joints.
//
// Four machines, one for each joint type:
//
//   a rope bridge   hinges, joining plank to plank
//   a windmill      a hinge with a motor
//   a lift          a slider with limits and a motor that reverses at each end
//   a pendulum      distance joints in a chain
//
// Joints are given in world space, as a pivot and an axis. The library converts
// them to the body-local frames the solver uses.
//
// Controls:
//   Click   drop a ball onto whatever is under the pointer
//   Space   pause and resume physics
//   R       rebuild everything
//   Escape  quit

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

constexpr int PlankCount = 11;

struct Machines
{
    std::vector<ludifex::Actor3D> Actors;
    ludifex::Joint3D Windmill;
    ludifex::Joint3D Lift;
};

Machines Build(ludifex::World3D& world)
{
    Machines machines;

    auto Remember = [&](ludifex::Actor3D actor) {
        machines.Actors.push_back(actor);
        return actor;
    };

    // --- a rope bridge -----------------------------------------------------
    //
    // Planks chained by hinges, with the two ends pinned to static posts. The
    // hinge axis runs along Z, so the span sags downward in the XY plane.

    const float plankWidth = 0.9f;
    const float spanStart = -5.0f;

    ludifex::Actor3D leftPost = Remember(world.AddBox({
        .Scale = { 0.6f, 2.0f, 1.4f },
        .Position = { spanStart - 0.6f, 4.0f, -3.0f },
        .Type = ludifex::BodyType::Static,
    }));
    leftPost.SetColor(ludifex::Color::FromBytes(70, 78, 92));

    ludifex::Actor3D previous = leftPost;
    float previousEdge = spanStart - 0.3f;

    for (int index = 0; index < PlankCount; ++index)
    {
        const float centre = spanStart + plankWidth * (static_cast<float>(index) + 0.5f);

        ludifex::Actor3D plank = Remember(world.AddBox({
            .Scale = { plankWidth * 0.94f, 0.16f, 1.2f },
            .Position = { centre, 4.6f, -3.0f },
            .Density = 0.7f,
        }));
        plank.SetColor(ludifex::Color::FromBytes(198, 150, 92));

        world.AddHinge({
            .BodyA = previous,
            .BodyB = plank,
            .Anchor = { previousEdge, 4.6f, -3.0f },
            .Axis = { 0.0f, 0.0f, 1.0f },
        });

        previous = plank;
        previousEdge = centre + plankWidth * 0.5f;
    }

    ludifex::Actor3D rightPost = Remember(world.AddBox({
        .Scale = { 0.6f, 2.0f, 1.4f },
        .Position = { previousEdge + 0.3f, 4.0f, -3.0f },
        .Type = ludifex::BodyType::Static,
    }));
    rightPost.SetColor(ludifex::Color::FromBytes(70, 78, 92));

    world.AddHinge({
        .BodyA = previous,
        .BodyB = rightPost,
        .Anchor = { previousEdge, 4.6f, -3.0f },
        .Axis = { 0.0f, 0.0f, 1.0f },
    });

    // --- a windmill --------------------------------------------------------

    ludifex::Actor3D hub = Remember(world.AddBox({
        .Scale = { 0.5f, 0.5f, 0.5f },
        .Position = { -5.0f, 4.0f, 2.0f },
        .Type = ludifex::BodyType::Static,
    }));
    hub.SetColor(ludifex::Color::FromBytes(70, 78, 92));

    ludifex::Actor3D blades = Remember(world.AddBox({
        .Scale = { 5.0f, 0.5f, 0.3f },
        .Position = { -5.0f, 4.0f, 2.0f },
        .Density = 0.5f,
    }));
    blades.SetColor(ludifex::Color::FromBytes(120, 190, 230));

    machines.Windmill = world.AddHinge({
        .BodyA = hub,
        .BodyB = blades,
        .Anchor = { -5.0f, 4.0f, 2.0f },
        .Axis = { 0.0f, 0.0f, 1.0f },
        .EnableMotor = true,
        .MotorSpeed = 2.0f,
        .MaxMotorTorque = 4000.0f,
    });

    // --- a lift ------------------------------------------------------------
    //
    // A slider with limits. The motor reverses when the platform reaches
    // either end, so it moves up and down.

    ludifex::Actor3D rail = Remember(world.AddBox({
        .Scale = { 0.4f, 0.4f, 0.4f },
        .Position = { 3.0f, 1.0f, 2.5f },
        .Type = ludifex::BodyType::Static,
    }));
    rail.SetColor(ludifex::Color::FromBytes(70, 78, 92));

    ludifex::Actor3D platform = Remember(world.AddBox({
        .Scale = { 2.4f, 0.3f, 2.4f },
        .Position = { 3.0f, 1.0f, 2.5f },
        .Density = 2.0f,
    }));
    platform.SetColor(ludifex::Color::FromBytes(150, 200, 150));

    machines.Lift = world.AddSlider({
        .BodyA = rail,
        .BodyB = platform,
        .Anchor = { 3.0f, 1.0f, 2.5f },
        .Axis = { 0.0f, 1.0f, 0.0f },
        .EnableLimit = true,
        .LowerTranslation = 0.0f,
        .UpperTranslation = 4.0f,
        .EnableMotor = true,
        .MotorSpeed = 1.5f,
        .MaxMotorForce = 8000.0f,
    });

    // --- a pendulum chain --------------------------------------------------

    ludifex::Actor3D ceiling = Remember(world.AddBox({
        .Scale = { 0.4f, 0.4f, 0.4f },
        .Position = { 8.0f, 7.5f, 0.0f },
        .Type = ludifex::BodyType::Static,
    }));
    ceiling.SetColor(ludifex::Color::FromBytes(70, 78, 92));

    ludifex::Actor3D link = ceiling;
    float height = 7.5f;

    for (int index = 0; index < 5; ++index)
    {
        const float nextHeight = height - 0.9f;

        ludifex::Actor3D bead = Remember(world.AddSphere({
            .Radius = 0.28f,
            .Position = { 8.0f + static_cast<float>(index) * 0.25f, nextHeight, 0.0f },
            .Density = 1.5f,
        }));
        bead.SetColor(ludifex::Color::FromBytes(232, 130, 150));

        world.AddDistanceJoint({
            .BodyA = link,
            .BodyB = bead,
            .AnchorA = { 8.0f, height, 0.0f },
            .AnchorB = bead.GetPosition(),
        });

        link = bead;
        height = nextHeight;
    }

    return machines;
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: joints",
        .Width = 1280,
        .Height = 760,
    });

    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::AdoptHost({ app.GetGpuDevice(), app.GetWindow() });

    ludifex::World3D world = ludifex::CreateWorld3D({ .WorkerCount = 0 });
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 60.0f, .Depth = 60.0f, .Name = "Ground" });

    Machines machines = Build(world);
    std::vector<ludifex::Actor3D> balls;

    ludifex::Camera3D& camera = world.GetCamera();
    camera.Position = { 2.0f, 9.0f, 18.0f };
    camera.Target = { 1.0f, 3.0f, 0.0f };

    // --- interface ---------------------------------------------------------

    opane::Element* root = app.GetRoot();

    opane::Panel* panel = root->Add<opane::Panel>();
    panel->Size = opane::Size2::FromOffset(266.0f, 210.0f);
    panel->Position = opane::Position2::FromOffset(22.0f, 22.0f);
    panel->ChildLayout = opane::LayoutMode::Vertical;
    panel->Padding = 16.0f;
    panel->Spacing = 8.0f;

    opane::Label* title = panel->Add<opane::Label>();
    title->Text = "Joints";
    title->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    opane::Label* stats = panel->Add<opane::Label>();
    stats->Muted = true;
    stats->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };

    opane::Label* speedLabel = panel->Add<opane::Label>();
    speedLabel->Text = "Windmill speed";
    speedLabel->Muted = true;
    speedLabel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(20.0f) };

    opane::Slider* speed = panel->Add<opane::Slider>();
    speed->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    speed->Minimum = -6.0f;
    speed->Maximum = 6.0f;
    speed->Value = 2.0f;
    speed->OnChanged = [&](float value) { machines.Windmill.SetMotorSpeed(value); };

    opane::Button* rebuild = panel->Add<opane::Button>();
    rebuild->Text = "Rebuild";
    rebuild->Accent = true;
    rebuild->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };

    opane::Checkbox* running = panel->Add<opane::Checkbox>();
    running->Text = "Physics running";
    running->Checked = true;
    running->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    running->OnChanged = [&](bool checked) {
        if (checked)
        {
            world.StartPhysics();
        }
        else
        {
            world.StopPhysics();
        }
    };

    auto Rebuild = [&] {
        // Destroying an actor takes its joints with it, so tearing the scene
        // down needs no joint bookkeeping at all.
        for (ludifex::Actor3D& actor : machines.Actors)
        {
            actor.Destroy();
        }
        for (ludifex::Actor3D& ball : balls)
        {
            ball.Destroy();
        }
        balls.clear();
        world.ApplyPendingChanges();

        machines = Build(world);
        machines.Windmill.SetMotorSpeed(speed->Value);
    };

    rebuild->OnClick = Rebuild;

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;
    float liftDirection = 1.0f;

    std::printf("\n  Click   drop a ball\n  Space   pause physics\n  R       rebuild\n"
                "  Escape  quit\n\n");

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();

        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        if (input.WasKeyPressed(opane::Key::Space))
        {
            running->Checked = !running->Checked;
            if (running->OnChanged)
            {
                running->OnChanged(running->Checked);
            }
        }

        if (input.WasKeyPressed(opane::Key::R))
        {
            Rebuild();
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

        if (input.WasMouseButtonPressed(opane::MouseButton::Left) && windowSize.X > 0.0f)
        {
            const opane::Vec2 pointer = input.GetMousePosition();
            const ludifex::RayHit hit =
                world.PickFromView(pointer.X / windowSize.X, pointer.Y / windowSize.Y);

            if (hit.Hit)
            {
                // Dropped a little above whatever was clicked.
                ludifex::Actor3D ball = world.AddSphere({
                    .Radius = 0.35f,
                    .Position = { hit.Point.X, hit.Point.Y + 4.0f, hit.Point.Z },
                    .Density = 3.0f,
                    .Restitution = 0.2f,
                });
                ball.SetColor(ludifex::Color::FromBytes(250, 220, 120));
                balls.push_back(ball);
            }
        }

        // The lift reverses at each end of its travel. Reading the joint back
        // like this is what the translation accessor is for.
        const float travel = machines.Lift.GetTranslation();
        if (travel > 3.9f && liftDirection > 0.0f)
        {
            liftDirection = -1.0f;
            machines.Lift.SetMotorSpeed(-1.5f);
        }
        else if (travel < 0.1f && liftDirection < 0.0f)
        {
            liftDirection = 1.0f;
            machines.Lift.SetMotorSpeed(1.5f);
        }

        world.Update(deltaSeconds);
        world.Render();

        char line[128];
        std::snprintf(line, sizeof(line), "%zu actors   %zu joints   lift %.2f m",
                      world.GetActorCount(), world.GetJointCount(),
                      static_cast<double>(travel));
        stats->Text = line;

        if (worldView.IsValid())
        {
            app.GetDrawList().DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }

        app.GetDrawList().DrawText("Click drop    Space pause    R rebuild    Esc quit",
                                   { windowSize.X * 0.5f, windowSize.Y - 32.0f },
                                   app.GetDefaultFont(),
                                   opane::Color::FromBytes(255, 255, 255, 105),
                                   opane::TextAlign::Center);
    });

    app.Shutdown();
    return 0;
}
