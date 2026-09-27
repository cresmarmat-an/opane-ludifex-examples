# opane + ludifex examples

Example programs that use [opane](https://github.com/cresmarmat-an/opane) and
[ludifex](https://github.com/cresmarmat-an/ludifex) together: opane provides
the window, interface, and input, and ludifex provides the worlds shown in
them.

The sounds, images, and models the programs use (including skins, morph
targets, and materials) are generated when they start, so the repository
contains no binary files.

Documentation: [opane](https://cresmarmat-an.github.io/opane/),
[ludifex](https://cresmarmat-an.github.io/ludifex/).

## Building and running

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release --parallel
```

The programs are written to `build/bin`, with `SDL3.dll` copied next to them.

The first configure downloads opane 0.0.1 and ludifex 0.0.1 along with SDL3,
stb, miniaudio, Box2D, Box3D, cgltf, and Assimp, which takes a few minutes.
Later configures reuse the download. To build against your own copies, add
`-DFETCHCONTENT_SOURCE_DIR_OPANE=path/to/opane` or
`-DFETCHCONTENT_SOURCE_DIR_LUDIFEX=path/to/ludifex` when configuring.

You need CMake 3.22 or later, a C++20 compiler, and the Windows SDK, whose
`dxc` compiles the shaders for Direct3D 12. If the
[Vulkan SDK](https://vulkan.lunarg.com) is installed, the shaders are also
compiled for Vulkan.

## The programs

| | |
| --- | --- |
| [01-falling-box](#01-falling-box) | A 3D world in an opane window: interpolation, collision sounds, and picking |
| [02-world-2d](#02-world-2d) | A 2D world with picking, sensors, and impacts |
| [03-viewport](#03-viewport) | A world inside a `Viewport` next to ordinary widgets |
| [04-model](#04-model) | A glTF model placed twenty times, with positional sound |
| [05-world-shaders](#05-world-shaders) | Three custom world shaders, with hot reload |
| [06-joints](#06-joints) | A bridge, a windmill, a lift, and a pendulum chain |
| [07-rendering](#07-rendering) | Shadows, textures, lamps, glass, and every anti-aliasing preset |
| [08-sprites](#08-sprites) | A small platformer made from sprites and a sprite sheet |
| [09-run-hosted](#09-run-hosted) | The shortest program that uses both libraries |
| [10-audio-field](#10-audio-field) | Forty-nine sound sources, the voice limit, walls, and the Doppler shift, shown on screen |
| [11-character](#11-character) | A character that walks, slides along walls, climbs steps, and stops at ledges |
| [12-scale](#12-scale) | Ten thousand objects, a thousand shaded actors, and 256 lights |
| [13-loading](#13-loading) | Models loaded in the foreground and in the background |
| [14-eight-programs](#14-eight-programs) | Eight short programs, one function each |
| [15-animation](#15-animation) | Skeletons, crossfades, two skins, and morph targets |
| [16-editor](#16-editor) | An editor with docking panels, a menu bar, and a world in a panel |
| [17-materials](#17-materials) | Normal, metallic-roughness, occlusion, and emissive maps |
| [18-graphics](#18-graphics) | Quality presets, ray tracing, and path tracing |
| [package-check](#package-check) | Both libraries used from their installed packages with `find_package` |

## 01-falling-box

A 3D world stepped at a fixed 60 Hz, drawn by ludifex into a texture that
opane shows, with a small panel of information on top.

| Key | Action |
| --- | --- |
| Space | Pause and resume physics (drawing continues while paused) |
| R | Reset the crate |
| D | Drop another crate |
| Click | Push the object under the pointer |
| Escape | Quit |

It shows how the two libraries share the GPU device, smooth motion between
physics steps, collision events that play sounds, and picking objects with a
ray. The crate falls 7.5 m and the collision event reports the impact speed
you would expect from that height.

## 02-world-2d

A 2D world: a stack of nine blocks on the ground and a catch zone to the
right. 2D worlds are drawn by the same renderer as 3D worlds, with an
orthographic camera.

- **Click a block** to pick it with `PickFromView` and knock it away from the
  side you clicked. The readout shows its name.
- **Click empty space** to drop a ball at that point, found with
  `ViewToWorld`.
- **The catch zone** is a sensor. Sensors are not drawn, so its outline is
  drawn over the world with `WorldToView`, and it counts what is inside from
  its enter and leave events.
- **Hard impacts** are counted by the world's collision handler, with the
  threshold raised to 3 m/s so resting contacts are ignored.

## 03-viewport

A sidebar of widgets on the left and a 3D world in a `Viewport` element on
the right.

`viewport->SetWorld(world)` is all the setup needed. From then on the
viewport sizes the world's image to fit, steps the world, draws it, and shows
it, and resizing the window or the sidebar resizes the world's image too. The
viewport converts pointer positions into the view coordinates `PickFromView`
expects, so clicking an object shows its name and distance in the sidebar.

The sidebar controls pause, respawn, gravity, and multisampling.

## 04-model

Loads a glTF model, written at startup as a glTF file with its data embedded,
and drops twenty copies of it.

The load message is printed once for twenty actors, because the model is
parsed once and shared. The readout shows the number of draw calls: the
twenty copies are drawn together as one instanced call.

Collisions play positional sound at the contact point, with the listener
following the camera.

## 05-world-shaders

Three custom world shaders, each written a different way:

- `stripes.hlsl` chooses a colour and calls `Shade()`, so the object is lit
  like everything else.
- `hologram.hlsl` ignores the built-in lighting and draws a rim glow with
  scan lines.
- `dissolve.hlsl` cuts the object away with noise. Its alpha is turned into
  multisample coverage, so the ragged edge is smoothed.

Sixteen actors with three materials are drawn in eight calls, shadows and
tone mapping included, because actors are grouped by material and mesh.

Edit any of the three shaders and save while the program runs. A valid change
shows up immediately; a broken one keeps the last working shader on screen
and logs the file, line, and column of the error.

## 06-joints

Four machines, one for each joint type:

- a **rope bridge**: eleven planks joined with hinges
- a **windmill**: a hinge with a motor, with its speed on a slider in the
  sidebar
- a **lift**: a slider with limits, whose motor reverses at each end
- a **pendulum chain**: five beads hung on distance joints

Joints are described in world space, with a pivot and an axis in the same
coordinates as the bodies, and nothing jumps when a joint is created.

Click to drop a ball onto whatever is under the pointer. Space pauses and R
rebuilds the scene.

## 07-rendering

One scene that shows the main rendering features:

- **pillars** under a fixed sun, so their shadows can be checked against the
  light direction
- **the floor**, a small checker texture tiled across forty metres, which
  shows whether mip maps and filtering are working
- **brick crates** with a texture on a primitive shape
- **glass** panels that are translucent, sorted, and blended
- **sixty-four spheres** sharing one custom material, each with its own colour
  set with `SetUniform`, drawn in one call
- **three coloured lamps** circling the spheres, and a fourth attached to a
  moving ball
- **a vignette** drawn by a post-process material after tone mapping

Keys 1 to 5 switch the anti-aliasing preset (Off, Fast, Balanced, High,
Temporal). S, F, P, and D toggle shadows, fog, the vignette, and physics debug
drawing. The readout shows draw calls, objects, lights, and frame time.

## 08-sprites

A small platformer: a sky background, a runner animated from a four-frame
sprite sheet that flips when it turns, platforms, bushes on layers behind and
in front of the level, and coins. The coins are sensor sprites that destroy
themselves from inside the trigger handler when the runner touches them.

Left and Right run, Space jumps (only when a short ray from the feet finds
ground), and R puts the coins back.

## 09-run-hosted

The shortest program that uses both libraries: `opane::StartApp`, a few
actors, and `world.Run()`, with no frame loop of its own. The world finds
opane's device, window, and loop and runs inside that window, under an opane
label.

## 10-audio-field

Forty-nine looping chimes stand on a 7 by 7 grid, and a listener walks among
them while the sidebar shows what the mixer is doing.

A chime lights up while its voice is being mixed, with a line drawn to the
listener. It goes dark while its voice is waiting silently, and turns dull
while a wall is between it and the listener. Raise or lower the voice limit to
see voices move between chimes. A racer circling outside the grid changes
pitch as it passes.

The listener is not at the camera here. It is the marker on the ground,
placed with `SetListener`, while the camera follows it from behind, as in a
third-person game.

| Key | Action |
| --- | --- |
| W / S | Walk forward and back |
| A / D | Turn |
| Space | Play a sound somewhere nearby; distant ones are culled |
| Escape | Quit |

Walking with the keys stops the automatic tour.

## 11-character

A capsule character that stops at walls, slides along them, climbs steps up
to its step height, and stops at ledges that are too tall.

Gravity and jumping are handled by the program, not the library: the library
only works out where the capsule can move. The readout shows what the
character is standing on, and the green mark at its feet is the ground normal.

| Key | Action |
| --- | --- |
| W / A / S / D | Walk, relative to the camera |
| Space | Jump |
| Drag, or Left / Right | Turn the camera |
| Escape | Quit |

## 12-scale

Three scenes that test larger numbers.

**Ten thousand objects** that share two meshes, so they are drawn in a
handful of calls. Objects behind the camera are not drawn at all.

**A thousand actors with one custom shader** and different values on each.
Per-actor values are stored with the transforms, so almost all of the visible
actors are drawn in one call. Turning on level of detail splits them across
the three mesh densities.

**256 point lights** over a hall of four hundred pillars. Clustered lighting
means each pixel is shaded only by the lights near it.

| Key | Action |
| --- | --- |
| 1, 2, 3 | Switch between the three scenes |
| Escape | Quit |

## 13-loading

Creates sixty model actors from twelve generated glTF files, either in the
foreground or in the background, and shows how long the call took and the
longest frame since. In the foreground the window stops responding while the
files load. In the background every actor appears at once as a plain box and
takes its model's shape when its file is ready.

| Key | Action |
| --- | --- |
| Buttons | Load either way and compare the longest frame |
| Escape | Quit |

## 14-eight-programs

Eight short programs, one function each. Run one by passing its number:

```bash
build/bin/14-eight-programs 5
```

With no number it runs 6.

| | |
| --- | --- |
| 1 | The simplest 3D program: a model, the ground, music, `world.Run()` |
| 2 | The simplest 2D program: a background, a sprite, a platform |
| 3 | A button drawn by a shader, with its speed animated on hover |
| 4 | A gauge written from scratch: its own measuring, painting, hit testing, and events |
| 5 | A custom world material and post-process shaders at two points in the frame |
| 6 | An interface controlling physics: pause, resume, and gravity on a slider |
| 7 | Changing a world from inside a collision handler |
| 8 | A hand-written loop: fixed steps, interpolation, and drawing |

The assets they use are generated into a temporary folder at startup, which
is added as an asset root, so names such as `"robot.gltf"` and `"click.wav"`
are found the same way as in a real program.

Each program that uses a shader checks that it compiled and prints the
result, because a material that fails to compile falls back to the built-in
shader and would otherwise go unnoticed.

## 15-animation

Skeletal animation and blending between clips.

A row of columns is bound to a chain of joints. Each column starts at a
different point in the same clip, so the row moves like a wave. One clip
sways and loops; the other curls forward once and stops. Choosing a clip
crossfades every column into it. Set the crossfade time to zero to see the
hard cut that a crossfade avoids.

The gold marker is a separate actor moved to the top joint of the middle
column every frame, after the world updates, the way you would attach a sword
to a hand.

All seven columns come from one model and share its skeleton and clips.
Shadows are cast from the animated pose.

In front on the left, two columns share one skeleton through two skins that
list the joints in opposite orders. They only bend together correctly if each
mesh uses its own skin.

On the right are two flat panels with morph targets. The left one is driven
by a clip; the right one is set by the slider, from inverted through flat to
exaggerated.

```bash
build/bin/15-animation --still
```

`--still` keeps the camera fixed in front instead of circling.

| Key | Action |
| --- | --- |
| 1 | Sway, which loops |
| 2 | Curl, which plays once and holds |
| 3 | Stop, fading out |
| Crossfade slider | The crossfade time, from a cut to one second |
| Wave slider | The right panel's morph weight |
| Escape | Quit |

## 16-editor

An editor layout built from opane's parts: a menu bar with shortcuts, a dock
space holding a scene view, a hierarchy, an inspector, a console, notes, and
an asset list, and a status bar.

Drag a tab onto a group's guides to add it as a tab or split beside it, onto
the edge guides to dock it along a side, or into empty space to make it a
floating window. Drag a floating window by its title onto a guide to dock it
again. The layout is saved when the editor closes and restored when it opens.

The scene is a ludifex world in a docked panel. It keeps running wherever the
panel is moved, including into a floating window.

```bash
build/bin/16-editor --scale 1.5
```

| | |
| --- | --- |
| Drag a tab | Rearrange, split, or float |
| Right-click the hierarchy | Open a context menu |
| Ctrl+S / Ctrl+L | Save and load the layout |
| Ctrl+Q | Quit, with a confirmation dialog |
| F1 | About |

## 17-materials

Normal maps, metallic and roughness maps, emission, and occlusion.

From left to right: a box and a sphere with a normal map of raised bumps, a
box with the normal map turned off for comparison, a glowing box under a roof
(emission is not affected by shadow), and a sphere loaded from glTF that uses
all five glTF maps: base colour, metallic-roughness stripes, a normal map,
occlusion, and an emissive band made brighter than white with
`KHR_materials_emissive_strength`. The ground uses the bump map too.

The bumps should look raised, lit on top and dark underneath. If the normal
map were read with its green channel flipped, they would look like dents.

| Key | Action |
| --- | --- |
| Space | Stop or restart the camera |
| Escape | Quit |

## 18-graphics

The quality presets, ray tracing, and path tracing, on a scene with something
for each feature to show: a procedural sky, five metal spheres from mirror to
matte, a row of columns forty metres long whose shadows cross the shadow
cascades, a corner with a crate for ambient occlusion, glowing bars for bloom,
a glass pane, and a swaying skinned column. The panel on the right chooses a
preset, turns each effect on and off, and shows the frame time, the render
resolution, whether rays were traced, and the path tracer's sample count.

`--tour` steps through every preset and tracing mode for a few seconds each,
prints the frame time of each, and quits, so you can compare them on your own
machine. `--quality N`, `--shadows`, `--occlusion`, `--reflections`, and
`--path` start the program in that state.

| Key | Action |
| --- | --- |
| 1-6 | Preset: Potato, Low, Medium, High, Ultra, Extreme |
| T / O / R | Traced shadows, occlusion, reflections |
| P | Path tracing |
| K | Sky |
| F | Filmic tone curve |
| E | Automatic exposure |
| B | Bloom |
| Space | Orbit the camera |
| Escape | Quit |

## package-check

Uses both libraries the way someone who downloads a release would: installed,
and found with `find_package`.

```bash
cmake -P package-check/release.cmake
```

The script configures each library separately with freshly downloaded
dependencies, builds it in Release and Debug, installs both into one folder,
and creates the release zips. It then builds the program in `package-check`
against the installed packages in both configurations and runs it. The
program finds both libraries with `find_package`, compiles one material for
each at build time with `opane_add_material` and `ludifex_add_material`,
compiles two more at runtime, runs a world inside the interface, and fails if
anything logs an error.

The zips, the install folder, and the build folders are written to
`build/release`. If a step fails, the script stops and shows its output. The
libraries are cloned from GitHub at v0.0.1; pass `-DOPANE_DIR=<path>` and
`-DLUDIFEX_DIR=<path>` before `-P` to use local copies instead.

## License

MIT, copyright (c) 2026 Cresmar Mat-an. See [LICENSE](LICENSE).
