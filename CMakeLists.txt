cmake_minimum_required(VERSION 3.22)

project(opane_ludifex_examples
    DESCRIPTION "Example programs that use opane and ludifex together"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The libraries, downloaded from GitHub at the release these examples are
# written for, along with their dependencies. To build against your own copy
# instead, configure with
#   -DFETCHCONTENT_SOURCE_DIR_OPANE=path/to/opane
#   -DFETCHCONTENT_SOURCE_DIR_LUDIFEX=path/to/ludifex
include(FetchContent)
FetchContent_Declare(opane
    GIT_REPOSITORY https://github.com/cresmarmat-an/opane.git
    GIT_TAG v0.0.1
    GIT_SHALLOW TRUE)
FetchContent_Declare(ludifex
    GIT_REPOSITORY https://github.com/cresmarmat-an/ludifex.git
    GIT_TAG v0.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(opane ludifex)

# One icon for every example that opens an opane window, drawn while they
# build rather than kept as an image in the repository: see common/MakeIcon.cpp.
add_executable(make_icon common/MakeIcon.cpp)
set(ExampleIcon ${CMAKE_BINARY_DIR}/example-icon.bmp)
add_custom_command(
    OUTPUT ${ExampleIcon}
    COMMAND make_icon ${ExampleIcon}
    DEPENDS make_icon
    COMMENT "Drawing the examples' icon"
    VERBATIM)

function(add_example name)
    # Every example can report where it crashed; see common/CrashReport.h.
    add_executable(${name} ${name}/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/common/CrashReport.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN})

    set(Libraries ${ARGN})
    if("opane::opane" IN_LIST Libraries)
        opane_set_app_icon(${name} ${ExampleIcon})
    endif()

    # Lets an example find its own shader files, and lets hot reload watch the
    # real source rather than a copy.
    target_compile_definitions(${name} PRIVATE
        EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${name}")

    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

    # SDL3 builds as a shared library, so its DLL has to sit next to the
    # executable. An example that links no shared library produces an empty
    # list, and copy_if_different with only a destination is an error, so the
    # command degrades to a no-op in that case.
    if(WIN32)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E
                    "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${name}>>,copy_if_different,true>"
                    "$<TARGET_RUNTIME_DLLS:${name}>" "$<TARGET_FILE_DIR:${name}>"
            COMMAND_EXPAND_LISTS
            VERBATIM)
    endif()
endfunction()

# Uses both libraries together.
add_example(01-falling-box opane::opane ludifex::ludifex)

# A 2D world, rendered through the same pipeline as a 3D one.
add_example(02-world-2d opane::opane ludifex::ludifex)

# A world inside an interface, next to ordinary widgets.
add_example(03-viewport opane::opane ludifex::ludifex)

# Loading a glTF model, generated at runtime so no binary assets are needed.
add_example(04-model opane::opane ludifex::ludifex)

# Custom shaders on world actors, with hot reload.
add_example(05-world-shaders opane::opane ludifex::ludifex)

# Joints: hinges, a slider, distance chains, and a weld.
add_example(06-joints opane::opane ludifex::ludifex)

# The renderer: shadows, textures, point lights, translucency, per-actor
# uniforms, post-processing, fog, and every anti-aliasing preset.
add_example(07-rendering opane::opane ludifex::ludifex)

# Sprites: a background, an animated sprite sheet, sensor coins, and layers.
add_example(08-sprites opane::opane ludifex::ludifex)

# StartApp, then world.Run(): the world finds opane's window by itself.
add_example(09-run-hosted opane::opane ludifex::ludifex)

# World audio: a listener among looping emitters, the voice limit handing the
# voices to the nearest, walls muffling what is behind them, and Doppler.
add_example(10-audio-field opane::opane ludifex::ludifex)

# A character that walks: shape casts, a capsule character controller, steps,
# and slopes, with gravity handled by the program.
add_example(11-character opane::opane ludifex::ludifex)

# Scale: ten thousand objects, a thousand actors sharing one shader, and 256
# point lights.
add_example(12-scale opane::opane ludifex::ludifex)

# Loading models in the foreground and in the background.
add_example(13-loading opane::opane ludifex::ludifex)

# Eight short programs, one function each, following the documentation.
add_example(14-eight-programs opane::opane ludifex::ludifex)

# Skeletal animation: a skinned model, two clips, and crossfading between them,
# plus two skins over one skeleton and morph targets.
add_example(15-animation opane::opane ludifex::ludifex)

# An editor layout: a menu bar and a dock space of panels that can be
# rearranged, floated, and docked again, with a world in one of them and the
# layout saved between runs.
add_example(16-editor opane::opane ludifex::ludifex)

# Surfaces: normal maps, metallic and roughness maps, emission, and occlusion,
# on primitives and on a glTF material that uses all of them.
add_example(17-materials opane::opane ludifex::ludifex)

# Graphics quality presets from Potato to Extreme, the sky, ambient occlusion,
# bloom, exposure, and grading, and ray-traced shadows, occlusion, and
# reflections and a path tracer, in compute shaders on any backend.
add_example(18-graphics opane::opane ludifex::ludifex)
