// Example 04: loading a glTF model.
//
// The model is generated at startup as a glTF file with an embedded buffer, so
// the repository has no binary files and the loader reads a real file.
//
// Twenty actors share one file. It is parsed and uploaded once, and the console
// prints one load message.
//
// Behind them are models in three other formats, read through Assimp: a
// textured OBJ box, an FBX pillar written with Z up, and a skinned Collada
// column playing its own clip.
//
// Controls:
//   Click   push the object under the pointer
//   Space   pause and resume physics
//   R       drop a new batch
//   Escape  quit

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/Gltf.h"
#include "../common/ModelFormats.h"
#include "../common/Tone.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: glTF model",
        .Width = 1220,
        .Height = 740,
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

    // Write the asset, then load it the same way any other glTF would be.
    const std::string modelPath =
        (std::filesystem::temp_directory_path() / "ludifex-gem.gltf").string();

    if (!examples::WriteGltfOctahedron(modelPath, 0.55f))
    {
        std::fprintf(stderr, "Could not write the test model.\n");
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 40.0f, .Depth = 40.0f, .Name = "Ground" });

    // The other formats, written the same way and standing in a row behind.
    const std::filesystem::path temporary = std::filesystem::temp_directory_path();
    const std::string boxPath = (temporary / "ludifex-crate.obj").string();
    const std::string pillarPath = (temporary / "ludifex-pillar.fbx").string();
    const std::string columnPath = (temporary / "ludifex-column.dae").string();
    if (examples::WriteObjBox(boxPath, 2.0f, 1.0f, 1.0f, "ludifex-crate.png") &&
        examples::WriteFbxBox(pillarPath, 0.6f, 0.6f, 2.4f, 100.0, true) &&
        examples::WriteColladaBendingColumn(columnPath))
    {
        world.AddModel({ .Path = boxPath, .Position = { -4.0f, 0.5f, -6.0f }, .Type = ludifex::BodyType::Static });
        world.AddModel({ .Path = pillarPath, .Position = { 0.0f, 1.2f, -6.0f }, .Type = ludifex::BodyType::Static });
        ludifex::Actor3D column = world.AddModel({ .Path = columnPath,
                                                   .Position = { 4.0f, 0.0f, -6.0f },
                                                   .Type = ludifex::BodyType::Static });
        world.ApplyPendingChanges();
        column.SetColor(ludifex::Color{ 0.95f, 0.55f, 0.35f, 1.0f });
        column.Play({ .Index = 0, .Loop = true });
    }

    std::vector<ludifex::Actor3D> gems;

    auto DropBatch = [&] {
        for (ludifex::Actor3D& gem : gems)
        {
            gem.Destroy();
        }
        gems.clear();
        world.ApplyPendingChanges();

        for (int index = 0; index < 20; ++index)
        {
            const float angle = static_cast<float>(index) * 0.9f;
            const float radius = 1.0f + static_cast<float>(index % 5) * 0.55f;

            // Every one of these shares a single parsed mesh and a single set
            // of GPU buffers.
            ludifex::Actor3D gem = world.AddModel({
                .Path = modelPath,
                .Position = { std::cos(angle) * radius, 4.0f + static_cast<float>(index) * 0.8f,
                              std::sin(angle) * radius },
                .Scale = 1.0f,
                .Restitution = 0.25f,
                .Name = "Gem " + std::to_string(index),
            });

            const float hue = static_cast<float>(index) / 19.0f;
            gem.SetColor(ludifex::Color{ 0.45f + hue * 0.5f, 0.75f - hue * 0.25f,
                                         0.95f - hue * 0.35f, 1.0f });
            gems.push_back(gem);
        }
    };

    DropBatch();

    world.SetCollisionThreshold(2.0f);

    // Positional audio: the impact is played where the collision happened, and
    // the listener follows the camera, so a gem landing on the far side of the
    // scene sounds farther away than one landing underfoot.
    const std::string impactPath =
        (std::filesystem::temp_directory_path() / "ludifex-chime.wav").string();
    examples::WriteToneWav(impactPath, 520.0f, 240, 0.45f, 14.0f);

    const opane::SoundId chime = app.LoadSound(impactPath);
    int impacts = 0;

    world.WhenActorCollided([&](const ludifex::CollisionInfo& info) {
        ++impacts;

        const float strength = std::clamp(info.ImpactSpeed / 10.0f, 0.15f, 1.0f);

        app.PlaySoundAt(chime, opane::Vec3{ info.Point.X, info.Point.Y, info.Point.Z },
                        {
                            .Volume = strength,
                            .Pitch = 0.9f + strength * 0.5f,
                            .MinDistance = 2.0f,
                            .MaxDistance = 35.0f,
                        });
    });

    ludifex::RenderSettings& settings = world.GetRenderSettings();
    settings.SkyColor = ludifex::Color::FromBytes(18, 22, 32);

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;
    float orbit = 0.0f;

    std::printf("\n  Click   punch an object\n  Space   pause physics\n  R       drop a fresh batch\n"
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
            DropBatch();
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
                ludifex::Actor3D picked = hit.Actor;
                picked.ApplyImpulse({ 0.0f, 6.0f, 0.0f });
            }
        }

        orbit += deltaSeconds * 0.18f;

        ludifex::Camera3D& camera = world.GetCamera();
        camera.Position = ludifex::Vec3{ std::sin(orbit) * 11.0f, 6.0f, std::cos(orbit) * 11.0f };
        camera.Target = ludifex::Vec3{ 0.0f, 1.2f, 0.0f };

        // Keep the listener on the camera. ludifex positions sounds, opane
        // mixes them; the three lines between are all the coupling there is.
        const ludifex::Vec3 toTarget{ camera.Target.X - camera.Position.X,
                                      camera.Target.Y - camera.Position.Y,
                                      camera.Target.Z - camera.Position.Z };
        const float length = std::sqrt(toTarget.X * toTarget.X + toTarget.Y * toTarget.Y +
                                       toTarget.Z * toTarget.Z);
        if (length > 1e-4f)
        {
            app.SetListener(opane::Vec3{ camera.Position.X, camera.Position.Y, camera.Position.Z },
                            opane::Vec3{ toTarget.X / length, toTarget.Y / length,
                                         toTarget.Z / length });
        }

        world.Update(deltaSeconds);
        world.Render();

        opane::DrawList& drawList = app.GetDrawList();
        if (worldView.IsValid())
        {
            drawList.DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }

        const opane::Rect panel{ 22.0f, 22.0f, 268.0f, 78.0f };
        drawList.FillRoundedRect(panel, 14.0f, opane::Color::FromBytes(14, 16, 22, 225));
        drawList.StrokeRect(panel, 1.0f, opane::Color::FromBytes(255, 255, 255, 38), 14.0f);

        const opane::FontId font = app.GetDefaultFont();
        drawList.DrawText("ludifex-gem.gltf", { panel.X + 20.0f, panel.Y + 18.0f }, font,
                          opane::Color::FromBytes(236, 240, 248));

        char detail[128];
        std::snprintf(detail, sizeof(detail), "%zu actors   %u draws   %u drawn   %d impacts",
                      world.GetActorCount(), world.GetLastDrawCallCount(),
                      world.GetDrawnInstanceCount(), impacts);
        drawList.DrawText(detail, { panel.X + 20.0f, panel.Y + 44.0f }, font,
                          opane::Color::FromBytes(150, 160, 178));

        drawList.DrawText("Click punch    Space pause    R drop    Esc quit",
                          { windowSize.X * 0.5f, windowSize.Y - 32.0f }, font,
                          opane::Color::FromBytes(255, 255, 255, 105), opane::TextAlign::Center);
    });

    app.Shutdown();
    return 0;
}
