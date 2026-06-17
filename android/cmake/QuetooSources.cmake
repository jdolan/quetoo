# QuetooSources.cmake
#
# Canonical per-component source lists for the Quetoo engine, transcribed
# VERBATIM from the autotools Makefile.am files (the authoritative source of
# truth). Paths are relative to the repo root (QUETOO_ROOT), which the including
# CMakeLists must define.
#
# IMPORTANT — why these are explicit lists and NOT file(GLOB):
#   The Makefile.am files name each .c that is compiled. The on-disk directories
#   sometimes contain .c files that are deliberately NOT part of a given target
#   (e.g. src/client/cl_null.c is compiled into libclient_null, NOT libclient;
#   src/null.c is unused). A glob would silently pull those in and break the
#   build or change behavior. If you add/remove engine sources upstream, update
#   the matching list here to match the Makefile.am.
#
# Each list is verified 1:1 against the corresponding Makefile.am as of the
# revision this was authored on. See the inline "from: <path>" comment.

set(QUETOO_SRC "${QUETOO_ROOT}/src")

# --- src/shared/Makefile.am : libshared.la -----------------------------------
set(QUETOO_SHARED_SOURCES
    "${QUETOO_SRC}/shared/parse.c"
    "${QUETOO_SRC}/shared/shared.c"
    "${QUETOO_SRC}/shared/shared-anorms.c"
    "${QUETOO_SRC}/shared/swap.c"
)

# --- src/common/Makefile.am : libcommon.la -----------------------------------
set(QUETOO_COMMON_SOURCES
    "${QUETOO_SRC}/common/atlas.c"
    "${QUETOO_SRC}/common/cmd.c"
    "${QUETOO_SRC}/common/common.c"
    "${QUETOO_SRC}/common/console.c"
    "${QUETOO_SRC}/common/cvar.c"
    "${QUETOO_SRC}/common/filesystem.c"
    "${QUETOO_SRC}/common/image.c"
    "${QUETOO_SRC}/common/installer.c"
    "${QUETOO_SRC}/common/mem.c"
    "${QUETOO_SRC}/common/mem_buf.c"
    "${QUETOO_SRC}/common/rgb9e5.c"
    "${QUETOO_SRC}/common/sys.c"
    "${QUETOO_SRC}/common/thread.c"
)

# --- src/collision/Makefile.am : libcollision.la -----------------------------
set(QUETOO_COLLISION_SOURCES
    "${QUETOO_SRC}/collision/cm_bsp.c"
    "${QUETOO_SRC}/collision/cm_entity.c"
    "${QUETOO_SRC}/collision/cm_manifest.c"
    "${QUETOO_SRC}/collision/cm_material.c"
    "${QUETOO_SRC}/collision/cm_model.c"
    "${QUETOO_SRC}/collision/cm_polylib.c"
    "${QUETOO_SRC}/collision/cm_test.c"
    "${QUETOO_SRC}/collision/cm_trace.c"
    "${QUETOO_SRC}/collision/cm_voxel.c"
)

# --- src/net/Makefile.am : libnet.la -----------------------------------------
# v78: the HTTP *client* (old net_http.c, Objectively URLSession) was removed
# upstream; net_http_server.c is the pure-C listen-server responder (no Objectively).
set(QUETOO_NET_SOURCES
    "${QUETOO_SRC}/net/net_chan.c"
    "${QUETOO_SRC}/net/net_http_server.c"
    "${QUETOO_SRC}/net/net_message.c"
    "${QUETOO_SRC}/net/net_sock.c"
    "${QUETOO_SRC}/net/net_udp.c"
)

# --- src/server/Makefile.am : libserver.la -----------------------------------
set(QUETOO_SERVER_SOURCES
    "${QUETOO_SRC}/server/sv_admin.c"
    "${QUETOO_SRC}/server/sv_client.c"
    "${QUETOO_SRC}/server/sv_console.c"
    "${QUETOO_SRC}/server/sv_editor.c"
    "${QUETOO_SRC}/server/sv_entity.c"
    "${QUETOO_SRC}/server/sv_game.c"
    "${QUETOO_SRC}/server/sv_http.c"
    "${QUETOO_SRC}/server/sv_init.c"
    "${QUETOO_SRC}/server/sv_main.c"
    "${QUETOO_SRC}/server/sv_map_list.c"
    "${QUETOO_SRC}/server/sv_master.c"
    "${QUETOO_SRC}/server/sv_send.c"
    "${QUETOO_SRC}/server/sv_world.c"
)

