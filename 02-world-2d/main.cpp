// Example 02: a 2D world.
//
// World2D is drawn by the same renderer as World3D, with an orthographic camera
// looking down the Z axis. Shapes are given a little depth so the light still
// shades them.
//
// It also uses the 2D event and query API: clicking picks the block under the
// pointer and knocks it away, clicking empty space drops a ball there, a sensor
// on the right counts what is inside it, and hard impacts are counted by the
// world's collision handler.
//
// Controls:
//   Click   knock the block under the pointer, or drop a ball on empty space
//   Space   pause and resume physics
//   R       rebuild the stack
//   Escape  quit

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

constexpr int StackHeight = 9;

void BuildStack(ludifex::World2D& world, std::vector<ludifex::Actor2D>& outBlocks)
{
    for (ludifex::Actor2D& block : outBlocks)
    {
        block.Destroy();
    }
    outBlocks.clear();
    world.ApplyPendingChanges();

    for (int level = 0; level < StackHeight; ++level)
    {
        const float y = 0.3f + static_cast<float>(level) * 0.62f;
        const float offset = (level % 2 == 0) ? -0.12f : 0.12f;

        ludifex::Actor2D block = world.AddRectangle({
            .Width = 1.6f,
            .Height = 0.6f,
            .Position = { offset, y },
            .Friction = 0.5f,
            .Name = "block " + std::to_string(level + 1),
        });

        // Shade the stack by height so the settling is easy to read.
        const float t = static_cast<float>(level) / static_cast<float>(StackHeight - 1);
        block.SetColor(ludifex::Color{ 0.95f - t * 0.35f, 0.62f + t * 0.10f, 0.28f + t * 0.45f, 1.0f });

        outBlocks.push_back(block);
    }
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: 2D world",
        .Width = 1100,
        .Height = 700,
    });

    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::AdoptHost({ app.GetGpuDevice(), app.GetWindow() });

    ludifex::World2D world = ludifex::CreateWorld2D();
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 60.0f, .Name = "Ground" });

    // A catch zone. Sensors start hidden, so its outline is drawn as an
    // overlay using WorldToView, and whatever is inside it is counted from its
    // enter and leave events.
    constexpr float ZoneX = 6.0f;
    constexpr float ZoneWidth = 3.0f;
    constexpr float ZoneHeight = 2.5f;

    // It floats a few centimetres above the ground: a sensor notices static
    // actors too, and one resting on the ground would count the ground.
    constexpr float ZoneLift = 0.05f;

    ludifex::Actor2D zone = world.AddRectangle({
        .Width = ZoneWidth,
        .Height = ZoneHeight,
        .Position = { ZoneX, ZoneLift + ZoneHeight * 0.5f },
        .Type = ludifex::BodyType::Static,
        .IsSensor = true,
        .Name = "Catch zone",
    });

    int insideZone = 0;
    zone.WhenEntered([&](const ludifex::TriggerInfo2D&) { ++insideZone; });
    zone.WhenExited([&](const ludifex::TriggerInfo2D&) { insideZone = std::max(0, insideZone - 1); });

    int hardImpacts = 0;
    world.SetCollisionThreshold(3.0f);
    world.WhenActorCollided([&](const ludifex::CollisionInfo2D&) { ++hardImpacts; });

    std::vector<ludifex::Actor2D> blocks;
    BuildStack(world, blocks);

    std::vector<ludifex::Actor2D> balls;
    std::string lastPicked = "nothing yet";

    ludifex::Camera2D camera;
    camera.Center = { 0.0f, 3.2f };
    camera.Height = 11.0f;
    world.SetCamera(camera);

    world.GetRenderSettings().SkyColor = ludifex::Color::FromBytes(20, 24, 34);

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;

    std::printf("\n  Click   knock a block, or drop a ball on empty space\n  Space   pause physics\n  R       rebuild the stack\n"
                "  Escape  quit\n\n");

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();

        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        if (input.WasKeyPressed(opane::Key::Space))
        {
            if (world.IsPhysicsRunning())
            {
                world.StopPhysics();
            }
            else
            {
                world.StartPhysics();
            }
        }

        if (input.WasKeyPressed(opane::Key::R))
        {
            BuildStack(world, blocks);
            for (ludifex::Actor2D& ball : balls)
            {
                ball.Destroy();
            }
            balls.clear();
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
            const float viewX = pointer.X / windowSize.X;
            const float viewY = pointer.Y / windowSize.Y;

            const ludifex::RayHit2D pick = world.PickFromView(viewX, viewY);

            if (pick.Hit && pick.Actor.GetName() != "Ground")
            {
                // Knock it up and away from the side it was clicked on.
                ludifex::Actor2D target = pick.Actor;
                const ludifex::Vec2 centre = target.GetPosition();
                const float side = (pick.Point.X < centre.X) ? 1.0f : -1.0f;
                const float mass = target.GetMass();
                target.ApplyImpulse({ side * 4.0f * mass, 6.0f * mass });
                target.SetColor(ludifex::Color::FromBytes(250, 120, 110));
                lastPicked = target.GetName().empty() ? "a ball" : target.GetName();
            }
            else if (!pick.Hit)
            {
                ludifex::Actor2D ball = world.AddCircle({
                    .Radius = 0.28f,
                    .Position = world.ViewToWorld(viewX, viewY),
                    .Restitution = 0.45f,
                });
                ball.SetColor(ludifex::Color::FromBytes(120, 200, 255));
                balls.push_back(ball);
            }
        }

        world.Update(deltaSeconds);
        world.Render();

        opane::DrawList& drawList = app.GetDrawList();
        if (worldView.IsValid())
        {
            drawList.DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }

        // The catch zone's outline, placed with WorldToView. Its fill brightens
        // while something is inside it.
        const ludifex::Vec2 zoneTopLeft =
            world.WorldToView({ ZoneX - ZoneWidth * 0.5f, ZoneLift + ZoneHeight });
        const ludifex::Vec2 zoneBottomRight = world.WorldToView({ ZoneX + ZoneWidth * 0.5f, ZoneLift });
        const opane::Rect zoneRect{ zoneTopLeft.X * windowSize.X, zoneTopLeft.Y * windowSize.Y,
                                    (zoneBottomRight.X - zoneTopLeft.X) * windowSize.X,
                                    (zoneBottomRight.Y - zoneTopLeft.Y) * windowSize.Y };

        drawList.FillRect(zoneRect, insideZone > 0 ? opane::Color::FromBytes(120, 220, 150, 60)
                                                   : opane::Color::FromBytes(255, 255, 255, 14));
        drawList.StrokeRect(zoneRect, 1.5f, opane::Color::FromBytes(120, 220, 150, 170));
        drawList.DrawText("catch zone", { zoneRect.X + zoneRect.Width * 0.5f, zoneRect.Y - 24.0f },
                          app.GetDefaultFont(), opane::Color::FromBytes(120, 220, 150, 200),
                          opane::TextAlign::Center);

        // Heads-up readout.
        const opane::Rect panel{ 22.0f, 22.0f, 280.0f, 124.0f };
        drawList.FillRoundedRect(panel, 14.0f, opane::Color::FromBytes(14, 16, 22, 225));
        drawList.StrokeRect(panel, 1.0f, opane::Color::FromBytes(255, 255, 255, 38), 14.0f);

        drawList.FillCircle({ panel.X + 26.0f, panel.Y + 28.0f }, 7.0f,
                            world.IsPhysicsRunning() ? opane::Color::FromBytes(120, 210, 140)
                                                     : opane::Color::FromBytes(232, 176, 92));

        const opane::FontId font = app.GetDefaultFont();
        drawList.DrawText(world.IsPhysicsRunning() ? "Running" : "Paused",
                          { panel.X + 44.0f, panel.Y + 19.0f }, font,
                          opane::Color::FromBytes(236, 240, 248));

        char detail[96];
        std::snprintf(detail, sizeof(detail), "%zu actors   step %llu", world.GetActorCount(),
                      static_cast<unsigned long long>(world.GetStepCount()));
        drawList.DrawText(detail, { panel.X + 20.0f, panel.Y + 44.0f }, font,
                          opane::Color::FromBytes(150, 160, 178));

        std::snprintf(detail, sizeof(detail), "in zone %d   hard impacts %d", insideZone, hardImpacts);
        drawList.DrawText(detail, { panel.X + 20.0f, panel.Y + 68.0f }, font,
                          opane::Color::FromBytes(150, 160, 178));

        std::snprintf(detail, sizeof(detail), "picked %s", lastPicked.c_str());
        drawList.DrawText(detail, { panel.X + 20.0f, panel.Y + 92.0f }, font,
                          opane::Color::FromBytes(150, 160, 178));

        drawList.DrawText("Click knock or drop    Space pause    R rebuild    Esc quit",
                          { windowSize.X * 0.5f, windowSize.Y - 32.0f }, font,
                          opane::Color::FromBytes(255, 255, 255, 105), opane::TextAlign::Center);
    });

    app.Shutdown();
    return 0;
}
