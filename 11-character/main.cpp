// Example 11: a walking character.
//
// The capsule is not a dynamic body, which would slide down slopes, catch on
// seams, and tip over. Instead it is moved once a frame with Move: it slides
// along what it hits, steps over low obstacles, stops at slopes that are too
// steep, and reports what it is standing on.
//
// Gravity and jumping are handled here in the program. The character
// controller only works out where the capsule can go, so how the character
// feels is up to the game.
//
// W / A / S / D walk, Space jumps, the mouse turns the camera, Escape quits.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

constexpr float Gravity = -22.0f;    // brisker than the real thing, as games are
constexpr float WalkSpeed = 5.5f;
constexpr float JumpSpeed = 8.5f;
constexpr float Pi = 3.14159265358979323846f;

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane + ludifex: a character",
        .Width = 1280,
        .Height = 800,
    });
    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::World3D world = ludifex::CreateWorld3D();
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 80.0f, .Depth = 80.0f });

    // --- a level with something to climb, something to slide along, and
    //     something to push ----------------------------------------------------

    auto AddSolid = [&](ludifex::Vec3 scale, ludifex::Vec3 position, ludifex::Color color,
                        const std::string& name) {
        ludifex::Actor3D block = world.AddBox({
            .Scale = scale,
            .Position = position,
            .Type = ludifex::BodyType::Static,
            .Name = name,
        });
        block.SetColor(color);
        return block;
    };

    const ludifex::Color stone = ludifex::Color::FromBytes(120, 124, 136);
    const ludifex::Color warm = ludifex::Color::FromBytes(168, 138, 104);

    // A staircase of four steps, each within the character's step height.
    for (int step = 0; step < 4; ++step)
    {
        const float height = 0.3f * static_cast<float>(step + 1);
        AddSolid({ 3.0f, height, 1.2f },
                 { -6.0f, height * 0.5f, -2.0f - static_cast<float>(step) * 1.2f }, stone,
                 "Step " + std::to_string(step));
    }

    // A platform the stairs lead onto, and a wall to slide along.
    AddSolid({ 6.0f, 1.2f, 4.0f }, { -6.0f, 0.6f, -8.0f }, stone, "Platform");
    AddSolid({ 12.0f, 3.0f, 0.5f }, { 2.0f, 1.5f, -12.0f }, stone, "Wall");

    // A ledge too tall to step onto: it has to be walked around.
    AddSolid({ 3.0f, 1.0f, 3.0f }, { 6.0f, 0.5f, -4.0f }, warm, "Ledge");

    // A ramp, shallow enough to stand on, and a steeper one that is not.
    ludifex::Actor3D ramp = world.AddBox({
        .Scale = { 4.0f, 0.4f, 6.0f },
        .Position = { 0.0f, 0.9f, 5.0f },
        .Rotation = ludifex::Quat::FromAxisAngle({ 1.0f, 0.0f, 0.0f }, -0.35f),
        .Type = ludifex::BodyType::Static,
        .Name = "Ramp",
    });
    ramp.SetColor(warm);

    // Crates: dynamic bodies, so the character's own capsule pushes them.
    std::vector<ludifex::Actor3D> crates;
    for (int index = 0; index < 6; ++index)
    {
        ludifex::Actor3D crate = world.AddBox({
            .Scale = { 0.7f, 0.7f, 0.7f },
            .Position = { 4.0f + static_cast<float>(index % 3) * 0.9f, 0.4f,
                          2.0f + static_cast<float>(index / 3) * 0.9f },
            .Friction = 0.5f,
            .Name = "Crate",
        });
        crate.SetColor(ludifex::Color::FromBytes(198, 152, 92));
        crates.push_back(crate);
    }

    // --- the character ---------------------------------------------------------

    ludifex::Character3D walker = world.AddCharacter({
        .Position = { -6.0f, 1.2f, 2.0f },
        .Radius = 0.35f,
        .Height = 1.8f,
        .StepHeight = 0.4f,
        .SlopeLimitDegrees = 50.0f,
    });
    walker.GetActor().SetColor(ludifex::Color::FromBytes(240, 240, 250));

    float heading = 0.0f;
    float fallSpeed = 0.0f;
    bool wasOnGround = true;

    // --- interface -------------------------------------------------------------

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;

    opane::Panel* sidebar = root->Add<opane::Panel>();
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(280.0f), opane::Dim::FromScale(1.0f) };
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

    AddLabel("The mover", false, 28.0f);
    opane::Label* standing = AddLabel("", false);
    opane::Label* ground = AddLabel("", true);
    opane::Label* speed = AddLabel("", true);
    opane::Label* place = AddLabel("", true);

    opane::Checkbox* showProbe = sidebar->Add<opane::Checkbox>();
    showProbe->Text = "Draw what it stands on";
    showProbe->Checked = true;
    showProbe->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    opane::Button* reset = sidebar->Add<opane::Button>();
    reset->Text = "Back to the stairs";
    reset->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };
    reset->OnClick = [&] {
        walker.SetPosition({ -6.0f, 1.2f, 2.0f });
        fallSpeed = 0.0f;
    };

    AddLabel("W / A / S / D walk", true);
    AddLabel("Space jumps", true);
    AddLabel("Drag with the left button to turn", true);

    opane::Viewport* viewport = root->Add<opane::Viewport>();
    viewport->Size = opane::Size2{ opane::Dim{ 1.0f, -280.0f }, opane::Dim::FromScale(1.0f) };
    viewport->SetWorld(world);

    bool turning = false;
    viewport->OnViewEvent = [&](const opane::Event& viewEvent) {
        if (viewEvent.Type == opane::EventType::PointerDown)
        {
            turning = true;
        }
        else if (viewEvent.Type == opane::EventType::PointerUp)
        {
            turning = false;
        }
    };

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        const float step = std::min(deltaSeconds, 1.0f / 30.0f);

        if (turning)
        {
            heading -= input.GetMouseDelta().X * 0.006f;
        }
        if (input.IsKeyDown(opane::Key::Left))
        {
            heading += 2.0f * step;
        }
        if (input.IsKeyDown(opane::Key::Right))
        {
            heading -= 2.0f * step;
        }

        // Walking is relative to the camera, so W always moves away from it.
        const ludifex::Vec3 forward{ -std::sin(heading), 0.0f, -std::cos(heading) };
        const ludifex::Vec3 right{ std::cos(heading), 0.0f, -std::sin(heading) };

        float alongX = 0.0f;
        float alongZ = 0.0f;
        if (input.IsKeyDown(opane::Key::W))
        {
            alongX += forward.X;
            alongZ += forward.Z;
        }
        if (input.IsKeyDown(opane::Key::S))
        {
            alongX -= forward.X;
            alongZ -= forward.Z;
        }
        if (input.IsKeyDown(opane::Key::D))
        {
            alongX += right.X;
            alongZ += right.Z;
        }
        if (input.IsKeyDown(opane::Key::A))
        {
            alongX -= right.X;
            alongZ -= right.Z;
        }

        const float length = std::sqrt(alongX * alongX + alongZ * alongZ);
        if (length > 1e-4f)
        {
            alongX = alongX / length * WalkSpeed;
            alongZ = alongZ / length * WalkSpeed;
        }

        // Gravity belongs to the program. The mover only answers where the
        // capsule can go.
        if (walker.IsOnGround())
        {
            // A little downward push keeps it on the ground over a crest
            // rather than launching off it.
            fallSpeed = -2.0f;
            if (input.WasKeyPressed(opane::Key::Space))
            {
                fallSpeed = JumpSpeed;
            }
        }
        else
        {
            fallSpeed += Gravity * step;
        }

        const ludifex::Vec3 moved =
            walker.Move({ alongX * step, fallSpeed * step, alongZ * step });

        // If Move blocked most of the vertical movement (landing, or hitting
        // a ceiling), the vertical speed is reset.
        if (std::abs(moved.Y) < std::abs(fallSpeed * step) * 0.5f)
        {
            fallSpeed = 0.0f;
        }

        const ludifex::Vec3 position = walker.GetPosition();

        // A chase camera, behind and above.
        ludifex::Camera3D& camera = world.GetCamera();
        camera.Position = { position.X - forward.X * 9.5f, position.Y + 5.0f,
                            position.Z - forward.Z * 9.5f };
        camera.Target = { position.X + forward.X * 3.0f, position.Y + 0.2f,
                          position.Z + forward.Z * 3.0f };

        if (showProbe->Checked)
        {
            const ludifex::Vec3 normal = walker.GetGroundNormal();
            const ludifex::Color color = walker.IsOnGround()
                                             ? ludifex::Color::FromBytes(120, 230, 140)
                                             : ludifex::Color::FromBytes(230, 120, 120);
            const ludifex::Vec3 feet{ position.X, position.Y - 0.9f, position.Z };
            world.DrawLine(feet, { feet.X + normal.X * 1.2f, feet.Y + normal.Y * 1.2f,
                                   feet.Z + normal.Z * 1.2f },
                           color);
            world.DrawSphere(feet, 0.12f, color);
        }

        const bool onGround = walker.IsOnGround();
        if (onGround != wasOnGround)
        {
            wasOnGround = onGround;
        }

        char line[96];
        standing->Text = onGround ? "On the ground" : "In the air";
        const ludifex::Vec3 normal = walker.GetGroundNormal();
        std::snprintf(line, sizeof(line), "Ground normal %.2f, %.2f, %.2f",
                      static_cast<double>(normal.X), static_cast<double>(normal.Y),
                      static_cast<double>(normal.Z));
        ground->Text = line;
        std::snprintf(line, sizeof(line), "Vertical speed %+.1f m/s", static_cast<double>(fallSpeed));
        speed->Text = line;
        std::snprintf(line, sizeof(line), "At %.1f, %.1f, %.1f", static_cast<double>(position.X),
                      static_cast<double>(position.Y), static_cast<double>(position.Z));
        place->Text = line;
    });

    app.Shutdown();
    return 0;
}