# --- src/client/renderer/Makefile.am : librenderer.la ------------------------
# This is the GL renderer. For Android it must be built against GL ES 3.0 via
# the QUETOO_GLES dual code path (PORTING.md §4); that switch is applied as a
# compile definition by the main CMakeLists, not here.
set(QUETOO_RENDERER_SOURCES
    "${QUETOO_SRC}/client/renderer/r_animation.c"
    "${QUETOO_SRC}/client/renderer/r_atlas.c"
    "${QUETOO_SRC}/client/renderer/r_bsp_draw.c"
    "${QUETOO_SRC}/client/renderer/r_bsp_model.c"
    "${QUETOO_SRC}/client/renderer/r_context.c"
    "${QUETOO_SRC}/client/renderer/r_cull.c"
    "${QUETOO_SRC}/client/renderer/r_decal.c"
    "${QUETOO_SRC}/client/renderer/r_depth_pass.c"
    "${QUETOO_SRC}/client/renderer/r_draw_2d.c"
    "${QUETOO_SRC}/client/renderer/r_draw_3d.c"
    "${QUETOO_SRC}/client/renderer/r_entity.c"
    "${QUETOO_SRC}/client/renderer/r_framebuffer.c"
    "${QUETOO_SRC}/client/renderer/r_gl.c"
    "${QUETOO_SRC}/client/renderer/r_image.c"
    "${QUETOO_SRC}/client/renderer/r_light.c"
    "${QUETOO_SRC}/client/renderer/r_main.c"
    "${QUETOO_SRC}/client/renderer/r_material.c"
    "${QUETOO_SRC}/client/renderer/r_media.c"
    "${QUETOO_SRC}/client/renderer/r_mesh_draw.c"
    "${QUETOO_SRC}/client/renderer/r_mesh_model.c"
    "${QUETOO_SRC}/client/renderer/r_mesh_model_md3.c"
    "${QUETOO_SRC}/client/renderer/r_mesh_model_obj.c"
    "${QUETOO_SRC}/client/renderer/r_mesh.c"
    "${QUETOO_SRC}/client/renderer/r_model.c"
    "${QUETOO_SRC}/client/renderer/r_occlude.c"
    "${QUETOO_SRC}/client/renderer/r_post.c"
    "${QUETOO_SRC}/client/renderer/r_program.c"
    "${QUETOO_SRC}/client/renderer/r_shadow.c"
    "${QUETOO_SRC}/client/renderer/r_sky.c"
    "${QUETOO_SRC}/client/renderer/r_sprite.c"
)

# --- src/client/sound/Makefile.am : libsound.la ------------------------------
set(QUETOO_SOUND_SOURCES
    "${QUETOO_SRC}/client/sound/s_main.c"
    "${QUETOO_SRC}/client/sound/s_media.c"
    "${QUETOO_SRC}/client/sound/s_mix.c"
    "${QUETOO_SRC}/client/sound/s_music.c"
    "${QUETOO_SRC}/client/sound/s_sample.c"
)

# --- src/client/ui/Makefile.am : libui.la ------------------------------------
# Uses ObjectivelyMVC (the MVC UI toolkit). On Android this is part of the
# longer-term UI story (PORTING.md); kept here for build fidelity with desktop.
set(QUETOO_UI_SOURCES
    "${QUETOO_SRC}/client/ui/QuetooRenderer.c"
    "${QUETOO_SRC}/client/ui/ui_data.c"
    "${QUETOO_SRC}/client/ui/ui_main.c"
)

