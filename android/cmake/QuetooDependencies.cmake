# QuetooDependencies.cmake
#
# Locates / builds the external libraries the Quetoo engine links against, and
# exposes them as imported/interface targets so the main CMakeLists can just
# `target_link_libraries(... ${QUETOO_*_LIBS})`.
#
# Dependency inventory (from the Makefile.am @*_CFLAGS@/@*_LIBS@ substitutions):
#   REQUIRED, mapped onto NDK-buildable libs:
#     - SDL3        (sdl3, sdl3-image)  -> platform, window, GL context, audio, input
#     - OpenAL      (openal)            -> 3D audio mixer            [src/client/sound]
#     - PhysicsFS   (-lphysfs -lz)      -> virtual filesystem / pk3  [src/common]
#     - libsndfile  (sndfile)           -> audio decode (ogg/wav)    [src/client/sound]
#     - Objectively (Objectively)       -> OO runtime + HTTP (net_http.c URLSession)
#     - ObjectivelyMVC (ObjectivelyMVC) -> MVC desktop UI            [src/client/ui, cgame ui]
#     - ncurses     (ncurses)           -> dedicated/quemap console  [server, quemap]
#   GL:
#     - Desktop: OpenGL (-lGL).  Android: GLES v3 + EGL (QUETOO_GLES path).
#   NOTE: glib2 is intentionally NOT here — it is replaced by android/qglib
#         (handled directly in the main CMakeLists, see compat/glib.h).
#   NOTE: libcurl is NOT a direct dependency. configure.ac never checks for it,
#         and src/net/net_http.c uses Objectively's URLRequest/URLSession, not
#         curl. (Objectively may use curl internally, but that is transitive and
#         resolved when Objectively itself is built.) The task brief listed curl
#         as an assumed dep; the real source set contradicts that, so we expose
#         an OPTIONAL hook below but do not require it.
#
# Each dependency is wrapped in a cache variable for its location so that an
# Android Studio / CI invocation can point at prebuilt NDK artifacts, while a
# desktop "sanity" configure can fall back to find_package / pkg-config.

include_guard(GLOBAL)

set(QUETOO_DEP_LIBS "")          # accumulates link targets common to all engine pieces
set(QUETOO_DEP_INCLUDE_DIRS "")  # accumulates include dirs (for header-only/manual deps)

# ---------------------------------------------------------------------------
# SDL3 (+ SDL3_image)
# ---------------------------------------------------------------------------
# Two supported acquisition modes; pick via -DQUETOO_SDL3_SOURCE_DIR=... or by
# having SDL3 discoverable through CMAKE_PREFIX_PATH (e.g. a prebuilt AAR).
#
#   (A) SOURCE  : point QUETOO_SDL3_SOURCE_DIR at an SDL checkout; we
#                 add_subdirectory() it. This is the recommended Android path —
#                 SDL3 builds libSDL3.so for the active ANDROID_ABI and provides
#                 the SDL3::SDL3 + SDL3::SDL3main targets and (with vendored
#                 satellite libs) SDL3_image::SDL3_image.
#   (B) PACKAGE : find_package(SDL3 CONFIG). Use when SDL3 is installed/prebuilt
#                 (desktop dev, or an SDL3 AAR that ships CMake config files).
#
# TODO(fill-in): set QUETOO_SDL3_SOURCE_DIR (or CMAKE_PREFIX_PATH) for your
#   environment. A common layout is a sibling checkout: ../../SDL relative to
#   this repo, or a vendored copy under android/app/jni/SDL (SDL's own template
#   layout). There is no SDL inside this repo, so this MUST be provided.
set(QUETOO_SDL3_SOURCE_DIR "" CACHE PATH
    "Path to an SDL3 source tree to add_subdirectory(). Leave empty to use find_package(SDL3).")
set(QUETOO_SDL3_IMAGE_SOURCE_DIR "" CACHE PATH
    "Path to an SDL3_image source tree. Leave empty to use find_package(SDL3_image) or SDL's vendored copy.")

