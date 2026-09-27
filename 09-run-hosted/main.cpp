// Example 09: the shortest program that uses both libraries.
//
// There is no AdoptHost call and no frame loop. StartApp publishes opane's
// device, window, and loop through SDL's shared properties, and world.Run()
// finds them and runs the world inside opane's window until it closes. Neither
// library includes the other's headers.
//
// The label is an ordinary opane element drawn over the world.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

int main()
{
    opane::App app = opane::StartApp({ .Title = "ludifex: hosted by opane", .Width = 1100, .Height = 660 });
    if (!app.IsValid())
    {
        return 1;
    }

    opane::Label* label = app.GetRoot()->Add<opane::Label>();
    label->Text = "An opane label, drawn over a ludifex world that found this window by itself";
    label->Position = opane::Position2::FromOffset(24.0f, 20.0f);
    label->Size = opane::Size2::FromOffset(700.0f, 28.0f);

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddGround();
    for (int index = 0; index < 12; ++index)
    {
        world.AddBox({ .Position = { -2.0f + static_cast<float>(index % 4), 2.0f + static_cast<float>(index), 0.0f } });
    }
    world.AddSphere({ .Radius = 0.7f, .Position = { 0.3f, 16.0f, 0.2f }, .Restitution = 0.5f });

    world.Run();

    app.Shutdown();
    return 0;
}
