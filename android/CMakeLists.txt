# =============================================================================
# Quetoo — Android NDK / cross-platform CMake build
# =============================================================================
#
# An ALTERNATIVE to the autotools build (configure.ac + Makefile.am), primarily
# for the Android NDK port (jdolan/quetoo#856) and the iOS port (#855). The
# autotools build is left completely untouched: nothing under src/, configure.ac,
# Makefile.am, or android/qglib/ is modified by this file. All paths below are
# relative to the repo root via QUETOO_ROOT (this file lives in android/, so the
# root is one directory up), referencing ../src etc.
#
# WHAT THIS BUILDS
#   Android (ANDROID is set by the NDK toolchain, driven by android/app/build.gradle):
#     - libmain.so   : the engine client, packaged as the SDL3 "main" library.
#                      SDLActivity loads libSDL3.so then libmain.so and calls
#                      SDL_main (the engine's main(), renamed by <SDL3/SDL_main.h>).
#                      Statically links the engine component archives below.
#     - libgame.so   : loadable game module    (dlopen'd at runtime)
#     - libcgame.so  : loadable client-game module (dlopen'd at runtime)
#   Desktop (sanity build, used to validate the qglib migration before any
#   Android tooling exists — PORTING.md §5 item 2):
#     - quetoo            : GUI client executable      (src/main/main.c + libclient)
#     - quetoo-dedicated  : headless server executable (src/main/main.c + libclient_null)
#     - quetoo-master     : master server tool
#     - quemap            : BSP/lighting compiler
#     (plus the same game.so / cgame.so modules)
#
# COMPONENT MODEL
#   The autotools build factors the engine into many libtool convenience
#   archives (libshared, libcommon, libcollision, libnet, librenderer, libsound,
#   libui, libclient[/_null], libserver) plus two loadable modules and several
#   bin_PROGRAMS. We mirror that with OBJECT/STATIC libraries so link
#   dependencies stay faithful, then assemble the final artifacts.
#
# GLIB REPLACEMENT (qglib)
#   The engine does `#include <glib.h>` (src/quetoo.h). We cannot edit src/, so:
#     - android/cmake/compat/ contains a glib.h that forwards to qglib.h, and is
#       placed FIRST on the include path so <glib.h> resolves to it.
#     - android/qglib/ is added to the include path so qglib.h + its sub-headers
#       resolve, and its qglib_*.c are compiled into every engine piece.
#   See android/cmake/compat/glib.h and PORTING.md §3.
# =============================================================================

cmake_minimum_required(VERSION 3.22)   # matches the NDK-bundled cmake (build.gradle: 3.22.1)

project(Quetoo
    VERSION 0.1.0
    LANGUAGES C CXX
)

