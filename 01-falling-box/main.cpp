// Example 01: opane and ludifex together, in 3D.
//
// A window and frame loop from opane, input, a Box3D world stepped at a fixed
// rate, smooth motion between steps, and pausing and resuming physics.
//
// Controls:
//   Space  pause and resume physics
//   R      reset the crate
//   D      drop another crate
//   Escape quit

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/Tone.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace
{

constexpr float DropHeight = 8.0f;

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: falling box",
        .Width = 1280,
        .Height = 720,
    });

    if (!app.IsValid())
    {
        std::fprintf(stderr, "Could not start the application.\n");
        return 1;
    }

    // Give ludifex the device opane already created, so there is one device
    // with one owner. Neither library includes the other's headers.
    ludifex::AdoptHost({ app.GetGpuDevice(), app.GetWindow() });

    ludifex::World3D world = ludifex::CreateWorld3D();
    if (!world.IsValid())
    {
        std::fprintf(stderr, "Could not create the world.\n");
        app.Shutdown();
        return 1;
    }

    world.AddGround({
        .Width = 50.0f,
        .Depth = 50.0f,
        .Name = "Ground",
    });

    ludifex::Actor3D crate = world.AddBox({
        .Scale = { 1.0f, 1.0f, 1.0f },
        .Position = { 0.0f, DropHeight, 0.0f },
        .Restitution = 0.15f,
        .Name = "Crate",
    });

    std::vector<ludifex::Actor3D> extras;

    // A collision event plays a sound. The impact sound is generated, not
    // loaded, so this needs no asset files.
    const std::string impactPath =
        (std::filesystem::temp_directory_path() / "ludifex-impact.wav").string();
    examples::WriteToneWav(impactPath, 180.0f, 260, 0.5f, 22.0f);

    const opane::SoundId impactSound = app.LoadSound(impactPath);

    int collisionCount = 0;
    float lastImpactSpeed = 0.0f;

    // Anything softer than this is a resting contact rather than a hit.
    world.SetCollisionThreshold(1.5f);

    world.WhenActorCollided([&](const ludifex::CollisionInfo& info) {
        ++collisionCount;
        lastImpactSpeed = info.ImpactSpeed;

        // Scaling volume and pitch by the approach speed is why the event
        // carries it: a nudge and a drop should not sound the same.
        const float strength = std::clamp(info.ImpactSpeed / 12.0f, 0.15f, 1.0f);

        opane::SoundPlayback playback;
        playback.Volume = strength;
        playback.Pitch = 0.85f + strength * 0.4f;
        app.PlaySound(impactSound, playback);

        // Flash the actor that was hit, so the event can be seen as well as
        // heard. The handle is copied because the event arrives by const
        // reference; copying a handle is cheap.
        ludifex::Actor3D struck = info.Self;
        struck.SetColor(ludifex::Color{ 1.0f, 0.95f, 0.75f, 1.0f });
    });

    std::printf("\n");
    std::printf("  Space  pause and resume physics\n");
    std::printf("  R      reset the crate\n");
    std::printf("  D      drop an extra crate\n");
    std::printf("  Click  punch an object upward\n");
    std::printf("  Escape quit\n\n");

    uint64_t lastReportedStep = 0;

    // The world's render target, wrapped so opane can draw it. It is recreated
    // whenever the window size changes.
    opane::TextureId worldView;
    int lastViewWidth = 0;
    int lastViewHeight = 0;

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
                std::printf("Physics paused at step %llu. Rendering continues.\n",
                            static_cast<unsigned long long>(world.GetStepCount()));
            }
            else
            {
                world.StartPhysics();
                std::printf("Physics resumed.\n");
            }
        }

        if (input.WasKeyPressed(opane::Key::R))
        {
            crate.SetPosition({ 0.0f, DropHeight, 0.0f });
            crate.SetRotation({});
            crate.SetLinearVelocity({});
            crate.SetAngularVelocity({});
            crate.SetAwake(true);
            std::printf("Crate reset.\n");
        }

        if (input.WasKeyPressed(opane::Key::D))
        {
            const float offset = static_cast<float>(extras.size() % 5) * 0.7f - 1.4f;
            extras.push_back(world.AddBox({
                .Scale = { 0.6f, 0.6f, 0.6f },
                .Position = { offset, DropHeight + 2.0f, offset * 0.5f },
                .Restitution = 0.3f,
            }));
            std::printf("Dropped an extra crate. The world now holds %zu actors.\n",
                        world.GetActorCount());
        }

        // Automatic mode: this runs as many fixed steps as the elapsed time
        // allows, then leaves the interpolation alpha ready for rendering.
        // Collision handlers fire from inside here.
        world.Update(deltaSeconds);

        // Ease every actor back from its impact flash toward its resting tint.
        world.ForEachActor([&](ludifex::Actor3D& actor) {
            const bool isGround = (actor.GetName() == "Ground");
            const ludifex::Color resting = isGround
                                               ? ludifex::Color{ 0.28f, 0.31f, 0.38f, 1.0f }
                                               : ludifex::Color{ 0.89f, 0.64f, 0.31f, 1.0f };

            const ludifex::Color current = actor.GetColor();
            const float rate = std::min(1.0f, deltaSeconds * 4.0f);

            actor.SetColor(ludifex::Color{
                current.R + (resting.R - current.R) * rate,
                current.G + (resting.G - current.G) * rate,
                current.B + (resting.B - current.B) * rate,
                1.0f,
            });
        });

        // The interpolated transform blends the previous and current physics
        // steps, so fixed-step motion looks smooth at any frame rate. The world
        // uses it when it draws; it is read here only for the readout.
        const ludifex::Transform3 transform = crate.GetInterpolatedTransform();

        const opane::Vec2 windowSize = app.GetWindowSize();
        const int viewWidth = static_cast<int>(windowSize.X);
        const int viewHeight = static_cast<int>(windowSize.Y);

        if (viewWidth != lastViewWidth || viewHeight != lastViewHeight)
        {
            lastViewWidth = viewWidth;
            lastViewHeight = viewHeight;

            world.SetRenderSize(viewWidth, viewHeight);

            if (worldView.IsValid())
            {
                app.DestroyTexture(worldView);
            }
            worldView = app.WrapExternalTexture(world.GetRenderTarget(), viewWidth, viewHeight);
        }

        // Orbit slowly, which makes the depth and the lighting readable.
        ludifex::Camera3D& camera = world.GetCamera();
        const float orbit = app.GetTimeSeconds() * 0.22f;
        camera.Position = ludifex::Vec3{ std::sin(orbit) * 15.0f, 7.5f, std::cos(orbit) * 15.0f };
        camera.Target = ludifex::Vec3{ 0.0f, 1.5f, 0.0f };

        // Click an object to punch it upward. PickFromView turns a point on the
        // view into a ray through the camera, so this is the same query a
        // selection tool or a gun would use.
        if (input.WasMouseButtonPressed(opane::MouseButton::Left) && windowSize.X > 0.0f)
        {
            const opane::Vec2 pointer = input.GetMousePosition();
            const ludifex::RayHit hit =
                world.PickFromView(pointer.X / windowSize.X, pointer.Y / windowSize.Y);

            if (hit.Hit)
            {
                ludifex::Actor3D picked = hit.Actor;
                picked.ApplyImpulse({ 0.0f, 5.0f, 0.0f });
                picked.SetColor(ludifex::Color{ 0.45f, 0.85f, 1.0f, 1.0f });

                std::printf("Picked \"%s\" at %.2f m, %.2f m from the camera.\n",
                            picked.GetName().empty() ? "<unnamed>" : picked.GetName().c_str(),
                            static_cast<double>(hit.Point.Y), static_cast<double>(hit.Distance));
            }
        }

        // The world draws itself into its own texture, on its own command
        // buffer. opane then composites it like any other image.
        world.Render();

        opane::DrawList& drawList = app.GetDrawList();
        if (worldView.IsValid())
        {
            drawList.DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }

        // A heads-up panel: rounded corners, a stroke, a circle, and a bar.
        const opane::Rect panel{ 24.0f, 24.0f, 236.0f, 92.0f };
        drawList.FillRoundedRect(panel, 14.0f, opane::Color::FromBytes(16, 18, 24, 220));
        drawList.StrokeRect(panel, 1.0f, opane::Color::FromBytes(255, 255, 255, 40), 14.0f);

        const opane::Color statusColor = world.IsPhysicsRunning()
                                             ? opane::Color::FromBytes(120, 210, 140)
                                             : opane::Color::FromBytes(232, 176, 92);
        drawList.FillCircle({ panel.X + 26.0f, panel.Y + 30.0f }, 8.0f, statusColor);

        const opane::FontId font = app.GetDefaultFont();

        drawList.DrawText(world.IsPhysicsRunning() ? "Running" : "Paused",
                          { panel.X + 44.0f, panel.Y + 20.0f }, font,
                          opane::Color::FromBytes(236, 240, 248));

        char detail[128];
        std::snprintf(detail, sizeof(detail), "y %.2f m   %zu actors   %d hits @ %.1f m/s",
                      static_cast<double>(transform.Position.Y), world.GetActorCount(),
                      collisionCount, static_cast<double>(lastImpactSpeed));
        drawList.DrawText(detail, { panel.X + 20.0f, panel.Y + 38.0f }, font,
                          opane::Color::FromBytes(150, 160, 178));

        drawList.DrawText("Space pause    R reset    D drop    Esc quit",
                          { windowSize.X * 0.5f, windowSize.Y - 34.0f }, font,
                          opane::Color::FromBytes(255, 255, 255, 110), opane::TextAlign::Center);

        // How far through the current physics step this frame sits.
        const opane::Rect track{ panel.X + 20.0f, panel.Y + 60.0f, panel.Width - 40.0f, 8.0f };
        drawList.FillRoundedRect(track, 4.0f, opane::Color::FromBytes(255, 255, 255, 30));
        drawList.FillRoundedRect(
            { track.X, track.Y, track.Width * world.GetInterpolationAlpha(), track.Height }, 4.0f,
            opane::Color::FromBytes(120, 170, 240));

        if (app.GetFrameCount() % 15 == 0)
        {
            char title[256];
            std::snprintf(title, sizeof(title),
                          "ludifex: y %.2f m   %s   actors %zu   step %llu   alpha %.2f",
                          static_cast<double>(transform.Position.Y),
                          world.IsPhysicsRunning() ? "running" : "PAUSED",
                          world.GetActorCount(),
                          static_cast<unsigned long long>(world.GetStepCount()),
                          static_cast<double>(world.GetInterpolationAlpha()));
            app.SetTitle(title);
        }

        const uint64_t step = world.GetStepCount();
        if (step != lastReportedStep && step % 30 == 0)
        {
            lastReportedStep = step;
            const ludifex::Vec3 position = crate.GetPosition();
            std::printf("step %4llu   position %6.2f %6.2f %6.2f   %s\n",
                        static_cast<unsigned long long>(step),
                        static_cast<double>(position.X),
                        static_cast<double>(position.Y),
                        static_cast<double>(position.Z),
                        crate.IsAwake() ? "awake" : "asleep");
        }
    });

    std::printf("\nFinished after %llu physics steps and %llu frames.\n",
                static_cast<unsigned long long>(world.GetStepCount()),
                static_cast<unsigned long long>(app.GetFrameCount()));

    app.Shutdown();
    return 0;
}
