# The package check: a program that uses the installed packages the way
# someone who downloaded the release would (find_package, linking, and
# compiling materials at build time), and fails if any of that is broken.
# release.cmake builds it against a fresh install; see that file.

cmake_minimum_required(VERSION 3.22)
project(consumer LANGUAGES C CXX)

find_package(opane 0.0.1 REQUIRED)
find_package(ludifex 0.0.1 REQUIRED)

add_executable(consumer main.cpp)
target_compile_features(consumer PRIVATE cxx_std_20)
target_link_libraries(consumer PRIVATE opane::opane ludifex::ludifex)
target_compile_definitions(consumer PRIVATE CONSUMER_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

opane_add_material(consumer panel.hlsl SYMBOL PanelShader)
ludifex_add_material(consumer toon.hlsl SYMBOL ToonShader)

# The program's icon, drawn during the build by the examples' icon generator,
# so no image is kept in the repository.
add_executable(make_icon ../common/MakeIcon.cpp)
target_compile_features(make_icon PRIVATE cxx_std_20)
set(ConsumerIcon ${CMAKE_CURRENT_BINARY_DIR}/icon.bmp)
add_custom_command(
    OUTPUT ${ConsumerIcon}
    COMMAND make_icon ${ConsumerIcon}
    DEPENDS make_icon
    VERBATIM)
opane_set_app_icon(consumer ${ConsumerIcon})
target_compile_definitions(consumer PRIVATE CONSUMER_ICON="${ConsumerIcon}")

add_custom_command(TARGET consumer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:consumer>" "$<TARGET_FILE_DIR:consumer>"
    COMMAND_EXPAND_LISTS)