# Repo root, relative to this file (android/CMakeLists.txt -> ..).
get_filename_component(QUETOO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# --- Language standards (engine is C11; discord-rpc dep is C++11) -------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The autotools build enables system extensions (AC_USE_SYSTEM_EXTENSIONS), which
# defines _GNU_SOURCE so POSIX/GNU decls (unlink, etc.) are visible under -std=gnu11.
# Match that here, or many engine TUs hit implicit-declaration errors.
add_compile_definitions(_GNU_SOURCE)

# Position-independent code everywhere: required for shared modules and for
# Android in general. Mirrors autotools @SO_CFLAGS@ = -fPIC -DPIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -----------------------------------------------------------------------------
# Build options
# -----------------------------------------------------------------------------
# GL ES 3.0 dual code path (PORTING.md §4). Defaulted ON for Android (the
# build.gradle passes -DQUETOO_GLES=ON), OFF for desktop GL.
option(QUETOO_GLES "Build the renderer against OpenGL ES 3.0 (Android/iOS)" ${ANDROID})

# Desktop-only executables. Off under Android (we only want libmain.so + modules
# there); on elsewhere for the sanity build. (option() defaults must be a plain
# bool — not a generator expression — so compute it first.)
if(ANDROID)
    set(_quetoo_tools_default OFF)
else()
    set(_quetoo_tools_default ON)
endif()
option(QUETOO_BUILD_TOOLS
    "Build desktop executables (quetoo-dedicated, quetoo-master, quemap)"
    ${_quetoo_tools_default})
# Under Android these executables make no sense; force them off regardless.
if(ANDROID)
    set(QUETOO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
endif()

# -----------------------------------------------------------------------------
# Generated config.h (replaces autotools AC_CONFIG_HEADERS)
# -----------------------------------------------------------------------------
# configure.ac defaults VERSION to -1 (installer disabled). Keep that.
set(QUETOO_VERSION "-1" CACHE STRING "Quetoo game version (-1 disables the installer)")

if(ANDROID)
    set(QUETOO_BUILD "${ANDROID_ABI}-android")
else()
    set(QUETOO_BUILD "${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}")
endif()

# Runtime/install paths. On Android these are placeholders (data comes from the
# APK / SDL_GetPrefPath at runtime). Overridable for desktop packaging.
set(QUETOO_BINDIR     "/bin"               CACHE STRING "BINDIR define for config.h")
set(QUETOO_PKGLIBDIR  "lib/quetoo"         CACHE STRING "PKGLIBDIR define (where game.so/cgame.so live)")
set(QUETOO_PKGDATADIR "share/quetoo"       CACHE STRING "PKGDATADIR define (game data root)")

# execinfo.h: present on glibc, absent on Bionic. The NDK ships an execinfo.h
# header but backtrace() is only implemented at API 33+; we target API 24, and
# the symbols aren't linkable there, so force it off on Android.
include(CheckIncludeFile)
if(ANDROID)
  set(HAVE_EXECINFO OFF)
else()
  check_include_file("execinfo.h" HAVE_EXECINFO)
endif()

set(QUETOO_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in"
    "${QUETOO_GENERATED_DIR}/config.h"
    @ONLY
)

# -----------------------------------------------------------------------------
# Source lists + dependencies (split into helper modules for readability)
# -----------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/QuetooSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/QuetooDependencies.cmake")

# -----------------------------------------------------------------------------
# Common include paths + compile definitions, applied to every engine piece via
# an INTERFACE target. This is the single place the qglib shim is wired in.
# -----------------------------------------------------------------------------
add_library(quetoo_common_iface INTERFACE)

target_include_directories(quetoo_common_iface INTERFACE
    "${QUETOO_GENERATED_DIR}"                    # config.h  (must come before src)
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat"   # our <glib.h> shim -> qglib.h  (MUST be early)
    "${QGLIB_DIR}"                               # qglib.h and its sub-headers
    "${QUETOO_ROOT}"                             # -I$(top_srcdir): root-relative "src/..." includes (ui_types.h, QuetooRenderer.c)
    "${QUETOO_SRC}"                              # -I$(top_srcdir)/src (every Makefile.am has it)
)

# Route qglib's allocator through SDL so engine and shim share one heap on
# Android (qglib.h honors QG_MALLOC/REALLOC/CALLOC/FREE if predefined).
# Commented by default: enabling requires SDL headers on qglib's TUs, which is
# fine since SDL3 is always linked. Flip on if heap unification matters.
# target_compile_definitions(quetoo_common_iface INTERFACE
#     QG_MALLOC=SDL_malloc QG_REALLOC=SDL_realloc QG_CALLOC=SDL_calloc QG_FREE=SDL_free)

if(QUETOO_GLES)
    # The renderer's GL ES 3.0 path is gated on this (see src/client/renderer/r_gl.*).
    target_compile_definitions(quetoo_common_iface INTERFACE QUETOO_GLES=1)
endif()

# Warning flags mirror configure.ac's WARN_CFLAGS (without -Werror).
if(NOT MSVC)
    target_compile_options(quetoo_common_iface INTERFACE
        -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas)
endif()

# Compile the qglib shim once as an OBJECT lib and fold it into each component
# that needs containers (engine + both modules). Built with the same include
# iface so its own headers resolve.
add_library(qglib OBJECT ${QGLIB_SOURCES})
target_include_directories(qglib PUBLIC "${QGLIB_DIR}")
# qglib is self-contained C; no link deps. (If QG_MALLOC=SDL_* is enabled above,
# add SDL3 headers here: target_link_libraries(qglib PRIVATE ${QUETOO_SDL3_TARGETS}).)

# =============================================================================
# Engine component libraries (the libtool convenience archives, as OBJECT libs)
# =============================================================================
# Using OBJECT libraries keeps these from emitting standalone .so/.a files while
# preserving the dependency graph; the final artifacts gather the objects they
# need. Each gets the common iface (config.h + glib shim + src include + warns).

# -- libshared --
add_library(quetoo_shared OBJECT ${QUETOO_SHARED_SOURCES})
target_link_libraries(quetoo_shared PRIVATE quetoo_common_iface)
target_link_libraries(quetoo_shared PRIVATE ${QUETOO_SDL3_TARGETS})

# -- libcommon -- (depends on shared; uses PhysicsFS + SDL3)
add_library(quetoo_common OBJECT ${QUETOO_COMMON_SOURCES})
target_link_libraries(quetoo_common PRIVATE quetoo_common_iface)
target_link_libraries(quetoo_common PRIVATE ${QUETOO_SDL3_TARGETS} ${QUETOO_PHYSFS_TARGET} ZLIB::ZLIB)

# -- libcollision -- (depends on common)
add_library(quetoo_collision OBJECT ${QUETOO_COLLISION_SOURCES})
target_link_libraries(quetoo_collision PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS})

# -- libnet -- (depends on common; uses Objectively for net_http.c)
add_library(quetoo_net OBJECT ${QUETOO_NET_SOURCES})
target_link_libraries(quetoo_net PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS})
if(QUETOO_OBJECTIVELY_TARGET)
    target_link_libraries(quetoo_net PRIVATE ${QUETOO_OBJECTIVELY_TARGET})
