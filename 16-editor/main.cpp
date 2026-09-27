// Example 16: an editor layout.
//
// A menu bar, a dock space holding a scene view, a hierarchy, an inspector, a
// console, and an asset list, and a status bar. Each panel can be dragged by
// its tab onto another group's guides to split or join it, or into empty space
// to float, and back again. The layout is saved when the program closes and
// restored when it starts.
//
// The scene is a ludifex world in a docked panel. It keeps running wherever
// the panel is dragged, including into a floating window, because moving a
// panel does not recreate it.

#include <ludifex/ludifex.h>
#include <opane/opane.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    file << text;
}

} // namespace

int main(int argumentCount, char** arguments)
{
    // "--scale 1.5" forces the interface scale; without it the display's own
    // scale is followed.
    float scale = 0.0f;
    for (int index = 1; index + 1 < argumentCount; ++index)
    {
        if (std::string(arguments[index]) == "--scale")
        {
            scale = static_cast<float>(std::atof(arguments[index + 1]));
        }
    }

    opane::App app =
        opane::StartApp({ .Title = "opane: an editor", .Width = 1240, .Height = 740, .UiScale = scale });
    if (!app.IsValid())
    {
        return 1;
    }

    ludifex::World3D world = ludifex::CreateWorld3D();
    world.AddGround({ .Width = 30.0f, .Depth = 30.0f });
    for (int index = 0; index < 12; ++index)
    {
        ludifex::Actor3D box = world.AddBox({ .Position = { static_cast<float>(index % 4) - 1.5f,
                                                            1.0f + static_cast<float>(index / 4) * 1.1f, 0.0f } });
        box.SetColor({ 0.3f + 0.05f * static_cast<float>(index), 0.55f, 0.85f, 1.0f });
    }
    world.SetCamera({ .Position = { 6.0f, 5.0f, 9.0f }, .Target = { 0.0f, 1.0f, 0.0f } });

    const std::filesystem::path layoutFile = std::filesystem::temp_directory_path() / "opane-editor-layout.txt";

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Vertical;

    // --- the dock space and its panels ----------------------------------------

    opane::MenuBar* menu = root->Add<opane::MenuBar>();

    opane::DockSpace* dock = root->Add<opane::DockSpace>();
    dock->Flex = 1.0f;

    opane::DockPanel* scene = dock->AddPanel("Scene");
    opane::Viewport* viewport = scene->Add<opane::Viewport>();
    viewport->SetWorld(world);
    viewport->DrawBorder = false;

    opane::DockPanel* hierarchy = dock->AddPanel("Hierarchy", opane::DockSide::Left, nullptr, 0.2f);
    opane::TreeView* tree = hierarchy->Add<opane::TreeView>();
    tree->Items = {
        { "World", 1,
          { { "Camera", 2, {} },
            { "Sun", 3, {} },
            { "Ground", 4, {} },
            { "Crates", 5,
              { { "Crate 1", 6, {} }, { "Crate 2", 7, {} }, { "Crate 3", 8, {} }, { "Crate 4", 9, {} } },
              true } },
          true },
    };

    opane::DockPanel* inspector = dock->AddPanel("Inspector", opane::DockSide::Right, nullptr, 0.24f);
    inspector->ChildLayout = opane::LayoutMode::Vertical;
    inspector->Padding = 12.0f;
    inspector->Spacing = 8.0f;

    auto AddLabel = [&](opane::Element* parent, const std::string& text) {
        opane::Label* label = parent->Add<opane::Label>();
        label->Text = text;
        label->Muted = true;
        label->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(20.0f) };
        return label;
    };

    opane::Label* selected = AddLabel(inspector, "Nothing selected");
    selected->Muted = false;

    AddLabel(inspector, "Position");
    for (const char* axis : { "X", "Y", "Z" })
    {
        opane::NumberField* field = inspector->Add<opane::NumberField>();
        field->Prefix = axis;
        field->Step = 0.05;
        field->Tooltip = "Drag sideways to change, double-click to type";
    }

    AddLabel(inspector, "Material");
    opane::Dropdown* material = inspector->Add<opane::Dropdown>();
    material->Options = { "Painted wood", "Brushed metal", "Rubber", "Glass" };
    material->Selected = 0;

    opane::Slider* roughness = inspector->Add<opane::Slider>();
    roughness->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };
    roughness->Tooltip = "Roughness";

    opane::Toggle* physics = inspector->Add<opane::Toggle>();
    physics->Text = "Simulate physics";
    physics->On = true;

    opane::Checkbox* shadows = inspector->Add<opane::Checkbox>();
    shadows->Text = "Casts shadows";
    shadows->Checked = true;

    AddLabel(inspector, "Body");
    for (const char* kind : { "Dynamic", "Kinematic", "Static" })
    {
        opane::RadioButton* radio = inspector->Add<opane::RadioButton>();
        radio->Text = kind;
        radio->Group = "body";
        radio->Checked = std::string(kind) == "Dynamic";
    }

    opane::DockPanel* console = dock->AddPanel("Console", opane::DockSide::Bottom, scene, 0.28f);
    console->ChildLayout = opane::LayoutMode::Vertical;
    opane::ListView* output = console->Add<opane::ListView>();
    output->Flex = 1.0f;
    output->Items = { "ludifex: world created", "opane: interface ready", "Drag any tab to rearrange" };
    opane::TextInput* command = console->Add<opane::TextInput>();
    command->Placeholder = "Type a command and press Enter";
    command->OnSubmitted = [&](const std::string& text) {
        if (!text.empty())
        {
            output->Items.push_back("> " + text);
            output->ScrollTo(static_cast<int>(output->Items.size()) - 1);
            command->Text.clear();
        }
    };

    opane::DockPanel* notes = dock->AddPanel("Notes", opane::DockSide::Center, console);
    opane::TextArea* noteText = notes->Add<opane::TextArea>();
    noteText->Size = opane::Size2::Fill();
    noteText->Text = "Notes are a TextArea: several lines, word wrap, undo.\n\n"
                     "Try dragging this tab out into the open to float it, then drag the window "
                     "by its title back onto one of the guides that appear.";

    opane::DockPanel* assets = dock->AddPanel("Assets", opane::DockSide::Center, hierarchy);
    opane::ListView* assetList = assets->Add<opane::ListView>();
    for (int index = 0; index < 500; ++index)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "texture_%03d.png", index);
        assetList->Items.push_back(name);
    }
    dock->Activate(hierarchy);

    // --- the status bar -----------------------------------------------------------

    opane::Panel* status = root->Add<opane::Panel>();
    status->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(30.0f) };
    status->ChildLayout = opane::LayoutMode::Horizontal;
    status->Padding = 5.0f;
    status->Spacing = 10.0f;
    status->CornerRadius = 0.0f;
    opane::Label* statusText = status->Add<opane::Label>();
    statusText->Text = "Ready";
    statusText->Flex = 1.0f;
    opane::ProgressBar* progress = status->Add<opane::ProgressBar>();
    progress->Size = opane::Size2::FromOffset(220.0f, 0.0f);
    progress->Text = "Building {}%";

    // --- menus ---------------------------------------------------------------------

    auto Log = [&](const std::string& line) {
        output->Items.push_back(line);
        output->ScrollTo(static_cast<int>(output->Items.size()) - 1);
    };

    std::vector<opane::MenuItem> windowItems;
    for (opane::DockPanel* panel : dock->GetPanels())
    {
        windowItems.push_back(opane::MenuItem::Action(panel->Title, [dock, panel]() { dock->Show(panel); }));
    }

    menu->Menus = {
        { "File",
          {
              opane::MenuItem::Action("Save layout", [&]() { WriteFile(layoutFile, dock->SaveLayout()); Log("Layout saved"); }, "Ctrl+S"),
              opane::MenuItem::Action("Load layout", [&]() {
                  Log(dock->LoadLayout(ReadFile(layoutFile)) ? "Layout loaded" : "No saved layout");
              }, "Ctrl+L"),
              opane::MenuItem::Divider(),
              opane::MenuItem::Action("Quit", [&]() {
                  app.ShowDialog("Quit?", "The layout is saved as the editor closes.", { "Quit", "Cancel" },
                                 [&](int button) {
                                     if (button == 0)
                                     {
                                         app.Close();
                                     }
                                 });
              }, "Ctrl+Q"),
          } },
        { "View",
          {
              opane::MenuItem::Check("Show shadows", true, [&]() { shadows->Checked = !shadows->Checked; }),
              opane::MenuItem::Submenu("Panels", windowItems),
          } },
        { "Help",
          {
              opane::MenuItem::Action("About", [&]() {
                  app.ShowDialog("About", "opane and ludifex: an interface and a world, sharing one device.");
              }, "F1"),
          } },
    };

    tree->OnSelected = [&](const opane::TreeView::Item& item) { selected->Text = item.Text; };
    tree->OnContextMenu = [&](const opane::TreeView::Item& item, opane::Vec2 at) {
        const std::string name = item.Text;
        app.ShowMenu({ opane::MenuItem::Action("Rename", [&, name]() { Log("Rename " + name); }),
                       opane::MenuItem::Action("Duplicate", [&, name]() { Log("Duplicate " + name); }),
                       opane::MenuItem::Divider(),
                       opane::MenuItem::Action("Delete", [&, name]() { Log("Delete " + name); }) },
                     at);
    };

    // Restore the last arrangement, if there is one.
    if (std::filesystem::exists(layoutFile))
    {
        dock->LoadLayout(ReadFile(layoutFile));
    }

    float clock = 0.0f;
    app.Run([&](float deltaSeconds) {
        clock += deltaSeconds;
        progress->Value = std::fmod(clock * 0.12f, 1.0f);
        if (physics->On != world.IsPhysicsRunning())
        {
            if (physics->On)
            {
                world.StartPhysics();
            }
            else
            {
                world.StopPhysics();
            }
        }

        char line[96];
        std::snprintf(line, sizeof(line), "%zu panels, %s", dock->GetPanels().size(),
                      dock->IsFloating(scene) ? "the scene is floating" : "the scene is docked");
        statusText->Text = line;
    });

    WriteFile(layoutFile, dock->SaveLayout());
    app.Shutdown();
    return 0;
}