# --- src/client/Makefile.am : libclient.la (full, GUI) -----------------------
set(QUETOO_CLIENT_SOURCES
    "${QUETOO_SRC}/client/cl_cgame.c"
    "${QUETOO_SRC}/client/cl_cmd.c"
    "${QUETOO_SRC}/client/cl_console.c"
    "${QUETOO_SRC}/client/cl_demo.c"
    "${QUETOO_SRC}/client/cl_entity.c"
    "${QUETOO_SRC}/client/cl_input.c"
    "${QUETOO_SRC}/client/cl_keys.c"
    "${QUETOO_SRC}/client/cl_main.c"
    "${QUETOO_SRC}/client/cl_media.c"
    "${QUETOO_SRC}/client/cl_mouse.c"
    "${QUETOO_SRC}/client/cl_parse.c"
    "${QUETOO_SRC}/client/cl_predict.c"
    "${QUETOO_SRC}/client/cl_renderer.c"
    "${QUETOO_SRC}/client/cl_screen.c"
    "${QUETOO_SRC}/client/cl_server.c"
    "${QUETOO_SRC}/client/cl_sound.c"
    "${QUETOO_SRC}/client/cl_touch.c"
)

# --- src/client/Makefile.am : libclient_null.la (headless, for dedicated) ----
set(QUETOO_CLIENT_NULL_SOURCES
    "${QUETOO_SRC}/client/cl_null.c"
)

# --- src/game/default/Makefile.am --------------------------------------------
# Two small static convenience libs shared between game.so and cgame.so ...
set(QUETOO_GAME_ITEM_SOURCES
    "${QUETOO_SRC}/game/default/bg_item.c"
)
set(QUETOO_GAME_PMOVE_SOURCES
    "${QUETOO_SRC}/game/default/bg_pmove.c"
)
# ... and the loadable game module itself (-> game.so / game.la).
set(QUETOO_GAME_SOURCES
    "${QUETOO_SRC}/game/default/g_ai_goal.c"
    "${QUETOO_SRC}/game/default/g_ai_grid.c"
    "${QUETOO_SRC}/game/default/g_ai_info.c"
    "${QUETOO_SRC}/game/default/g_ai_item.c"
    "${QUETOO_SRC}/game/default/g_ai_main.c"
    "${QUETOO_SRC}/game/default/g_ai_node.c"
    "${QUETOO_SRC}/game/default/g_ballistics.c"
    "${QUETOO_SRC}/game/default/g_client_chase.c"
    "${QUETOO_SRC}/game/default/g_client_stats.c"
    "${QUETOO_SRC}/game/default/g_client_view.c"
    "${QUETOO_SRC}/game/default/g_client.c"
    "${QUETOO_SRC}/game/default/g_cmd.c"
    "${QUETOO_SRC}/game/default/g_combat.c"
    "${QUETOO_SRC}/game/default/g_entity_func.c"
    "${QUETOO_SRC}/game/default/g_entity_info.c"
    "${QUETOO_SRC}/game/default/g_entity_misc.c"
    "${QUETOO_SRC}/game/default/g_entity_target.c"
    "${QUETOO_SRC}/game/default/g_entity_trigger.c"
    "${QUETOO_SRC}/game/default/g_entity.c"
    "${QUETOO_SRC}/game/default/g_item.c"
    "${QUETOO_SRC}/game/default/g_main.c"
    "${QUETOO_SRC}/game/default/g_physics.c"
    "${QUETOO_SRC}/game/default/g_sound.c"
    "${QUETOO_SRC}/game/default/g_util.c"
    "${QUETOO_SRC}/game/default/g_weapon.c"
)

# --- src/cgame/default/Makefile.am : cgame.la (-> cgame.so) -------------------
# NOTE: cg_discord.c IS compiled (it is in cgame_la_SOURCES even though it is not
# in noinst_HEADERS). It pulls in the discord-rpc dep.
set(QUETOO_CGAME_SOURCES
    "${QUETOO_SRC}/cgame/default/cg_client.c"
    "${QUETOO_SRC}/cgame/default/cg_discord.c"
    "${QUETOO_SRC}/cgame/default/cg_editor.c"
    "${QUETOO_SRC}/cgame/default/cg_effect.c"
    "${QUETOO_SRC}/cgame/default/cg_entity.c"
    "${QUETOO_SRC}/cgame/default/cg_entity_effect.c"
    "${QUETOO_SRC}/cgame/default/cg_entity_event.c"
    "${QUETOO_SRC}/cgame/default/cg_entity_misc.c"
    "${QUETOO_SRC}/cgame/default/cg_entity_trail.c"
    "${QUETOO_SRC}/cgame/default/cg_flare.c"
    "${QUETOO_SRC}/cgame/default/cg_hud.c"
    "${QUETOO_SRC}/cgame/default/cg_input.c"
    "${QUETOO_SRC}/cgame/default/cg_inventory.c"
    "${QUETOO_SRC}/cgame/default/cg_light.c"
    "${QUETOO_SRC}/cgame/default/cg_main.c"
    "${QUETOO_SRC}/cgame/default/cg_media.c"
    "${QUETOO_SRC}/cgame/default/cg_muzzle_flash.c"
    "${QUETOO_SRC}/cgame/default/cg_predict.c"
    "${QUETOO_SRC}/cgame/default/cg_score.c"
    "${QUETOO_SRC}/cgame/default/cg_sound.c"
    "${QUETOO_SRC}/cgame/default/cg_sprite.c"
    "${QUETOO_SRC}/cgame/default/cg_temp_entity.c"
    "${QUETOO_SRC}/cgame/default/cg_ui.c"
    "${QUETOO_SRC}/cgame/default/cg_view.c"
    "${QUETOO_SRC}/cgame/default/cg_weapon.c"
)

