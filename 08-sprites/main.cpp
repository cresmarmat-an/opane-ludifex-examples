// Example 08: sprites.
//
// A small platformer using the 2D sprite API:
//
//   the sky        a background image, cropped to fill the view at any size
//   the character  a four-frame sprite sheet, animated with SetFrame and
//                  mirrored with SetFlipX when it turns; FixedRotation keeps it
//                  upright
//   coins          sensor sprites with circle colliders; touching one is a
//                  trigger event, and the coin is destroyed inside the handler
//   scenery        sprites without colliders, on layers behind (-1) and in
//                  front of (+1) the level, which sets the drawing order
//
// Every image is generated at startup and loaded by name through an asset
// root, the same way shipped files would be.
//
// Controls:
//   Left, Right   run
//   Space         jump
//   R             put the coins back
//   Escape        quit

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include "../common/Png.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

using Pixels = std::vector<uint8_t>;

void Put(Pixels& pixels, int width, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    uint8_t* texel = &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4];
    texel[0] = r;
    texel[1] = g;
    texel[2] = b;
    texel[3] = a;
}

// A disc with a one-pixel soft edge, blended over what is already there.
void Disc(Pixels& pixels, int width, int height, float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dy = static_cast<float>(y) + 0.5f - cy;
            const float coverage = std::clamp(radius - std::sqrt(dx * dx + dy * dy) + 0.5f, 0.0f, 1.0f);
            if (coverage <= 0.0f)
            {
                continue;
            }
            uint8_t* texel = &pixels[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4];
            const float existing = texel[3] / 255.0f;
            const float alpha = coverage + existing * (1.0f - coverage);
            auto Mix = [&](uint8_t source, uint8_t destination) {
                if (alpha <= 0.0f)
                {
                    return static_cast<uint8_t>(0);
                }
                return static_cast<uint8_t>((source * coverage + destination * existing * (1.0f - coverage)) / alpha);
            };
            texel[0] = Mix(r, texel[0]);
            texel[1] = Mix(g, texel[1]);
            texel[2] = Mix(b, texel[2]);
            texel[3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }
}

// A vertical sky gradient with soft clouds.
void WriteSky(const std::string& path)
{
    constexpr int Width = 512;
    constexpr int Height = 256;
    Pixels pixels(static_cast<size_t>(Width) * Height * 4);
    for (int y = 0; y < Height; ++y)
    {
        const float t = static_cast<float>(y) / Height;
        for (int x = 0; x < Width; ++x)
        {
            Put(pixels, Width, x, y, static_cast<uint8_t>(70 + t * 120), static_cast<uint8_t>(130 + t * 90),
                static_cast<uint8_t>(210 + t * 35), 255);
        }
    }
    const float clouds[][3] = { { 90, 60, 26 }, { 130, 70, 32 }, { 330, 50, 24 }, { 370, 58, 30 }, { 410, 64, 22 } };
    for (const auto& cloud : clouds)
    {
        Disc(pixels, Width, Height, cloud[0], cloud[1], cloud[2], 250, 252, 255);
    }
    examples::WritePng(path, Width, Height, pixels);
}

// Four frames side by side: a round character whose feet swing as it runs.
void WriteRunner(const std::string& path)
{
    constexpr int Frame = 64;
    constexpr int Width = Frame * 4;
    constexpr int Height = Frame;
    Pixels pixels(static_cast<size_t>(Width) * Height * 4, 0);

    for (int frame = 0; frame < 4; ++frame)
    {
        const float x0 = static_cast<float>(frame * Frame);
        const float swing = std::sin(static_cast<float>(frame) * 3.14159f * 0.5f) * 9.0f;

        // Feet first, so the body is drawn over them.
        Disc(pixels, Width, Height, x0 + 24.0f + swing, 56.0f, 6.0f, 60, 50, 90);
        Disc(pixels, Width, Height, x0 + 40.0f - swing, 56.0f, 6.0f, 60, 50, 90);
        Disc(pixels, Width, Height, x0 + 32.0f, 32.0f, 22.0f, 250, 120, 80);

        // One eye, toward the direction of travel, so flipping reads clearly.
        Disc(pixels, Width, Height, x0 + 42.0f, 26.0f, 6.0f, 255, 255, 255);
        Disc(pixels, Width, Height, x0 + 44.0f, 27.0f, 3.0f, 30, 30, 40);
    }
    examples::WritePng(path, Width, Height, pixels);
}