set(QUETOO_SDL3_TARGETS "")
if(QUETOO_SDL3_SOURCE_DIR)
    message(STATUS "Quetoo: using SDL3 from source: ${QUETOO_SDL3_SOURCE_DIR}")
    add_subdirectory("${QUETOO_SDL3_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/SDL3-build")
    list(APPEND QUETOO_SDL3_TARGETS SDL3::SDL3)
    if(QUETOO_SDL3_IMAGE_SOURCE_DIR)
        add_subdirectory("${QUETOO_SDL3_IMAGE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/SDL3_image-build")
        list(APPEND QUETOO_SDL3_TARGETS SDL3_image::SDL3_image)
    endif()
else()
    # PACKAGE mode. REQUIRED — the engine cannot build without SDL3.
    find_package(SDL3 CONFIG REQUIRED)
    list(APPEND QUETOO_SDL3_TARGETS SDL3::SDL3)
    # SDL3_image ships separately; treat as required for the GUI client but
    # tolerate its absence for headless/dedicated-only configures.
    find_package(SDL3_image CONFIG QUIET)
    if(TARGET SDL3_image::SDL3_image)
        list(APPEND QUETOO_SDL3_TARGETS SDL3_image::SDL3_image)
    else()
        message(WARNING "Quetoo: SDL3_image not found; the GUI client needs it "
                        "(src/common/image.c). Dedicated/quemap-only builds are fine.")
    endif()
endif()

# SDL3_ttf ships separately and is required by ObjectivelyMVC for text rendering
# (TTF_*). Cross-built into the Android prefix; resolved via its CMake config
# package. Appended to the SDL3 target list so every client target links it.
find_package(SDL3_ttf CONFIG QUIET)
if(TARGET SDL3_ttf::SDL3_ttf)
    list(APPEND QUETOO_SDL3_TARGETS SDL3_ttf::SDL3_ttf)
else()
    message(WARNING "Quetoo: SDL3_ttf not found; ObjectivelyMVC text rendering "
                    "(TTF_*) will not link.")
endif()

# ---------------------------------------------------------------------------
# OpenAL (OpenAL-soft on Android)
# ---------------------------------------------------------------------------
# OpenAL-soft has an official NDK build and exports an OpenAL::OpenAL target.
# TODO(fill-in): provide via CMAKE_PREFIX_PATH or QUETOO_OPENAL_SOURCE_DIR.
set(QUETOO_OPENAL_SOURCE_DIR "" CACHE PATH
    "Path to an openal-soft source tree to add_subdirectory(). Empty -> find_package(OpenAL).")
if(QUETOO_OPENAL_SOURCE_DIR)
    add_subdirectory("${QUETOO_OPENAL_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/openal-build")
    # openal-soft's target is named OpenAL (alias OpenAL::OpenAL in recent versions).
    set(QUETOO_OPENAL_TARGET OpenAL)
else()
    find_package(OpenAL REQUIRED)   # provides OpenAL::OpenAL (CMake >= 3.10)
    set(QUETOO_OPENAL_TARGET OpenAL::OpenAL)
endif()

# ---------------------------------------------------------------------------
# PhysicsFS (+ zlib)
# ---------------------------------------------------------------------------
# autotools just does -lphysfs -lz. PhysicsFS provides a CMake build that
# exports PhysFS::PhysFS (static) / PhysFS::PhysFS-static depending on version.
# TODO(fill-in): provide via CMAKE_PREFIX_PATH or QUETOO_PHYSFS_SOURCE_DIR.
set(QUETOO_PHYSFS_SOURCE_DIR "" CACHE PATH
    "Path to a PhysicsFS source tree. Empty -> find_package(PhysFS)/manual lib lookup.")
if(QUETOO_PHYSFS_SOURCE_DIR)
    add_subdirectory("${QUETOO_PHYSFS_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/physfs-build")
    # Prefer the static target on Android to keep it inside the .so set.
    if(TARGET PhysFS::PhysFS-static)
        set(QUETOO_PHYSFS_TARGET PhysFS::PhysFS-static)
    else()
        set(QUETOO_PHYSFS_TARGET PhysFS::PhysFS)
    endif()