# --- src/cgame/default/ui/*/Makefile.am : the 9 libcgui* static libs ---------
# cgame.so links all of these. Grouped into one list; the cgame module is a
# single shared object so the static-lib boundary the autotools build uses is
# not load-bearing for us — we compile the sources straight into cgame.so.
set(QUETOO_CGAME_UI_SOURCES
    # ui/common  (libcguicommon)
    "${QUETOO_SRC}/cgame/default/ui/common/BindTextView.c"
    "${QUETOO_SRC}/cgame/default/ui/common/CvarCheckbox.c"
    "${QUETOO_SRC}/cgame/default/ui/common/CvarSelect.c"
    "${QUETOO_SRC}/cgame/default/ui/common/CvarSlider.c"
    "${QUETOO_SRC}/cgame/default/ui/common/CvarTextView.c"
    "${QUETOO_SRC}/cgame/default/ui/common/DialogViewController.c"
    # ui/controls  (libcguicontrols)
    "${QUETOO_SRC}/cgame/default/ui/controls/ControlsViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/controls/CrosshairView.c"
    "${QUETOO_SRC}/cgame/default/ui/controls/MovementCombatViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/controls/ResponseServiceViewController.c"
    # ui/credits  (libcguicredits)
    "${QUETOO_SRC}/cgame/default/ui/credits/CreditsViewController.c"
    # ui/editor  (libcguieditor)
    "${QUETOO_SRC}/cgame/default/ui/editor/EditorViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/editor/EntityView.c"
    "${QUETOO_SRC}/cgame/default/ui/editor/EntityViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/editor/MaterialViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/editor/MeshViewController.c"
    # ui/home  (libcguihome)
    "${QUETOO_SRC}/cgame/default/ui/home/HomeViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/home/LeaderboardViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/home/StatsViewController.c"
    # ui/main  (libcguimain)
    "${QUETOO_SRC}/cgame/default/ui/main/LoadingViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/main/MainView.c"
    "${QUETOO_SRC}/cgame/default/ui/main/MainViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/main/UpdateViewController.c"
    # ui/play  (libcguiplay)
    "${QUETOO_SRC}/cgame/default/ui/play/CreateServerViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/play/JoinServerViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/play/MapListCollectionItemView.c"
    "${QUETOO_SRC}/cgame/default/ui/play/MapListCollectionView.c"
    "${QUETOO_SRC}/cgame/default/ui/play/PlayerModelView.c"
    "${QUETOO_SRC}/cgame/default/ui/play/PlayerSetupViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/play/PlayViewController.c"
    # ui/settings  (libcguisettings)
    "${QUETOO_SRC}/cgame/default/ui/settings/SettingsViewController.c"
    # ui/teams  (libcguiteams)
    "${QUETOO_SRC}/cgame/default/ui/teams/TeamPlayerView.c"
    "${QUETOO_SRC}/cgame/default/ui/teams/TeamsViewController.c"
    "${QUETOO_SRC}/cgame/default/ui/teams/TeamView.c"
)