void WriteCoin(const std::string& path)
{
    constexpr int Size = 48;
    Pixels pixels(static_cast<size_t>(Size) * Size * 4, 0);
    Disc(pixels, Size, Size, 24.0f, 24.0f, 22.0f, 200, 140, 20);
    Disc(pixels, Size, Size, 24.0f, 24.0f, 17.0f, 255, 205, 60);
    Disc(pixels, Size, Size, 19.0f, 18.0f, 5.0f, 255, 245, 190);
    examples::WritePng(path, Size, Size, pixels);
}

void WriteBush(const std::string& path)
{
    constexpr int Width = 128;
    constexpr int Height = 64;
    Pixels pixels(static_cast<size_t>(Width) * Height * 4, 0);
    Disc(pixels, Width, Height, 30.0f, 44.0f, 22.0f, 40, 120, 60);
    Disc(pixels, Width, Height, 64.0f, 34.0f, 30.0f, 50, 140, 70);
    Disc(pixels, Width, Height, 98.0f, 44.0f, 22.0f, 40, 120, 60);
    examples::WritePng(path, Width, Height, pixels);
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "ludifex: sprites",
        .Width = 1100,
        .Height = 640,
    });

    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::AdoptHost({ app.GetGpuDevice(), app.GetWindow() });

    const std::filesystem::path assets = std::filesystem::temp_directory_path() / "ludifex-sprite-assets";
    std::filesystem::create_directories(assets);
    WriteSky((assets / "sky.png").string());
    WriteRunner((assets / "runner.png").string());
    WriteCoin((assets / "coin.png").string());
    WriteBush((assets / "bush.png").string());
    ludifex::AddAssetRoot(assets.string());

    ludifex::World2D world = ludifex::CreateWorld2D({ .Gravity = { 0.0f, -25.0f } });
    world.SetBackground("sky.png");

    ludifex::Camera2D& camera = world.GetCamera();
    camera.Center = { 0.0f, 3.5f };
    camera.Height = 10.0f;

    // --- the level -----------------------------------------------------------

    ludifex::Actor2D ground = world.AddGround({ .Width = 60.0f, .Name = "ground" });
    ground.SetColor(ludifex::Color::FromBytes(92, 72, 58));

    const float platforms[][3] = { { -5.0f, 2.2f, 3.0f }, { 1.5f, 3.6f, 2.5f }, { 7.0f, 2.6f, 3.0f } };
    for (const auto& platform : platforms)
    {
        ludifex::Actor2D slab = world.AddRectangle({
            .Width = platform[2],
            .Height = 0.4f,
            .Position = { platform[0], platform[1] },
            .Type = ludifex::BodyType::Static,
            .Friction = 0.8f,
            .Name = "platform",
        });
        slab.SetColor(ludifex::Color::FromBytes(120, 96, 72));
    }

    // --- scenery, behind and in front ----------------------------------------

    for (int index = 0; index < 4; ++index)
    {
        world.AddSprite({
            .Path = "bush.png",
            .Position = { -9.0f + static_cast<float>(index) * 6.0f, 0.55f },
            .Size = { 2.4f, 0.0f },
            .Type = ludifex::BodyType::Static,
            .Collider = ludifex::SpriteCollider::None,
            .Layer = -1,
            .Name = "bush behind",
        });
    }
    ludifex::Actor2D frontBush = world.AddSprite({
        .Path = "bush.png",
        .Position = { 3.0f, 0.5f },
        .Size = { 2.6f, 0.0f },
        .Type = ludifex::BodyType::Static,
        .Collider = ludifex::SpriteCollider::None,
        .Layer = 1,
        .Name = "bush in front",
    });
    frontBush.SetColor(ludifex::Color::FromBytes(210, 255, 210));

    // --- the runner ------------------------------------------------------------

    ludifex::Actor2D runner = world.AddSprite({
        .Path = "runner.png",
        .Position = { -8.0f, 1.0f },
        .Size = { 1.0f, 1.0f },
        .Collider = ludifex::SpriteCollider::Circle,
        .Friction = 0.0f,
        .FixedRotation = true,
        .Name = "runner",
    });
    runner.SetFrame(0, 4, 1);

    // --- coins -------------------------------------------------------------------

    int collected = 0;
    std::vector<ludifex::Actor2D> coins;
    auto PlaceCoins = [&]() {
        for (ludifex::Actor2D& coin : coins)
        {
            coin.Destroy();
        }
        coins.clear();

        const float spots[][2] = { { -5.5f, 3.0f }, { -4.5f, 3.0f }, { 1.0f, 4.4f }, { 2.0f, 4.4f },
                                   { 6.5f, 3.4f },  { 7.5f, 3.4f },  { 10.0f, 1.0f } };
        for (const auto& spot : spots)
        {
            ludifex::Actor2D coin = world.AddSprite({
                .Path = "coin.png",
                .Position = { spot[0], spot[1] },
                .Size = { 0.55f, 0.55f },
                .Type = ludifex::BodyType::Static,
                .Collider = ludifex::SpriteCollider::Circle,
                .IsSensor = true,
                .Name = "coin",
            });

            coin.WhenEntered([&, coin](const ludifex::TriggerInfo2D& info) mutable {
                if (info.Other == runner)
                {
                    ++collected;

                    // Destroying from inside a handler is queued until the next
                    // sync point, so it is safe here.
                    coin.Destroy();
                }
            });
            coins.push_back(coin);
        }
    };
    PlaceCoins();

    // --- interface ---------------------------------------------------------------

    opane::Label* score = app.GetRoot()->Add<opane::Label>();
    score->Position = opane::Position2::FromOffset(24.0f, 20.0f);
    score->Size = opane::Size2::FromOffset(420.0f, 28.0f);

    opane::Label* keys = app.GetRoot()->Add<opane::Label>();
    keys->Text = "Left / Right run    Space jump    R coins    Esc quit";
    keys->Muted = true;
    keys->Align = opane::TextAlign::Center;
    keys->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };
    keys->Position = opane::Position2{ opane::Dim::FromScale(0.0f), opane::Dim{ 1.0f, -34.0f } };

    opane::TextureId worldView;
    int lastWidth = 0;
    int lastHeight = 0;
    float animation = 0.0f;

    app.Run([&](float deltaSeconds) {
        const opane::Input& input = app.GetInput();
        if (input.WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
        if (input.WasKeyPressed(opane::Key::R))
        {
            PlaceCoins();
            collected = 0;
        }

        // Running sets the horizontal speed directly, so the character
        // responds at once; gravity still controls the vertical speed.
        float direction = 0.0f;
        if (input.IsKeyDown(opane::Key::Left))
        {
            direction -= 1.0f;
        }
        if (input.IsKeyDown(opane::Key::Right))
        {
            direction += 1.0f;
        }

        ludifex::Vec2 velocity = runner.GetLinearVelocity();
        velocity.X = direction * 6.0f;

        // A short ray straight down from the feet decides whether a jump is
        // allowed. The runner's own collider is skipped by starting the ray
        // just below it.
        const ludifex::Vec2 feet{ runner.GetPosition().X, runner.GetPosition().Y - 0.52f };
        const bool grounded = world.CastRay(feet, { 0.0f, -1.0f }, 0.1f).Hit;
        if (grounded && input.WasKeyPressed(opane::Key::Space))
        {
            velocity.Y = 11.0f;
        }
        runner.SetLinearVelocity(velocity);

        if (direction != 0.0f)
        {
            runner.SetFlipX(direction < 0.0f);
            animation += deltaSeconds * 10.0f;
        }
        runner.SetFrame(direction != 0.0f ? static_cast<int>(animation) : 0, 4, 1);

        // The camera follows, clamped so it never shows below the ground.
        camera.Center.X += (runner.GetPosition().X - camera.Center.X) * std::min(1.0f, deltaSeconds * 4.0f);

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

        world.Update(deltaSeconds);
        world.Render();

        if (worldView.IsValid())
        {
            app.GetDrawList().DrawTexture({ 0.0f, 0.0f, windowSize.X, windowSize.Y }, worldView);
        }

        char text[96];
        std::snprintf(text, sizeof(text), "Coins %d / 7   %s", collected, grounded ? "on the ground" : "in the air");
        score->Text = text;
    });

    app.Shutdown();
    return 0;
}