else()
    find_package(PhysFS QUIET)
    if(TARGET PhysFS::PhysFS)
        set(QUETOO_PHYSFS_TARGET PhysFS::PhysFS)
    else()
        # Fallback: bare library/header lookup (older PhysicsFS w/o CMake config).
        find_library(PHYSFS_LIBRARY NAMES physfs REQUIRED)
        find_path(PHYSFS_INCLUDE_DIR NAMES physfs.h REQUIRED)
        add_library(quetoo_physfs INTERFACE)
        target_link_libraries(quetoo_physfs INTERFACE "${PHYSFS_LIBRARY}")
        target_include_directories(quetoo_physfs INTERFACE "${PHYSFS_INCLUDE_DIR}")
        set(QUETOO_PHYSFS_TARGET quetoo_physfs)
    endif()
endif()
# zlib: required by PhysicsFS (and miniz uses none). The NDK ships libz.
find_package(ZLIB REQUIRED)   # ZLIB::ZLIB

# ---------------------------------------------------------------------------
# libsndfile
# ---------------------------------------------------------------------------
# Has a CMake build exporting SndFile::sndfile.
# TODO(fill-in): provide via CMAKE_PREFIX_PATH or QUETOO_SNDFILE_SOURCE_DIR.
set(QUETOO_SNDFILE_SOURCE_DIR "" CACHE PATH
    "Path to a libsndfile source tree. Empty -> find_package(SndFile).")
if(QUETOO_SNDFILE_SOURCE_DIR)
    add_subdirectory("${QUETOO_SNDFILE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/sndfile-build")
    set(QUETOO_SNDFILE_TARGET SndFile::sndfile)
else()
    find_package(SndFile CONFIG QUIET)
    if(TARGET SndFile::sndfile)
        set(QUETOO_SNDFILE_TARGET SndFile::sndfile)
    else()
        find_library(SNDFILE_LIBRARY NAMES sndfile REQUIRED)
        find_path(SNDFILE_INCLUDE_DIR NAMES sndfile.h REQUIRED)
        add_library(quetoo_sndfile INTERFACE)
        target_link_libraries(quetoo_sndfile INTERFACE "${SNDFILE_LIBRARY}")
        target_include_directories(quetoo_sndfile INTERFACE "${SNDFILE_INCLUDE_DIR}")
        set(QUETOO_SNDFILE_TARGET quetoo_sndfile)
    endif()
endif()

# ---------------------------------------------------------------------------
# Objectively + ObjectivelyMVC  (jdolan's libraries)
# ---------------------------------------------------------------------------
# These are jdolan's own deps. Objectively is needed by libnet (net_http.c) and
# thus by everything that links net (server, quemap, master, client). MVC is the
# desktop UI; needed by libui and the cgame ui sources.
#
# TODO(fill-in): there are no upstream NDK builds of these. For the *desktop
#   sanity* build, find them via pkg-config (matching configure.ac's
#   PKG_CHECK_MODULES). For Android they must first be cross-compiled with this
#   same CMake/NDK toolchain and pointed at via CMAKE_PREFIX_PATH or the
#   QUETOO_OBJECTIVELY*_SOURCE_DIR hooks below. Porting them is out of scope for
#   this build file (tracked separately under #855/#856).
set(QUETOO_OBJECTIVELY_SOURCE_DIR "" CACHE PATH
    "Path to an Objectively source/install. Empty -> pkg-config Objectively.")
set(QUETOO_OBJECTIVELYMVC_SOURCE_DIR "" CACHE PATH
    "Path to an ObjectivelyMVC source/install. Empty -> pkg-config ObjectivelyMVC.")

find_package(PkgConfig QUIET)

# Objectively
if(QUETOO_OBJECTIVELY_SOURCE_DIR)
    add_subdirectory("${QUETOO_OBJECTIVELY_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/Objectively-build")
    set(QUETOO_OBJECTIVELY_TARGET Objectively)   # assumed target name
elseif(PkgConfig_FOUND)
    pkg_check_modules(OBJECTIVELY IMPORTED_TARGET Objectively)
    if(TARGET PkgConfig::OBJECTIVELY)
        set(QUETOO_OBJECTIVELY_TARGET PkgConfig::OBJECTIVELY)
    endif()
