// Uses opane and ludifex from their installed packages and checks what a
// developer relies on: both libraries start, materials compiled by the build
// load, a material compiled at runtime finds its includes and its compiler, a
// world steps and renders inside the interface, and nothing logs an error.

#include <ludifex/ludifex.h>
#include <ludifex/native.h>
#include <opane/opane.h>

#include "PanelShader.h"
#include "ToonShader.h"

#include <cstdio>

// Last, so its macros cannot reach the libraries' headers.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{

int g_Failures = 0;
int g_Errors = 0;

void Check(bool condition, const char* description)
{
    std::printf("  [%s] %s\n", condition ? "pass" : "FAIL", description);
    if (!condition)
    {
        ++g_Failures;
    }
}

// opane_set_app_icon put an icon in the .exe, where Explorer looks for one.
bool ExecutableHasIcon()
{
#ifdef _WIN32
    return FindResourceW(nullptr, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(14)) != nullptr; // RT_GROUP_ICON
#else
    return true;
#endif
}

} // namespace

int main()
{
    std::printf("opane %s, ludifex %s\n", opane::VersionString, ludifex::VersionString);

    opane::SetLogHandler([](opane::LogLevel level, const char* category, const char* message) {
        g_Errors += level == opane::LogLevel::Error ? 1 : 0;
        std::printf("    (opane %s) %s\n", category, message);
    });
    ludifex::SetLogHandler([](ludifex::LogLevel level, const char* category, const char* message) {
        g_Errors += level == ludifex::LogLevel::Error ? 1 : 0;
        std::printf("    (ludifex %s) %s\n", category, message);
    });

    opane::App app = opane::StartApp({ .Title = "consumer", .Width = 640, .Height = 480, .Hidden = true });
    Check(app.IsValid(), "an installed opane starts");
    if (!app.IsValid())
    {
        return 1;
    }
    std::printf("drawing with %s\n", opane::GetGraphicsBackendName(app.GetGraphicsBackend()));

    Check(ExecutableHasIcon(), "opane_set_app_icon gave the program its icon");
    Check(app.SetIcon(CONSUMER_ICON), "and the window takes one at runtime");

    const opane::MaterialId baked = app.CreateMaterial({
        .Bytecode = PanelShader(),
        .Uniforms = { { "Top", opane::Color{ 0.2f, 0.4f, 0.9f, 1.0f } }, { "Bottom", opane::Color{ 0.9f, 0.3f, 0.2f, 1.0f } } },
    });
    Check(baked.IsValid(), "an interface material compiled by opane_add_material loads");

    const opane::MaterialId compiled = app.CreateMaterial({ .ShaderPath = CONSUMER_DIR "/panel.hlsl" });
    Check(compiled.IsValid(), "and one compiled at runtime finds its include and its compiler");

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddGround({ .Width = 20.0f, .Depth = 20.0f });
    ludifex::Actor3D box = world.AddBox({ .Position = { 0.0f, 3.0f, 0.0f } });

    const ludifex::MaterialId toon = ludifex::CreateMaterial({ .Bytecode = ToonShader() });
    Check(toon.IsValid(), "a world material compiled by ludifex_add_material loads");
    box.SetMaterial(toon);

    const ludifex::MaterialId toonAtRuntime = ludifex::CreateMaterial({ .ShaderPath = CONSUMER_DIR "/toon.hlsl" });
    Check(toonAtRuntime.IsValid(), "and one compiled at runtime finds its includes and its compiler");

    Check(b3World_IsValid(ludifex::native::GetNativeWorld(world)), "Box3D's own headers and library came with it");

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;
    auto* panel = root->Add<opane::Panel>();
    panel->Size = opane::Size2::FromOffset(200.0f, 480.0f);
    panel->Material = baked;
    auto* view = root->Add<opane::Viewport>();
    view->Flex = 1.0f;
    view->SetWorld(world);

    auto RunFrames = [&](int frames) {
        for (int frame = 0; frame < frames; ++frame)
        {
            app.PollEvents();
            world.Update(1.0f / 60.0f);
            app.UpdateInterface(1.0f / 60.0f);
            app.BeginFrame();
            app.EndFrame();
        }
    };
    RunFrames(90);

    Check(box.GetPosition().Y < 1.0f, "a world steps inside the interface, drawn with its material");

    // A preset, the sky, and every tracing pass, from the shaders the
    // installed package carries.
    world.SetGraphicsQuality(ludifex::GraphicsQuality::Ultra);
    ludifex::RenderSettings& settings = world.GetRenderSettings();
    settings.Sky.Enabled = true;
    settings.RayTracing.Shadows = true;
    settings.RayTracing.AmbientOcclusion = true;
    settings.RayTracing.Reflections = true;
    RunFrames(20);
    Check(world.IsRayTracingActive(), "an installed ludifex traces rays at a graphics preset");

    settings.PathTracing.Enabled = true;
    RunFrames(10);
    Check(world.GetPathTracedSampleCount() > 0, "and path traces");

    Check(g_Errors == 0, "and nothing reported an error");

    app.Shutdown();

    std::printf("%s\n", g_Failures == 0 ? "All consumer checks passed." : "SOME CONSUMER CHECKS FAILED.");
    return g_Failures == 0 ? 0 : 1;
}
