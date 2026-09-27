// Example 10: a field of sound.
//
// Forty-nine looping chimes stand in a grid, and a listener walks among them.
// Only a few are mixed at once: the voice limit gives voices to the chimes that
// are loudest at the listener, the others wait silently and resume in step,
// walls turn down what is behind them, and a racer circling the field shows the
// Doppler shift.
//
// The listener is a marker on the ground, placed with SetListener, and the
// camera follows it from above. A chime lights up while it is being mixed, with
// a line drawn from the listener to it.
//
// W / S walk, A / D turn, Space plays a sound nearby, Escape quits.

#include "../common/Tone.h"

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace
{

constexpr int GridSize = 7;
constexpr float Spacing = 7.0f;
constexpr float FieldHalf = Spacing * static_cast<float>(GridSize - 1) * 0.5f;
constexpr float RacerRadius = FieldHalf + 9.0f;
constexpr float RacerSpeed = 32.0f;

struct Chime
{
    ludifex::Actor3D Bell;
    ludifex::VoiceId Voice;
    float StartAt = 0.0f;
    ludifex::SoundId Sound;
    float Pitch = 1.0f;
};

ludifex::Color Mix(ludifex::Color a, ludifex::Color b, float t)
{
    return ludifex::Color{ a.R + (b.R - a.R) * t, a.G + (b.G - a.G) * t, a.B + (b.B - a.B) * t, 1.0f };
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane + ludifex: a field of sound",
        .Width = 1320,
        .Height = 800,
    });
    if (!app.IsValid())
    {
        return 1;
    }

    // --- sounds ---------------------------------------------------------------
    //
    // Synthesized, so the example needs no audio files. Each chime is a ping
    // that dies away within its one-second loop; whole-number frequencies make
    // the loop seamless. The racer hums without decaying.

    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "ludifex-audio-field";
    std::filesystem::create_directories(directory);

    const float scale[] = { 262.0f, 294.0f, 330.0f, 392.0f, 440.0f };
    std::vector<ludifex::SoundId> notes;
    for (float frequency : scale)
    {
        const std::string path = (directory / ("chime-" + std::to_string(static_cast<int>(frequency)) + ".wav")).string();
        examples::WriteToneWav(path, frequency, 1000, 0.55f, 7.0f);
        notes.push_back(ludifex::LoadSound(path));
    }

    const std::string humPath = (directory / "hum.wav").string();
    examples::WriteToneWav(humPath, 150.0f, 1000, 0.5f, 0.0f);
    const ludifex::SoundId hum = ludifex::LoadSound(humPath);

    const std::string pingPath = (directory / "ping.wav").string();
    examples::WriteToneWav(pingPath, 880.0f, 400, 0.8f, 12.0f);
    const ludifex::SoundId ping = ludifex::LoadSound(pingPath);

    // --- world ------------------------------------------------------------------

    ludifex::World3D world = ludifex::CreateWorld3D({ .Gravity = { 0.0f, -10.0f, 0.0f } });
    if (!world.IsValid())
    {
        app.Shutdown();
        return 1;
    }

    world.AddGround({ .Width = 120.0f, .Depth = 120.0f });
    world.GetRenderSettings().Fog.Enabled = true;
    world.GetRenderSettings().Fog.Start = 30.0f;
    world.GetRenderSettings().Fog.End = 110.0f;

    ludifex::AudioSettings& audio = world.GetAudioSettings();
    audio.MaxVoices = 12;
    audio.ListenerFollowsCamera = false;

    // Dark while waiting silently, bright while being mixed, and dull between
    // the two while a wall is muffling it.
    const ludifex::Color idle = ludifex::Color::FromBytes(70, 82, 104);
    const ludifex::Color lit = ludifex::Color::FromBytes(255, 196, 92);
    const ludifex::Color dulled = ludifex::Color::FromBytes(126, 108, 92);

    std::vector<Chime> chimes;
    for (int row = 0; row < GridSize; ++row)
    {
        for (int column = 0; column < GridSize; ++column)
        {
            const float x = -FieldHalf + static_cast<float>(column) * Spacing;
            const float z = -FieldHalf + static_cast<float>(row) * Spacing;

            world.AddBox({
                .Scale = { 0.18f, 1.4f, 0.18f },
                .Position = { x, 0.7f, z },
                .Type = ludifex::BodyType::Static,
                .Name = "Post",
            });

            // A sensor, so one chime never muffles another behind it.
            Chime chime;
            chime.Bell = world.AddSphere({
                .Radius = 0.32f,
                .Position = { x, 1.72f, z },
                .Type = ludifex::BodyType::Static,
                .IsSensor = true,
                .Name = "Chime",
            });
            chime.Bell.SetColor(idle);

            // Sensors are hidden by default, since trigger volumes are usually
            // invisible. These are bells, so they are shown.
            chime.Bell.SetVisible(true);

            const int index = row * GridSize + column;
            chime.Sound = notes[static_cast<size_t>((row * 2 + column * 3) % static_cast<int>(notes.size()))];
            chime.Pitch = (index % 3 == 0) ? 0.5f : 1.0f;

            // Staggered, so the chimes do not all strike at once.
            chime.StartAt = 0.2f + static_cast<float>((index * 37) % 49) * 0.061f;
            chimes.push_back(chime);
        }
    }

    // Walls along the lines halfway between the rows, so each one stands
    // between chimes rather than among them. Every chime asks for occlusion, so
    // walking behind a wall muffles whatever is on its far side.
    std::vector<ludifex::Actor3D> walls;
    auto BuildWalls = [&] {
        const float half = Spacing * 0.5f;
        const ludifex::Vec3 places[] = {
            { -half, 1.6f, -half },
            { half * 3.0f, 1.6f, half },
            { -half * 3.0f, 1.6f, half * 3.0f },
            { half, 1.6f, -half * 3.0f },
        };
        for (const ludifex::Vec3& place : places)
        {
            ludifex::Actor3D wall = world.AddBox({
                .Scale = { 10.0f, 3.2f, 0.5f },
                .Position = place,
                .Type = ludifex::BodyType::Static,
                .Name = "Wall",
            });
            wall.SetColor(ludifex::Color::FromBytes(150, 138, 124));
            walls.push_back(wall);
        }
    };
    BuildWalls();

    // The racer circles the field fast enough for the Doppler shift to be
    // plain: it rises as it comes and falls as it goes. Its priority keeps it
    // mixed however many chimes are in range.
    ludifex::Actor3D racer = world.AddSphere({
        .Radius = 0.6f,
        .Position = { RacerRadius, 0.9f, 0.0f },
        .Type = ludifex::BodyType::Kinematic,
        .Name = "Racer",
    });
    racer.SetColor(ludifex::Color::FromBytes(110, 200, 255));
    ludifex::VoiceId racerVoice = world.PlaySoundAt(hum, racer, {
        .Volume = 0.8f,
        .Looping = true,
        .MinDistance = 2.0f,
        .MaxDistance = 45.0f,
        .Priority = 1,
    });
    float racerAngle = 0.0f;

    // The listener: a marker on the ground, with a nose showing which way it
    // faces. A sensor, so the ears inside it are not muffled by their own head.
    ludifex::Actor3D listener = world.AddCapsule({
        .Radius = 0.35f,
        .Height = 1.7f,
        .Position = { 0.0f, 0.85f, FieldHalf + 4.0f },
        .Type = ludifex::BodyType::Kinematic,
        .IsSensor = true,
        .Name = "Listener",
    });
    listener.SetColor(ludifex::Color::FromBytes(240, 240, 250));
    listener.SetVisible(true);

    ludifex::Vec3 position{ 0.0f, 0.0f, FieldHalf + 4.0f };
    float heading = 3.14159265f; // facing -Z, into the field
    float autoTime = 0.0f;

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

    AddLabel("Voices", false, 28.0f);
    opane::Label* mixing = AddLabel("", false);
    opane::Label* waiting = AddLabel("", true);
    opane::Label* muffled = AddLabel("", true);
    opane::Label* gaveWay = AddLabel("", true);
    opane::Label* culled = AddLabel("", true);

    opane::Label* limitLabel = AddLabel("", true);
    opane::Slider* limit = sidebar->Add<opane::Slider>();
    limit->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    limit->Minimum = 1.0f;
    limit->Maximum = 24.0f;
    limit->Value = 12.0f;
    limit->OnChanged = [&](float value) { audio.MaxVoices = static_cast<int>(std::lround(value)); };

    opane::Checkbox* autoFly = sidebar->Add<opane::Checkbox>();
    autoFly->Text = "Walk on its own";
    autoFly->Checked = true;
    autoFly->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    opane::Checkbox* wallsBox = sidebar->Add<opane::Checkbox>();
    wallsBox->Text = "Walls";
    wallsBox->Checked = true;
    wallsBox->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    wallsBox->OnChanged = [&](bool enabled) {
        if (enabled)
        {
            BuildWalls();
            return;
        }
        for (ludifex::Actor3D& wall : walls)
        {
            wall.Destroy();
        }
        walls.clear();
    };

    opane::Checkbox* racerBox = sidebar->Add<opane::Checkbox>();
    racerBox->Text = "Racer";
    racerBox->Checked = true;
    racerBox->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    racerBox->OnChanged = [&](bool enabled) {
        racer.SetVisible(enabled);
        if (enabled)
        {
            racerVoice = world.PlaySoundAt(hum, racer, {
                .Volume = 0.8f,
                .Looping = true,
                .MinDistance = 2.0f,
                .MaxDistance = 45.0f,
                .Priority = 1,
            });
        }
        else
        {
            world.StopSound(racerVoice, 0.3f);
        }
    };

    opane::Checkbox* linesBox = sidebar->Add<opane::Checkbox>();
    linesBox->Text = "Lines to mixed voices";
    linesBox->Checked = true;
    linesBox->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    // Where recent pings rang, drawn for a moment each.
    struct Flash
    {
        ludifex::Vec3 At;
        float Left = 0.0f;
    };
    std::vector<Flash> flashes;

    std::mt19937 random(7);
    auto Ping = [&] {
        // Anywhere within 50 m: the near ones ring, the far ones are culled
        // before they cost anything.
        std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
        std::uniform_real_distribution<float> distance(3.0f, 50.0f);
        const float a = angle(random);
        const float d = distance(random);
        const ludifex::Vec3 at{ position.X + std::cos(a) * d, 1.0f, position.Z + std::sin(a) * d };
        const ludifex::VoiceId voice = world.PlaySoundAt(ping, at, { .MaxDistance = 25.0f, .Priority = 2 });
        if (voice.IsValid())
        {
            flashes.push_back({ at, 0.6f });
        }
    };

    opane::Button* pingButton = sidebar->Add<opane::Button>();
    pingButton->Text = "Ping somewhere (Space)";
    pingButton->Accent = true;
    pingButton->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };
    pingButton->OnClick = [&] { Ping(); };

    AddLabel("W / S walk, A / D turn", true);

    opane::Viewport* viewport = root->Add<opane::Viewport>();
    viewport->Size = opane::Size2{ opane::Dim{ 1.0f, -300.0f }, opane::Dim::FromScale(1.0f) };
    viewport->SetWorld(world);

    float clock = 0.0f;

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
        clock += deltaSeconds;

        // Chimes start one by one.
        for (Chime& chime : chimes)
        {
            if (!chime.Voice.IsValid() && clock >= chime.StartAt)
            {
                chime.Voice = world.PlaySoundAt(chime.Sound, chime.Bell, {
                    .Volume = 0.9f,
                    .Pitch = chime.Pitch,
                    .Looping = true,
                    .MinDistance = 1.5f,
                    .MaxDistance = 16.0f,
                    .Occlusion = true,
                });
            }
        }

        // --- the listener walks ----------------------------------------------

        const bool walking = input.IsKeyDown(opane::Key::W) || input.IsKeyDown(opane::Key::S) ||
                             input.IsKeyDown(opane::Key::A) || input.IsKeyDown(opane::Key::D) ||
                             input.IsKeyDown(opane::Key::Up) || input.IsKeyDown(opane::Key::Down) ||
                             input.IsKeyDown(opane::Key::Left) || input.IsKeyDown(opane::Key::Right);
        if (walking)
        {
            autoFly->Checked = false;
        }

        if (autoFly->Checked)
        {
            // A slow figure of eight through the field.
            autoTime += deltaSeconds * 0.09f;
            const ludifex::Vec3 next{ std::sin(autoTime) * (FieldHalf + 2.0f), 0.0f,
                                      std::sin(autoTime * 2.0f) * (FieldHalf * 0.8f) };
            const float dx = next.X - position.X;
            const float dz = next.Z - position.Z;
            if (dx * dx + dz * dz > 1e-6f)
            {
                heading = std::atan2(dx, dz);
            }
            position = next;
        }
        else
        {
            const float turn = (input.IsKeyDown(opane::Key::A) || input.IsKeyDown(opane::Key::Left) ? 1.0f : 0.0f) -
                               (input.IsKeyDown(opane::Key::D) || input.IsKeyDown(opane::Key::Right) ? 1.0f : 0.0f);
            const float walk = (input.IsKeyDown(opane::Key::W) || input.IsKeyDown(opane::Key::Up) ? 1.0f : 0.0f) -
                               (input.IsKeyDown(opane::Key::S) || input.IsKeyDown(opane::Key::Down) ? 1.0f : 0.0f);
            heading += turn * 2.2f * deltaSeconds;
            position.X += std::sin(heading) * walk * 6.0f * deltaSeconds;
            position.Z += std::cos(heading) * walk * 6.0f * deltaSeconds;
        }

        if (input.WasKeyPressed(opane::Key::Space))
        {
            Ping();
        }

        const ludifex::Vec3 forward{ std::sin(heading), 0.0f, std::cos(heading) };
        listener.SetPosition({ position.X, 0.85f, position.Z });

        // The ears are at head height on the marker, facing where it faces.
        const ludifex::Vec3 ears{ position.X, 1.6f, position.Z };
        world.SetListener(ears, forward);

        ludifex::Camera3D& camera = world.GetCamera();
        camera.Position = { position.X - forward.X * 17.0f, 14.0f, position.Z - forward.Z * 17.0f };
        camera.Target = { position.X + forward.X * 7.0f, 1.0f, position.Z + forward.Z * 7.0f };
        world.DrawLine(ears, { ears.X + forward.X * 1.4f, ears.Y, ears.Z + forward.Z * 1.4f },
                       ludifex::Color::FromBytes(255, 255, 255));

        // --- the racer circles ---------------------------------------------------
        //
        // Driven by velocity rather than teleported, so the audio sees how fast
        // it moves. The radial term holds it on the circle.

        if (racerBox->Checked)
        {
            racerAngle += RacerSpeed / RacerRadius * deltaSeconds;
            const ludifex::Vec3 goal{ std::cos(racerAngle) * RacerRadius, 0.9f, std::sin(racerAngle) * RacerRadius };
            const ludifex::Vec3 now = racer.GetPosition();
            const float correction = 4.0f;
            racer.SetLinearVelocity({
                -std::sin(racerAngle) * RacerSpeed + (goal.X - now.X) * correction,
                (goal.Y - now.Y) * correction,
                std::cos(racerAngle) * RacerSpeed + (goal.Z - now.Z) * correction,
            });
        }
        else
        {
            racer.SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        }

        // --- what is being heard ---------------------------------------------------

        const float ease = std::min(1.0f, deltaSeconds * 8.0f);
        for (Chime& chime : chimes)
        {
            const bool heard = chime.Voice.IsValid() && world.IsAudible(chime.Voice);
            const bool behindAWall = heard && world.IsOccluded(chime.Voice);
            const ludifex::Color target = heard ? (behindAWall ? dulled : lit) : idle;
            chime.Bell.SetColor(Mix(chime.Bell.GetColor(), target, ease));
            if (heard && linesBox->Checked)
            {
                world.DrawLine(ears, chime.Bell.GetPosition(), behindAWall ? dulled : lit);
            }
        }
        if (racerBox->Checked && world.IsAudible(racerVoice) && linesBox->Checked)
        {
            world.DrawLine(ears, racer.GetPosition(), ludifex::Color::FromBytes(110, 200, 255));
        }

        for (Flash& flash : flashes)
        {
            flash.Left -= deltaSeconds;
            world.DrawSphere(flash.At, 0.3f + (0.6f - flash.Left), ludifex::Color::FromBytes(255, 120, 200));
        }
        std::erase_if(flashes, [](const Flash& flash) { return flash.Left <= 0.0f; });

        const ludifex::AudioStats stats = world.GetAudioStats();
        char line[96];
        std::snprintf(line, sizeof(line), "Mixing %d of %d", stats.Playing, audio.MaxVoices);
        mixing->Text = line;
        std::snprintf(line, sizeof(line), "Waiting silently: %d", stats.Virtual);
        waiting->Text = line;
        std::snprintf(line, sizeof(line), "Muffled by walls: %d", stats.Occluded);
        muffled->Text = line;
        std::snprintf(line, sizeof(line), "Gave way so far: %d", stats.Stolen);
        gaveWay->Text = line;
        std::snprintf(line, sizeof(line), "Pings too far to start: %d", stats.Culled);
        culled->Text = line;
        std::snprintf(line, sizeof(line), "Voice limit: %d", audio.MaxVoices);
        limitLabel->Text = line;
    });

    app.Shutdown();
    return 0;
}