endif()

# -- libserver -- (depends on collision + net; curses on desktop)
add_library(quetoo_server OBJECT ${QUETOO_SERVER_SOURCES})
target_link_libraries(quetoo_server PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS})
if(QUETOO_OBJECTIVELY_TARGET)
    target_link_libraries(quetoo_server PRIVATE ${QUETOO_OBJECTIVELY_TARGET})
endif()
if(QUETOO_CURSES_TARGET)
    target_link_libraries(quetoo_server PRIVATE ${QUETOO_CURSES_TARGET})
endif()

# -- librenderer -- (depends on common; GL/GLES)
add_library(quetoo_renderer OBJECT ${QUETOO_RENDERER_SOURCES})
target_link_libraries(quetoo_renderer PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS} ${QUETOO_GL_TARGET})
target_include_directories(quetoo_renderer PRIVATE "${QUETOO_SRC}/client")  # -I src/client (per Makefile.am)

# -- libsound -- (depends on common; OpenAL + sndfile)
add_library(quetoo_sound OBJECT ${QUETOO_SOUND_SOURCES})
target_link_libraries(quetoo_sound PRIVATE quetoo_common_iface
    ${QUETOO_SDL3_TARGETS} ${QUETOO_OPENAL_TARGET} ${QUETOO_SNDFILE_TARGET})

# -- libui -- (depends on common; ObjectivelyMVC). Needs src/client + renderer includes.
add_library(quetoo_ui OBJECT ${QUETOO_UI_SOURCES})
target_link_libraries(quetoo_ui PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS})
target_include_directories(quetoo_ui PRIVATE
    "${QUETOO_SRC}/client" "${QUETOO_SRC}/client/renderer")
if(QUETOO_OBJECTIVELYMVC_TARGET)
    target_link_libraries(quetoo_ui PRIVATE ${QUETOO_OBJECTIVELYMVC_TARGET})
endif()

# -- libclient (full GUI client) -- aggregates renderer/sound/ui --
add_library(quetoo_client OBJECT ${QUETOO_CLIENT_SOURCES})
target_link_libraries(quetoo_client PRIVATE quetoo_common_iface
    ${QUETOO_SDL3_TARGETS} ${QUETOO_OPENAL_TARGET} ${QUETOO_GL_TARGET} ${QUETOO_SNDFILE_TARGET})