# --- src/main/Makefile.am : the launcher (main.c) ----------------------------
# Shared by the `quetoo` client and `quetoo-dedicated` server (they differ only
# by linking libclient vs libclient_null). On Android this main() becomes
# SDL_main inside libmain.so.
set(QUETOO_MAIN_SOURCES
    "${QUETOO_SRC}/main/main.c"
)

# --- src/master/Makefile.am : quetoo-master (desktop-only tool) ---------------
set(QUETOO_MASTER_SOURCES
    "${QUETOO_SRC}/master/main.c"
)

# --- src/quemap/Makefile.am : quemap (BSP/light compiler, desktop tool) -------
# Has its OWN map.c/manifest.c/material.c/polylib.c distinct from collision's.
set(QUETOO_QUEMAP_SOURCES
    "${QUETOO_SRC}/quemap/brush.c"
    "${QUETOO_SRC}/quemap/bsp.c"
    "${QUETOO_SRC}/quemap/csg.c"
    "${QUETOO_SRC}/quemap/entity.c"
    "${QUETOO_SRC}/quemap/face.c"
    "${QUETOO_SRC}/quemap/leakfile.c"
    "${QUETOO_SRC}/quemap/light.c"
    "${QUETOO_SRC}/quemap/main.c"
    "${QUETOO_SRC}/quemap/manifest.c"
    "${QUETOO_SRC}/quemap/map.c"
    "${QUETOO_SRC}/quemap/material.c"
    "${QUETOO_SRC}/quemap/patch.c"
    "${QUETOO_SRC}/quemap/polylib.c"
    "${QUETOO_SRC}/quemap/portal.c"
    "${QUETOO_SRC}/quemap/qbsp.c"
    "${QUETOO_SRC}/quemap/qlight.c"
    "${QUETOO_SRC}/quemap/qzip.c"
    "${QUETOO_SRC}/quemap/texture.c"
    "${QUETOO_SRC}/quemap/tjunction.c"
    "${QUETOO_SRC}/quemap/tree.c"
    "${QUETOO_SRC}/quemap/voxel.c"
    "${QUETOO_SRC}/quemap/work.c"
    "${QUETOO_SRC}/quemap/writebsp.c"
)

# --- deps/minizip/Makefile.am : libminizip.la (quemap dependency) ------------
set(QUETOO_MINIZIP_SOURCES
    "${QUETOO_ROOT}/deps/minizip/miniz.c"
)

# --- deps/discord-rpc/src/Makefile.am : libdiscord-rpc.la (cgame dependency) -
# C++11. The platform connection/register sources are conditional; for
# Android/Linux the LINUX branch applies (connection_unix + register_linux).
set(QUETOO_DISCORD_RPC_SOURCES
    "${QUETOO_ROOT}/deps/discord-rpc/src/discord_rpc.cpp"
    "${QUETOO_ROOT}/deps/discord-rpc/src/rpc_connection.cpp"
    "${QUETOO_ROOT}/deps/discord-rpc/src/serialization.cpp"
)
set(QUETOO_DISCORD_RPC_SOURCES_UNIX
    "${QUETOO_ROOT}/deps/discord-rpc/src/connection_unix.cpp"
    "${QUETOO_ROOT}/deps/discord-rpc/src/discord_register_linux.cpp"
)

# --- android/qglib : the glib2 replacement shim ------------------------------
# Built into EVERY target that includes <glib.h> (engine + game.so + cgame.so),
# because the containers are used pervasively across all of them (PORTING.md §3c).
set(QGLIB_DIR "${QUETOO_ROOT}/android/qglib")
set(QGLIB_SOURCES
    "${QGLIB_DIR}/qglib_array.c"
    "${QGLIB_DIR}/qglib_checksum.c"
    "${QGLIB_DIR}/qglib_hash.c"
    "${QGLIB_DIR}/qglib_io.c"
    "${QGLIB_DIR}/qglib_list.c"
    "${QGLIB_DIR}/qglib_misc.c"
    "${QGLIB_DIR}/qglib_ptrarray.c"
    "${QGLIB_DIR}/qglib_queue.c"
    "${QGLIB_DIR}/qglib_rand.c"
    "${QGLIB_DIR}/qglib_regex.c"
    "${QGLIB_DIR}/qglib_slist.c"
    "${QGLIB_DIR}/qglib_string.c"
    "${QGLIB_DIR}/qglib_util.c"
)
