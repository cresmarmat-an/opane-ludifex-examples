# Builds a release of opane and ludifex and checks that it works.
#
#   cmake -P package-check/release.cmake
#
# Each library is configured on its own as the top-level project, with its
# dependencies downloaded fresh, built in Release and Debug, installed into one
# prefix, and packaged as a zip. Then the program in this folder, which only
# knows the prefix, is built against the installed packages in both
# configurations and run. If any step fails, the script stops.
#
# Optional, with -D before -P:
#   OPANE_DIR=<dir>    a copy of opane to release; the default clones v0.0.1
#   LUDIFEX_DIR=<dir>  a copy of ludifex to release; the default clones v0.0.1
#   OUTPUT=<dir>       where the build folders, the prefix, and the zips go;
#                      the default is build/release next to the examples
#   GENERATOR=<name>   the CMake generator; the default is Visual Studio 18 2026
#   EXTRA_ARGS=<args>  more arguments for configuring the libraries, separated
#                      by semicolons, for example a FETCHCONTENT_SOURCE_DIR_*
#                      to reuse a download

cmake_minimum_required(VERSION 3.22)

get_filename_component(Here "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

if(NOT OUTPUT)
    set(OUTPUT "${Here}/../build/release")
endif()
get_filename_component(OUTPUT "${OUTPUT}" ABSOLUTE)
if(NOT GENERATOR)
    set(GENERATOR "Visual Studio 18 2026")
endif()

set(Prefix "${OUTPUT}/prefix")
file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

function(run description)
    message(STATUS "${description}")
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message("${output}")
        message(FATAL_ERROR "Failed: ${description}")
    endif()
endfunction()

# The sources: a copy named with -D, or the release tag cloned from GitHub.
find_package(Git QUIET)
foreach(Library opane ludifex)
    string(TOUPPER "${Library}_DIR" Variable)
    if(${Variable})
        get_filename_component(${Library}Source "${${Variable}}" ABSOLUTE)
    else()
        if(NOT GIT_EXECUTABLE)
            message(FATAL_ERROR "Git is needed to clone ${Library}; or name a copy with -D${Variable}=<dir>")
        endif()
        set(${Library}Source "${OUTPUT}/sources/${Library}")
        run("Cloning ${Library} v0.0.1"
            "${GIT_EXECUTABLE}" clone --depth 1 --branch v0.0.1
            https://github.com/cresmarmat-an/${Library}.git "${${Library}Source}")
    endif()
endforeach()

foreach(Library opane ludifex)
    set(Build "${OUTPUT}/${Library}")
    run("Configuring ${Library}"
        ${CMAKE_COMMAND} -S "${${Library}Source}" -B "${Build}" -G "${GENERATOR}" -A x64 ${EXTRA_ARGS})
    foreach(Configuration Release Debug)
        run("Building ${Library} (${Configuration})"
            ${CMAKE_COMMAND} --build "${Build}" --config ${Configuration} --parallel)
    endforeach()
    # Release last, so the one copy of SDL3.dll the prefix holds is the
    # release one.
    foreach(Configuration Debug Release)
        run("Installing ${Library} (${Configuration})"
            ${CMAKE_COMMAND} --install "${Build}" --config ${Configuration} --prefix "${Prefix}")
    endforeach()
    execute_process(COMMAND "${CMAKE_CPACK_COMMAND}" -G ZIP -C "Debug;Release" WORKING_DIRECTORY "${Build}"
                    RESULT_VARIABLE packed OUTPUT_QUIET ERROR_QUIET)
    if(NOT packed EQUAL 0)
        message(FATAL_ERROR "Failed: packaging ${Library}")
    endif()
    file(GLOB Archives "${Build}/*.zip")
    file(COPY ${Archives} DESTINATION "${OUTPUT}")
    message(STATUS "Packaged ${Library}")
endforeach()

set(Check "${OUTPUT}/package-check")
run("Configuring the package check against the installed packages"
    ${CMAKE_COMMAND} -S "${Here}" -B "${Check}" -G "${GENERATOR}" -A x64 "-DCMAKE_PREFIX_PATH=${Prefix}")
foreach(Configuration Release Debug)
    run("Building the package check (${Configuration})"
        ${CMAKE_COMMAND} --build "${Check}" --config ${Configuration})
    execute_process(COMMAND "${Check}/${Configuration}/consumer.exe"
                    WORKING_DIRECTORY "${Check}/${Configuration}"
                    RESULT_VARIABLE checked OUTPUT_VARIABLE output ERROR_VARIABLE output)
    if(NOT checked EQUAL 0)
        message("${output}")
        message(FATAL_ERROR "Failed: the package check (${Configuration})")
    endif()
    message(STATUS "The package check passed (${Configuration})")
endforeach()

file(GLOB Archives "${OUTPUT}/*.zip")
message(STATUS "Released:")
foreach(Archive IN LISTS Archives)
    message(STATUS "  ${Archive}")
endforeach()