# -- libclient_null (headless client used by the dedicated server) --
add_library(quetoo_client_null OBJECT ${QUETOO_CLIENT_NULL_SOURCES})
target_link_libraries(quetoo_client_null PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS})

# -- game/default convenience libs (shared between game.so and cgame.so) --
add_library(quetoo_item OBJECT ${QUETOO_GAME_ITEM_SOURCES})
target_link_libraries(quetoo_item PRIVATE quetoo_common_iface)
add_library(quetoo_pmove OBJECT ${QUETOO_GAME_PMOVE_SOURCES})
target_link_libraries(quetoo_pmove PRIVATE quetoo_common_iface)

# -- deps: minizip (quemap) and discord-rpc (cgame) --
add_library(quetoo_minizip OBJECT ${QUETOO_MINIZIP_SOURCES})
target_link_libraries(quetoo_minizip PRIVATE quetoo_common_iface)

add_library(quetoo_discord_rpc OBJECT
    ${QUETOO_DISCORD_RPC_SOURCES} ${QUETOO_DISCORD_RPC_SOURCES_UNIX})
target_include_directories(quetoo_discord_rpc PRIVATE
    "${QUETOO_ROOT}/deps/discord-rpc/include"
    "${QUETOO_ROOT}/deps/rapidjson")
# discord-rpc is C++; it does NOT include quetoo.h/glib, so it does not need the
# common iface (and must not get the warning flags meant for the C engine).

# =============================================================================
# Loadable modules: game.so and cgame.so  (dlopen'd — Android supports it)
# =============================================================================
# Autotools builds these with -module -avoid-version -no-undefined. We emit
# them as plain SHARED libs named "game"/"cgame" -> libgame.so / libcgame.so.
# OUTPUT_NAME has no "lib" prefix concern on Android (dlopen uses the full
# soname); the engine resolves them via PKGLIBDIR/default/.

# -- game.so --
add_library(game SHARED
    ${QUETOO_GAME_SOURCES}
    $<TARGET_OBJECTS:qglib>
    $<TARGET_OBJECTS:quetoo_item>
    $<TARGET_OBJECTS:quetoo_pmove>
    $<TARGET_OBJECTS:quetoo_shared>      # bg_pmove pulls libshared in autotools
)
target_link_libraries(game PRIVATE quetoo_common_iface)
# game.so is self-contained logic; it resolves engine symbols at load time.
set_target_properties(game PROPERTIES PREFIX "" OUTPUT_NAME "game")

# -- cgame.so -- (the big one: cgame sources + all 9 ui groups + item/pmove + discord)
add_library(cgame SHARED
    ${QUETOO_CGAME_SOURCES}
    ${QUETOO_CGAME_UI_SOURCES}
    $<TARGET_OBJECTS:qglib>
    $<TARGET_OBJECTS:quetoo_item>
    $<TARGET_OBJECTS:quetoo_pmove>
    $<TARGET_OBJECTS:quetoo_discord_rpc>
    $<TARGET_OBJECTS:quetoo_shared>      # BigLong/LittleLong/va/vtos (libshared); cgame is a standalone .so
)
target_link_libraries(cgame PRIVATE quetoo_common_iface ${QUETOO_SDL3_TARGETS} ${QUETOO_OPENAL_TARGET} ${QUETOO_GL_TARGET})
if(QUETOO_OBJECTIVELYMVC_TARGET)
    target_link_libraries(cgame PRIVATE ${QUETOO_OBJECTIVELYMVC_TARGET})
