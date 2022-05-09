/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __CGAME_H__
#define __CGAME_H__

#ifndef CGAME_EXPORT
 #if defined(_WIN32)
  #define CGAME_EXPORT __declspec(dllexport)
 #else
  #define CGAME_EXPORT extern
 #endif
#endif

#include "client/cl_types.h"

#define CGAME_API_VERSION 23

/**
 * @brief The client game import struct imports engine functionailty to the client game.
 */
typedef struct cg_import_s {

	/**
	 * @brief The client structure.
	 */
	cl_client_t *client;

	/**
	 * @brief The client state.
	 */
	const cl_state_t *state;

	/**
	 * @brief The server name we're connecting/connected to.
	 */
	const char *server_name;

	/**
	 * @brief The renderer context.
	 */
	const r_context_t *context;

	/**
	 * @brief The renderer view definition.
	 */
	r_view_t *view;

	/**
	 * @brief The sound stage.
	 */
	s_stage_t *stage;

	/**
	 * @defgroup console-appending Console appending
	 * @{
	 */

	/**
	 * @brief Prints a formatted message to the configured consoles.
	 */
	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

	/**
	 * @return The active debug mask.
	 */
	debug_t (*DebugMask)(void);

	/**
	 * @brief Prints a formatted debug message to the configured consoles.
	 * @details If the proivided `debug` mask is inactive, the message will not be printed.
	 */
	void (*Debug_)(const debug_t debug, const char *func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

	/**
	 * @brief Prints a formatted warning message to the configured consoles.
	 */
	void (*Warn_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

	/**
	 * @brief Prints a formattet error message to the configured consoles.
	 */
	void (*Error_)(const char *func, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

	/**
	 * @}
	 * @defgroup installer Installer
	 * @{
	 */

	/**
	 * @brief Checks for available updates via the official distribution site.
	 * @return Zero if no updates are available, non-zero if updates are available.
	 */
	int32_t (*CheckForUpdates)(void);

	/**
	 * @brief Launches the installer to fetch available updates.
	 * @return Zero if the installer was successfully launched, non-zero on error.
	 */
	int32_t (*LaunchInstaller)(void);

	/**
	 * @}
	 * @defgroup memory-management Memory management
	 * @{
	 */

	/**
	 * @param tag The tag to associate the managed block with (e.g. MEM_TAG_CGAME_LEVEL).
	 * @return A newly allocated block of managed memory under the given `tag`.
	 */
	void *(*Malloc)(size_t size, mem_tag_t tag);

	/**
	 * @return A newly allocated block of managed memory, linked to `parent`.
	 * @remarks The returned memory will be freed automatically when `parent` is freed.
	 */
	void *(*LinkMalloc)(size_t size, void *parent);

	/**
	 * @brief Reallocates the dynamic memory block `p` to the specified size.
	 * @return The reallocated memory. If `p` is grown, the newly allocated region is not cleared.
	 */
	void *(*Realloc)(void *p, size_t size);

	/**
	 * @brief Frees the specified managed memory.
	 */
	void (*Free)(void *p);

	/**
	 * @brief Frees all managed memory allocated with the given `tag`.
	 */
	void (*FreeTag)(mem_tag_t tag);

	/**
	 * @}
	 * @defgroup threads Threads
	 * @{
	 */

	/**
	 * @brief Starts a thread for the given function.
	 * @param name The thread name.
	 * @param run The thread function.
	 * @param data User data.
	 * @param options The thread options.
	 * @remarks Unless `THREAD_NO_WAIT` is passed via `options`, the caller must also
	 * call `Wait` on the returned thread in order to relinquish it to the thread pool.
	 */
	thread_t *(*Thread)(const char *name, ThreadRunFunc run, void *data, thread_options_t options);

	/**
	 * @brief Waits for the previously started thread, blocking the calling thread.
	 * @param thread The thread.
	 */
	void (*Wait)(thread_t *thread);

	/**
	 * @}
	 * @defgroup filesystem Filesystem
	 * @{
	 */

	/**
	 * @brief Opens the specified file for reading.
	 * @param path The file path (e.g. `"maps/torn.bsp"`).
	 */
	file_t *(*OpenFile)(const char *path);

	/**
	 * @brief Seeks to the specified offset.
	 * @param file The file.
	 * @param offset The offset.
	 * @return True on success, false on error.
	 */
	_Bool (*SeekFile)(file_t *file, int64_t offset);

	/**
	 * @brief Reads from the specified file.
	 * @param file The file.
	 * @param buffer The buffer into which to read.
	 * @param size The size of the objects to read.
	 * @param count The count of the objects to read.
	 * @return The number of objects read, or -1 on failure.
	 */
	int64_t (*ReadFile)(file_t *file, void *buffer, size_t size, size_t count);

	/**
	 * @brief Opens the specified file for writing.
	 * @param path The file path (e.g. `"maps.ui.list"`).
	 */
	file_t *(*OpenFileWrite)(const char *path);

	/**
	 * @brief Writes `count` objects of size `size` from `buffer` to `file`.
	 * @param file The file.
	 * @param buffer The buffer to write from.
	 * @param size The size of the objects to write.
	 * @param count The count of the objecst to write.
	 * @return The number of objects written, or `-1` on error.
	 */
	int64_t (*WriteFile)(file_t *file, const void *buffer, size_t size, size_t count);

	/**
	 * @brief Closes the specified file.
	 * @param file The file.
	 * @return True on success, false on error.
	 */
	_Bool (*CloseFile)(file_t *file);

	/**
	 * @brief Loads the file resource at `path` into the buffer pointed to by `buffer`.
	 * @param path The file path (Quake path, e.g. `"pics/ch1"`).
	 * @param buffer The destination buffer, which is allocated by this function.
	 * @return The size of the file, in bytes, or `-1` on error.
	 */
	int64_t (*LoadFile)(const char *path, void **buffer);

	/**
	 * @brief Frees a buffer previously returned from `LoadFile`.
	 * @param buffer The buffered file contents.
	 */
	void (*FreeFile)(void *buffer);

	/**
	 * @brief Enumerates files matching `pattern`, calling the given function.
	 * @param pattern A Unix glob style pattern.
	 * @param enumerator The enumerator function.
	 * @param data User data.
	 */
	void (*EnumerateFiles)(const char *pattern, Fs_Enumerator enumerator, void *data);

	/**
	 * @brief Check if a file exists or not.
	 * @return True if the specified filename exists on the search path.
	 */
	_Bool (*FileExists)(const char *path);

	/**
	 * @}
	 * @defgroup console-variables Console variables & commands
	 * @{
	 */

	/**
	 * @brief Resolves a console variable, creating it if not found.
	 * @param name The variable name.
	 * @param value The variable value string.
	 * @param flags The variable flags (e.g. CVAR_ARCHIVE).
	 * @param desc The variable description for builtin console help.
	 * @return The console variable.
	 */
	cvar_t *(*AddCvar)(const char *name, const char *value, uint32_t flags, const char *desc);

	/**
	 * @brief Resolves a console variable that is expected to be defined by the engine.
	 * @return The predefined console variable.
	 */
	cvar_t *(*GetCvar)(const char *name);

	/**
	 * @return The integer value of the console variable with the given name.
	 */
	int32_t (*GetCvarInteger)(const char *name);

	/**
	 * @return The string value of the console variable with the given name.
	 */
	const char *(*GetCvarString)(const char *name);

	/**
	 * @return The floating point value of the console variable with the given name.
	 */
	float (*GetCvarValue)(const char *name);

	/**
	 * @brief Sets the console variable by `name` to `value`.
	 */
	cvar_t *(*SetCvarInteger)(const char *name, int32_t value);

	/**
	 * @brief Sets the console variable by `name` to `string`.
	 */
	cvar_t *(*SetCvarString)(const char *name, const char *string);

	/**
	 * @brief Sets the console variable by `name` to `value`.
	 */
	cvar_t *(*SetCvarValue)(const char *name, float value);

	/**
	 * @brief Forces the console variable to take the value of the string immediately.
	 * @param name The variable name.
	 * @param string The variable string.
	 * @return The modified variable.
	 */
	cvar_t *(*ForceSetCvarString)(const char *name, const char *string);

	/**
	 * @brief Forces the console variable to take the given value immediately.
	 * @param name The variable name.
	 * @param value The variable value.
	 * @return The modified variable.
	 */
	cvar_t *(*ForceSetCvarValue)(const char *name, float value);

	/**
	 * @brief Toggles the console variable by `name`.
	 */
	cvar_t *(*ToggleCvar)(const char *name);

	/**
	 * @brief Registers and returns a console command.
	 * @param name The command name (e.g. `"wave"`).
	 * @param function The command function.
	 * @param flags The command flags (e.g. CMD_CGAME).
	 * @param desc The command description for builtin console help.
	 * @return The console command.
	 */
	cmd_t *(*AddCmd)(const char *name, CmdExecuteFunc function, uint32_t flags, const char *desc);

	/**
	 * @brief Appends the specified string to the command buffer.
	 */
	void (*Cbuf)(const char *s);

	/**
	 * @}
	 * @defgroup ui User interface
	 * @{
	 */

	/**
	 * @brief Resolves the current Theme.
	 */
	Theme *(*Theme)(void);

	/**
	 * @brief Pushes the specified ViewController to the user interface.
	 */
	void (*PushViewController)(ViewController *viewController);

	/**
	 * @brief Pops all higher ViewControllers from the user interface to make the selected one visible.
	 */
	void (*PopToViewController)(ViewController *viewController);

	/**
	 * @brief Pops the top ViewController from the user interface.
	 */
	void (*PopViewController)(void);

	/**
	 * @brief Pops all ViewControllers from the user interface.
	 */
	void (*PopAllViewControllers)(void);

	/**
	 * @}
	 */

	/**
	 * @brief Resolves the next key after `from` that references `bind`.
	 * @param from The key to search from (SDL_SCANCODE_UNKNOWN to begin).
	 * @param bind The key binding to search for.
	 * @return The next key bound to `bind`.
	 */
	SDL_Scancode (*KeyForBind)(SDL_Scancode from, const char *bind);

	/**
	 * @return The human readable name of the given `key`.
	 */
	const char *(*KeyName)(SDL_Scancode key);

	/**
	 * @brief Binds or unbinds `key`.
	 * @param key The key to bind or unbind.
	 * @param bind The binding, or `NULL` to unbind.
	 */
	void (*BindKey)(SDL_Scancode key, const char *bind);

	/**
	 * @return The list of known servers (cl_server_info_t).
	 */
	GList *(*Servers)(void);

	/**
	 * @brief Refreshes the list of known servers from the master and LAN.
	 */
	void (*GetServers)(void);

	/**
	 * @brief Initiates the connection sequence to the specified server address.
	 * @param addr The network server address.
	 */
	void (*Connect)(const net_addr_t *addr);

	/**
	 * @return The list of mapshots (char *) for the given map.
	 */
	GList *(*Mapshots)(const char *map);

	/**
	 * @return The configuration string at `index`.
	 */
	char *(*ConfigString)(int32_t index);

	/**
	 * @defgroup network Network messaging
	 * @{
	 */

	/**
	 * @brief Reads up to `len` bytes of data from the last received network message into `buf`.
	 */
	void (*ReadData)(void *buf, size_t len);

	/**
	 * @brief Reads a character from the last received network message.
	 * @return The character.
	 */
	int32_t (*ReadChar)(void);

	/**
	 * @brief Reads a byte from the last received network message.
	 * @return The byte.
	 */
	int32_t (*ReadByte)(void);

	/**
	 * @brief Reads a short integer from the last received network message.
	 * @return The short integer.
	 */
	int32_t (*ReadShort)(void);

	/**
	 * @brief Reads a long integer from the last received network message.
	 * @return The long integer.
	 */
	int32_t (*ReadLong)(void);

	/**
	 * @brief Reads a null-terminated string from the last received network message.
	 * @return The string.
	 * @remarks The returned value is statically allocated. Do not free it.
	 */
	char *(*ReadString)(void);

	/**
	 * @brief Reads a vector (float) from the last received network message.
	 * @return The vector.
	 */
	float (*ReadFloat)(void);

	/**
	 * @brief Reads a positional vector from the last received network message.
	 */
	vec3_t (*ReadPosition)(void);

	/**
	 * @brief Reads a 32 bit precision directional vector from the last received network message.
	 */
	vec3_t (*ReadDir)(void);

	/**
	 * @brief Reads a 16 bit precision angle from the last received network message.
	 */
	float (*ReadAngle)(void);

	/**
	 * @brief Reads a 16 bit precision angle triplet from the last received network message.
	 */
	vec3_t (*ReadAngles)(void);

	/**
	 * @}
	 * @defgroup collision Collision model
	 * @{
	 */

	/**
	 * @brief Finds the key-value pair for `key` within the specifed entity.
	 * @param entity The entity.
	 * @param key The entity key.
	 * @return The key-value pair for the specified key within entity.
	 * @remarks This function will always return non-NULL for convenience. Check the
	 * parsed types on the returned pair to differentiate "not present" from "0."
	 */
	const cm_entity_t *(*EntityValue)(const cm_entity_t *entity, const char *key);

	/**
	 * @brief Finds all brushes within the specified entity.
	 * @param entity The entity.
	 * @return A pointer array of brushes originally defined within `entity`.
	 * @remarks This function returns the brushes within an entity as it was defined
	 * in the source .map file. Even `func_group` and other entities which have their
	 * brushes merged into `worldspawn` during the compilation step are fully supported.
	 */
	GPtrArray *(*EntityBrushes)(const cm_entity_t *entity);

	/**
	 * @return The contents mask at the specified point.
	 * @param point The point to test.
	 * @remarks This checks the world model and all known solid entities.
	 */
	int32_t (*PointContents)(const vec3_t point);

	/**
	 * @return The contents mask of all leafs within the specified box.
	 * @param bounds The bounding box to test.
	 * @remarks This checks the world model and all known solid entities.
	 */
	int32_t (*BoxContents)(const box3_t bounds);

	/**
	 * @return True if `point` resides inside `brush`, falses otherwise.
	 * @param point The point to test.
	 * @param brush The brush to test against.
	 * @remarks This function is useful for testing points against non-solid brushes
	 * from brush entities. For general purpose collision detection, use PointContents.
	 */
	_Bool (*PointInsideBrush)(const vec3_t point, const cm_bsp_brush_t *brush);

	/**
	 * @brief Traces from `start` to `end`, clipping to all known solids matching the given `contents` mask.
	 * @param start The trace start point.
	 * @param end The trace end point.
	 * @param bounds The trace bounds, or `Box3_Zero()` for point/line trace.
	 * @param skip The entity number to skip (typically our own client).
	 * @param contents Solids matching this mask will clip the returned trace.
	 * @return A trace result.
	 */
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const box3_t bounds, int32_t skip, int32_t contents);

	/**
	 * @}
	 */

	/**
	 * @brief Set the keyboard input destination.
	 */
	void (*SetKeyDest)(cl_key_dest_t dest);

	/**
	 * @brief Register a button as being held down.
	 */
	void (*KeyDown)(button_t *b);

	/**
	 * @brief Register a button as being released.
	 */
	void (*KeyUp)(button_t *b);

	/**
	 * @brief Returns the fraction of the command interval for which the key was down.
	 */
	float (*KeyState)(button_t *key, uint32_t cmd_msec);

	/**
	 * @brief Update the loading progress during media loading.
	 * @param percent The percent. Positive values for absolute, negaitve for relative increment.
	 * @param status The status message.
	 */
	void (*LoadingProgress)(int32_t percent, const char *status);

	/**
	 * @brief Loads a sound sample by the given name.
	 * @param name The sample name or alias (e.g. `"weapons/bfg/fire"`, `"#players/common/gurp"`).
	 * @return The loaded sample.
	 */
	s_sample_t *(*LoadSample)(const char *name);

	/**
	 * @brief Loads a sound sample for the given player model and name.
	 * @param model The player model name (e.g. `"qforcer"`).
	 * @param name The sample name (e.g. `"*gurp"`).
	 * @return The loaded sample, which may be an aliased common sample.
	 */
	s_sample_t *(*LoadClientModelSample)(const char *model, const char *name);

	/**
	 * @brief Precache all sound samples for a given player model.
	 */
	void (*LoadClientModelSamples)(const char *model);

	/**
	 * @brief Adds a sound sample to the playback queue.
	 * @param stage The sound stage.
	 * @param play The play sample.
	 */
	void (*AddSample)(s_stage_t *stage, const s_play_sample_t *play);

	/**
	 * @brief Creates an OpenGL framebuffer with color and depth attachments.
	 * @param width The framebuffer width, in pixels.
	 * @param height The framebuffer height, in pixels.
	 * @param attachments The framebuffer attachments.
	 * @return The framebuffer.
	 */
	r_framebuffer_t (*CreateFramebuffer)(GLint width, GLint height, int32_t attachments);

	/**
	 * @brief Destroys the specified framebuffer, releasing any OpenGL resources.
	 * @param framebuffer The framebuffer to destroy.
	 */
	void (*DestroyFramebuffer)(r_framebuffer_t *framebuffer);

	/**
	 * @brief Blits the framebuffer to the specified rectangle on the screen.
	 * @param framebuffer The framebuffer to blit.
	 * @param x The horizontal origin of the screen rectangle in drawable pixels.
	 * @param y The vertical origin of the screen rectangle in drawable pixels.
	 * @param w The width of the screen rectangle in drawable pixels.
	 * @param h The height of the screen rectangle in drawable pixels.
	 */
	void (*BlitFramebuffer)(const r_framebuffer_t *framebuffer, GLint x, GLint y, GLint w, GLint h);

	/**
	 * @brief Loads the image by `name` into the SDL_Surface `surface`.
	 * @param name The image name (e.g. `"pics/ch1"`).
	 * @return The surface, or `NULL` if it could not be loaded.
	 */
	SDL_Surface *(*LoadSurface)(const char *name);

	/**
	 * @brief Loads the image with the given name and type.
	 * @param name The image name, e.g. `"pics/health_i"`.
	 * @param type The image type, e.g. `IT_PIC`.
	 * @return The image.
	 * @remarks This function never returns `NULL`, but instead will return the null texture.
	 */
	r_image_t *(*LoadImage)(const char *name, r_image_type_t type);

	/**
	 * @brief Loads or creates an image atlas.
	 * @param name The name to give to the atlas, e.g. `"cg_particle_atlas"`
	 * @return The atlas that has been created.
	 */
	r_atlas_t *(*LoadAtlas)(const char *name);

	/**
	 * @brief Load an image into an atlas. The atlas must be [re]compiled.
	 * @param atlas The atlas to add an image to.
	 * @param image The image to add to the atlas.
	 * @return The atlas image, or a placeholder if the image could not be loaded.
	 */
	r_atlas_image_t *(*LoadAtlasImage)(r_atlas_t *atlas, const char *name, r_image_type_t type);

	/**
	 * @brief Compiles the specified atlas, preparing all atlas images it contains for rendering.
	 * @param atlas The atlas to stitch together and produce the image for.
	 */
	void (*CompileAtlas)(r_atlas_t *atlas);

	/**
	 * @brief Creates an animation.
	 * @param name The name to give to the animation, e.g. `"cg_flame_1"`
	 * @param num_images The number of images in the image pointer list.
	 * @param images The image pointer list.
	 * @return The animation that has been created.
	 */
	r_animation_t *(*CreateAnimation)(const char *name, int32_t num_images, const r_image_t **images);

	/**
	 * @brief Loads the material with the given name.
	 * @param name The material name, e.g. `"objects/rocket/skin"`.
	 * @param context The asset context, e.g. `ASSET_CONTEXT_PLAYERS`.
	 * @return The material.
	 */
	r_material_t *(*LoadMaterial)(const char *name, cm_asset_context_t context);

	/**
	 * @brief Loads all materials defined in the given file.
	 * @param path The materials file path, e.g. `"maps/torn.mat"`.
	 * @param context The asset context, e.g. `ASSET_CONTEXT_TEXTURES`.
	 * @param materials The list of materials to prepend.
	 * @return The number of materials loaded.
	 */
	ssize_t (*LoadMaterials)(const char *path, cm_asset_context_t context, GList **materials);

	/**
	 * @brief Loads the model with the given name.
	 * @param name The model name (e.g. `"models/objects/rocket/tris"`).
	 * @return The model.
	 */
	r_model_t *(*LoadModel)(const char *name);

	/**
	 * @return The world model for the currently loaded level.
	 */
	r_model_t *(*WorldModel)(void);

	/**
	 * @defgroup scene Scene management
	 * @{
	 */

	/**
	 * @brief Adds an entity to the scene for the current frame.
	 * @return The added entity.
	 */
	r_entity_t *(*AddEntity)(r_view_t *view, const r_entity_t *e);

	/**
	 * @brief Adds an instantaneous light to the scene for the current frame.
	 */
	void (*AddLight)(r_view_t *view, const r_light_t *l);
	
	/**
	 * @brief Adds a sprite to the scene for the current frame.
	 */
	r_sprite_t *(*AddSprite)(r_view_t *view, const r_sprite_t *p);
	
	/**
	 * @brief Adds a beam to the scene for the current frame.
	 */
	r_beam_t *(*AddBeam)(r_view_t *view, const r_beam_t *p);

	/**
	 * @brief Add a stain to the scene.
	 */
	void (*AddStain)(r_view_t *view, const r_stain_t *s);

	/**
	 * @brief Draws the player model view.
	 */
	void (*DrawPlayerModelView)(r_view_t *view);

	/**
	 * @}
	 * @defgroup draw-2d 2D drawing
	 * @{
	 */

	/**
	 * @brief Binds the font by `name`, optionally returning the character width and height.
	 * @param name The font name (e.g. `"small"`).
	 * @param cw The optional return pointer for the character width.
	 * @param ch The optional return pointer for the character height.
	 */
	void (*BindFont)(const char *name, GLint *cw, GLint *ch);

	/**
	 * @brief Draws a filled rectangle in orthographic projection on the screen.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param w The width, in pixels.
	 * @param h The height, in pixels.
	 * @param c The color.
	 * @param a The alpha component.
	 */
	void (*Draw2DFill)(GLint x, GLint y, GLint w, GLint h, const color_t color);

	/**
	 * @brief Draws an image in orthographic projection on the screen.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param x The width, in pixels.
	 * @param y The height, in pixels.
	 * @param image The image.
	 * @param color The color.
	 */
	void (*Draw2DImage)(GLint x, GLint y, GLint w, GLint h, const r_image_t *image, const color_t color);

	/**
	 * @brief Draws the framebuffer color attachment in orthographic projection on the screen.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param x The width, in pixels.
	 * @param y The height, in pixels.
	 * @param image The image.
	 * @param color The color.
	 * @remarks This function uses deferred rendering, allowing framebuffers to be used as
	 * textures in menus or on the HUD. For drawing directly and immediately to the screen,
	 * use BlitFramebuffer.
	 */
	void (*Draw2DFramebuffer)(GLint x, GLint y, GLint w, GLint h, const r_framebuffer_t *framebuffer, const color_t color);

	/**
	 * @brief Draws the string `s` at the given coordinates.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param s The string.
	 * @param color The color.
	 * @return The number of visible characters drawn.
	 */
	size_t (*Draw2DString)(GLint x, GLint y, const char *s, const color_t color);

	/**
	 * @return The width of the string `s` in pixels, using the currently bound font.
	 */
	GLint (*StringWidth)(const char *s);

	/**
	 * @brief Draw a 3D line at the given coordinates.
	 * @param points Pointer to point coordinates.
	 * @param count Number of point coordinates.
	 * @param color Color of lines.
	*/
	void (*Draw3DLines)(const vec3_t *points, size_t count, const color_t color);
	
	/**
	 * @brief Draw a 3D bbox at the given coordinates.
	 * @param bounds Box.
	 * @param color Color.
	 * @param solid Whether to draw a solid or wireframe box.
	*/
	void (*Draw3DBox)(const box3_t bounds, const color_t color, const _Bool solid);

	/**
	 * @}
	 */

} cg_import_t;

/**
 * @brief The client game export struct exports client game functionality to the engine.
 */
typedef struct cg_export_s {

	int32_t api_version;
	int32_t protocol;

	/**
	 * @brief Initializes the client game.
	 * @details Called once when the client game module is loaded.
	 */
	void (*Init)(void);

	/**
	 * @brief Deinitializes the client game.
	 * @details Called once when the client game module is unloaded.
	 */
	void (*Shutdown)(void);

	/**
	 * @brief Clears client game state on disconnect or error events.
	 */
	void (*ClearState)(void);

	/**
	 * @brief Loads client game media on level load or subsystem restarts.
	 */
	void (*LoadMedia)(void);

	/**
	 * @brief Frees client game media on shutdown or subsystem restarts.
	 */
	void (*FreeMedia)(void);

	/**
	 * @brief Called when a server message known to the client is received.
	 * @param cmd The message type (e.g. SV_CMD_SOUND).
	 * @param data The parsed type-specific data or NULL.
	 */
	void (*ParsedMessage)(int32_t cmd, void *data);

	/**
	 * @brief Called when a server message not known to the client is received.
	 * @param cmd The message type.
	 * @details This allows the game and client game to define their own custom message types.
	 */
	_Bool (*ParseMessage)(int32_t cmd);

	/**
	 * @brief Called each frame to update the current movement command angles.
	 * @param cmd The current movement command.
	 */
	void (*Look)(pm_cmd_t *cmd);

	/**
	 * @brief Called each frame to updarte the current movement command movement.
	 * @param cmd The current movement command.
	 */
	void (*Move)(pm_cmd_t *cmd);

	/**
	 * @brief Called each client frame to interpolate the most recently received server frames.
	 * @details This does not populate the view with frame entities. Rather, this advances the
	 * simulation for each entity within the frame.
	 */
	void (*Interpolate)(const cl_frame_t *frame);

	/**
	 * @brief Called to determine if client side prediction should be used for the current frame.
	 * @details Third person, chasecam, or other conditions may prompt the client game to disable
	 * client side prediction.
	 */
	_Bool (*UsePrediction)(void);

	/**
	 * @brief Called each frame to run all pending movement commands and update the client's
	 * predicted state.
	 */
	void (*PredictMovement)(const GPtrArray *cmds);

	/**
	 * @brief Called during the loading process to allow the client game to update the loading
	 * screen.
	 */
	void (*UpdateLoading)(const cl_loading_t loading);

	/**
	 * @brief Called each frame to update the view definition and sound stage.
	 * @details This function should perform the minimal amount of work required to dispatch
	 * the depth pre-pass render. The scene should not be populated with entities, samples, etc.
	 */
	void (*PrepareScene)(const cl_frame_t *frame);

	/**
	 * @brief Called each frame to populate the view definition and sound stage.
	 * @details This function should add entities, sprites, lights, samples, etc. to the view
	 * definition and sound stage.
	 */
	void (*PopulateScene)(const cl_frame_t *frame);

	/**
	 * @brief Called each frame to draw any non-view visual elements, such as the HUD.
	 */
	void (*UpdateScreen)(const cl_frame_t *frame);
	void (*UpdateDiscord)(void);

} cg_export_t;

#endif /* __CGAME_H__ */