endif()
if(NOT DEFINED QUETOO_OBJECTIVELY_TARGET)
    message(WARNING "Quetoo: Objectively not found. net/server/quemap/master/client "
                    "will not link. Set QUETOO_OBJECTIVELY_SOURCE_DIR or install it. "
                    "(qglib/SDL targets that don't use net still build.)")
    set(QUETOO_OBJECTIVELY_TARGET "")
endif()

# ObjectivelyMVC
if(QUETOO_OBJECTIVELYMVC_SOURCE_DIR)
    add_subdirectory("${QUETOO_OBJECTIVELYMVC_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/ObjectivelyMVC-build")
    set(QUETOO_OBJECTIVELYMVC_TARGET ObjectivelyMVC)
elseif(PkgConfig_FOUND)
    pkg_check_modules(OBJECTIVELYMVC IMPORTED_TARGET ObjectivelyMVC)
    if(TARGET PkgConfig::OBJECTIVELYMVC)
        set(QUETOO_OBJECTIVELYMVC_TARGET PkgConfig::OBJECTIVELYMVC)
    endif()
endif()
if(NOT DEFINED QUETOO_OBJECTIVELYMVC_TARGET)
    message(WARNING "Quetoo: ObjectivelyMVC not found. The GUI client (libui) and "
                    "cgame UI will not link. Set QUETOO_OBJECTIVELYMVC_SOURCE_DIR.")
    set(QUETOO_OBJECTIVELYMVC_TARGET "")
endif()

# ---------------------------------------------------------------------------
# curses (dedicated server + quemap console only)
# ---------------------------------------------------------------------------
# Only needed by the desktop tools (server console, quemap). Not needed for the
# Android `main` client library. Optional so Android-only builds don't require it.
set(QUETOO_CURSES_TARGET "")
if(NOT ANDROID)
    if(PkgConfig_FOUND)
        pkg_check_modules(CURSES IMPORTED_TARGET ncurses)
        if(TARGET PkgConfig::CURSES)
            set(QUETOO_CURSES_TARGET PkgConfig::CURSES)
        endif()
    endif()
    if(NOT QUETOO_CURSES_TARGET)
        find_package(Curses QUIET)   # CURSES_LIBRARIES / CURSES_INCLUDE_DIRS
        if(CURSES_FOUND)
            add_library(quetoo_curses INTERFACE)
            target_link_libraries(quetoo_curses INTERFACE ${CURSES_LIBRARIES})
            target_include_directories(quetoo_curses INTERFACE ${CURSES_INCLUDE_DIRS})
            set(QUETOO_CURSES_TARGET quetoo_curses)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# OpenGL / OpenGL ES
# ---------------------------------------------------------------------------
# Desktop: classic GL (-lGL). Android: GLES v3 + EGL, selected by QUETOO_GLES.
# The renderer source is shared; the GLES code path is gated by the QUETOO_GLES
# compile definition (added by the main CMakeLists), per PORTING.md §4.
if(ANDROID)
    # NDK system libs. GLESv3 supersets GLESv2 symbols at runtime.
    find_library(GLESv3_LIB GLESv3)
    find_library(EGL_LIB EGL)
    add_library(quetoo_gl INTERFACE)
    target_link_libraries(quetoo_gl INTERFACE "${GLESv3_LIB}" "${EGL_LIB}")
    set(QUETOO_GL_TARGET quetoo_gl)
else()
    find_package(OpenGL REQUIRED)   # OpenGL::GL
    set(QUETOO_GL_TARGET OpenGL::GL)
endif()

# ---------------------------------------------------------------------------
# libcurl (OPTIONAL — not used by the current source set; see note at top)
# ---------------------------------------------------------------------------
# Left as an opt-in so that if a future net backend (or a from-source build of
# Objectively that wants the system curl) needs it, it can be flipped on without
# editing this file's logic.
option(QUETOO_WITH_CURL "Link libcurl (not required by the current engine sources)" OFF)
set(QUETOO_CURL_TARGET "")
if(QUETOO_WITH_CURL)
    find_package(CURL REQUIRED)     # CURL::libcurl
    set(QUETOO_CURL_TARGET CURL::libcurl)
endif()