endif()
# C++ runtime: discord-rpc needs libstdc++ (autotools links -lstdc++ on Linux).
target_link_libraries(cgame PRIVATE $<$<NOT:$<PLATFORM_ID:Darwin>>:stdc++>)
# cgame include paths: it includes its own ui/* dirs (mirrors the per-dir -I lists)
target_include_directories(cgame PRIVATE
    "${QUETOO_SRC}/cgame"
    "${QUETOO_SRC}/cgame/default"
    "${QUETOO_SRC}/cgame/default/ui"
    "${QUETOO_SRC}/cgame/default/ui/common"
    "${QUETOO_SRC}/cgame/default/ui/controls"
    "${QUETOO_SRC}/cgame/default/ui/credits"
    "${QUETOO_SRC}/cgame/default/ui/home"
    "${QUETOO_SRC}/cgame/default/ui/play"
    "${QUETOO_SRC}/cgame/default/ui/settings"
    "${QUETOO_SRC}/cgame/default/ui/teams")
set_target_properties(cgame PROPERTIES PREFIX "" OUTPUT_NAME "cgame")

# GLES has no fixed-function pipeline; link no-op stubs for the legacy GL entry
# points ObjectivelyMVC's (unused) base Renderer references. See gles_compat.c.
if(QUETOO_GLES)
    target_sources(cgame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat/gles_compat.c")
endif()

# =============================================================================
# Final client artifact
# =============================================================================
# The set of engine objects that make up the full GUI client (everything except
# the loadable modules, which are dlopen'd separately).
set(QUETOO_CLIENT_OBJECTS
    $<TARGET_OBJECTS:qglib>
    $<TARGET_OBJECTS:quetoo_shared>
    $<TARGET_OBJECTS:quetoo_common>
    $<TARGET_OBJECTS:quetoo_collision>
    $<TARGET_OBJECTS:quetoo_net>
    $<TARGET_OBJECTS:quetoo_server>
    $<TARGET_OBJECTS:quetoo_renderer>
    $<TARGET_OBJECTS:quetoo_sound>
    $<TARGET_OBJECTS:quetoo_ui>
    $<TARGET_OBJECTS:quetoo_client>
)

# Libraries the assembled client links against (union of the component deps).
set(QUETOO_CLIENT_LINK
    quetoo_common_iface
    ${QUETOO_SDL3_TARGETS}
    ${QUETOO_OPENAL_TARGET}
    ${QUETOO_GL_TARGET}
    ${QUETOO_SNDFILE_TARGET}
    ${QUETOO_PHYSFS_TARGET}
    ZLIB::ZLIB
)
if(QUETOO_OBJECTIVELY_TARGET)
    list(APPEND QUETOO_CLIENT_LINK ${QUETOO_OBJECTIVELY_TARGET})
endif()
if(QUETOO_OBJECTIVELYMVC_TARGET)
    list(APPEND QUETOO_CLIENT_LINK ${QUETOO_OBJECTIVELYMVC_TARGET})
endif()

if(ANDROID)
    # ---- Android: the client is the SDL "main" shared library ----------------
    # build.gradle / SDLActivity contract: a library named "main" -> libmain.so,
    # entry point main() (SDL renames it to SDL_main via <SDL3/SDL_main.h>).
    add_library(main SHARED
        ${QUETOO_MAIN_SOURCES}
        ${QUETOO_CLIENT_OBJECTS}
    )
    target_link_libraries(main PRIVATE ${QUETOO_CLIENT_LINK})
    # SDL3main provides the SDL_main shim that SDLActivity calls into. SDL3's
    # CMake exposes it as SDL3::SDL3main (header-only-ish on Android).
    if(TARGET SDL3::SDL3main)
        target_link_libraries(main PRIVATE SDL3::SDL3main)
    endif()
    set_target_properties(main PROPERTIES PREFIX "" OUTPUT_NAME "main")
    # Android system libs commonly needed by SDL apps; SDL usually pulls these
    # transitively, listed defensively.
    target_link_libraries(main PRIVATE android log dl)
    if(QUETOO_GLES)
        target_sources(main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat/gles_compat.c")
    endif()

else()
    # ---- Desktop sanity build: real executables ------------------------------
    # `quetoo` (GUI) and `quetoo-dedicated` (headless) share main.c and differ
    # only by linking libclient vs libclient_null (per src/main/Makefile.am).

    add_executable(quetoo ${QUETOO_MAIN_SOURCES} ${QUETOO_CLIENT_OBJECTS})
    target_link_libraries(quetoo PRIVATE ${QUETOO_CLIENT_LINK})

    if(QUETOO_BUILD_TOOLS)
        # quetoo-dedicated: server + libclient_null (no renderer/sound/ui).
        add_executable(quetoo-dedicated
            ${QUETOO_MAIN_SOURCES}
            $<TARGET_OBJECTS:qglib>
            $<TARGET_OBJECTS:quetoo_shared>
            $<TARGET_OBJECTS:quetoo_common>
            $<TARGET_OBJECTS:quetoo_collision>
            $<TARGET_OBJECTS:quetoo_net>
            $<TARGET_OBJECTS:quetoo_server>
            $<TARGET_OBJECTS:quetoo_client_null>
        )
        target_link_libraries(quetoo-dedicated PRIVATE
            quetoo_common_iface ${QUETOO_SDL3_TARGETS} ${QUETOO_PHYSFS_TARGET} ZLIB::ZLIB)
        if(QUETOO_OBJECTIVELY_TARGET)
            target_link_libraries(quetoo-dedicated PRIVATE ${QUETOO_OBJECTIVELY_TARGET})
        endif()
        if(QUETOO_CURSES_TARGET)
            target_link_libraries(quetoo-dedicated PRIVATE ${QUETOO_CURSES_TARGET})
        endif()

        # quetoo-master: net + common.
        add_executable(quetoo-master
            ${QUETOO_MASTER_SOURCES}
            $<TARGET_OBJECTS:qglib>
            $<TARGET_OBJECTS:quetoo_shared>
            $<TARGET_OBJECTS:quetoo_common>
            $<TARGET_OBJECTS:quetoo_net>
        )
        target_link_libraries(quetoo-master PRIVATE
            quetoo_common_iface ${QUETOO_SDL3_TARGETS} ${QUETOO_PHYSFS_TARGET} ZLIB::ZLIB)
        if(QUETOO_OBJECTIVELY_TARGET)
            target_link_libraries(quetoo-master PRIVATE ${QUETOO_OBJECTIVELY_TARGET})
        endif()

        # quemap: collision + net + minizip.
        add_executable(quemap
            ${QUETOO_QUEMAP_SOURCES}
            $<TARGET_OBJECTS:qglib>
            $<TARGET_OBJECTS:quetoo_shared>
            $<TARGET_OBJECTS:quetoo_common>
            $<TARGET_OBJECTS:quetoo_collision>
            $<TARGET_OBJECTS:quetoo_net>
            $<TARGET_OBJECTS:quetoo_minizip>
        )
        target_link_libraries(quemap PRIVATE
            quetoo_common_iface ${QUETOO_SDL3_TARGETS} ${QUETOO_PHYSFS_TARGET} ZLIB::ZLIB)
        target_include_directories(quemap PRIVATE "${QUETOO_ROOT}")  # -I$(top_srcdir) per Makefile.am
        if(QUETOO_OBJECTIVELY_TARGET)
            target_link_libraries(quemap PRIVATE ${QUETOO_OBJECTIVELY_TARGET})
        endif()
        if(QUETOO_CURSES_TARGET)
            target_link_libraries(quemap PRIVATE ${QUETOO_CURSES_TARGET})
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "==================== Quetoo CMake configuration ====================")
message(STATUS "  Repo root .......... ${QUETOO_ROOT}")
message(STATUS "  Build (config.h) ... ${QUETOO_BUILD}")
message(STATUS "  Android ............ ${ANDROID}  (ABI: ${ANDROID_ABI})")
message(STATUS "  GL ES path ......... ${QUETOO_GLES}")
message(STATUS "  Desktop tools ...... ${QUETOO_BUILD_TOOLS}")
message(STATUS "  glib replacement ... android/qglib (via compat/glib.h shim)")
message(STATUS "  SDL3 source dir .... ${QUETOO_SDL3_SOURCE_DIR}")
message(STATUS "====================================================================")
