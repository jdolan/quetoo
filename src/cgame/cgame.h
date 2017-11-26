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

#include "client/cl_types.h"

#define CGAME_API_VERSION 19

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
	 * @brief The renderer view scene.
	 */
	r_view_t *view;

	/**
	 * @defgroup console-appending Console appending
	 * @{
	 */

	/**
	 * @brief Prints a formatted message to the configured consoles.
	 */
	void (*Print)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

	/**
	 * @brief Prints a formatted debug message to the configured consoles.
	 * @remarks If the `debug` cvar is unset, this function will simply return.
	 */
	void (*Debug_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

	/**
	 * @brief Prints a formatted player movement debug message to the configured consoles.
	 * @remarks If the `debug` cvar is unset, this function will simply return.
	 */
	void (*PmDebug)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

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
	 *
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
	 * @brief Frees the specified managed memory.
	 */
	void (*Free)(void *p);

	/**
	 * @brief Frees all managed memory allocated with the given `tag`.
	 */
	void (*FreeTag)(mem_tag_t tag);

	/**
	 * @brief Creates and starts a thread for the given function.
	 * @param name The thread name.
	 * @param run The thread function.
	 * @param data User data.
	 */
	thread_t *(*Thread)(const char *name, ThreadRunFunc run, void *data);

	/**
	 * @}
	 *
	 * @defgroup filesystem Filesystem
	 * @{
	 */

	/**
	 * @return The base directory, if running from a bundled application.
	 */
	const char *(*BaseDir)(void);

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
	_Bool (*SeekFile)(file_t *file, size_t offset);

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
	void (*EnumerateFiles)(const char *pattern, Fs_EnumerateFunc enumerator, void *data);

	/**
	 * @brief Check if a file exists or not.
	 * @return True if the specified filename exists on the search path.
	 */
	_Bool (*FileExists)(const char *path);

	/**
	 * @}
	 *
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
	cvar_t *(*Cvar)(const char *name, const char *value, uint32_t flags, const char *desc);

	/**
	 * @return The string value of the console variable with the given name.
	 */
	const char *(*CvarString)(const char *name);

	/**
	 * @return The floating point value of the console variable with the given name.
	 */
	vec_t (*CvarValue)(const char *name);

	/**
	 * @brief Resolves a console variable that is expected to be defined by the engine.
	 * @return The predefined console variable.
	 */
	cvar_t *(*CvarGet)(const char *name);

	/**
	 * @brief Sets the console variable by `name` to `string`.
	 */
	cvar_t *(*CvarSet)(const char *name, const char *string);

	/**
	 * @brief Sets the console variable by `name` to `value`.
	 */
	cvar_t *(*CvarSetValue)(const char *name, vec_t value);

	/**
	 * @brief Toggles the console variable by `name`.
	 */
	cvar_t *(*CvarToggle)(const char *name);

	/**
	 * @brief Registers and returns a console command.
	 * @param name The command name (e.g. `"wave"`).
	 * @param function The command function.
	 * @param flags The command flags (e.g. CMD_CGAME).
	 * @param desc The command description for builtin console help.
	 * @return The console command.
	 */
	cmd_t *(*Cmd)(const char *name, CmdExecuteFunc function, uint32_t flags, const char *desc);

	/**
	 * @brief Appends the specified string to the command buffer.
	 */
	void (*Cbuf)(const char *s);

	/**
	 * @}
	 */

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
	char *(*ConfigString)(uint16_t index);

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
	 * @brief Reads a NULL-terminated string from the last received network message.
	 * @return The string.
	 * @remarks The returned value is statically allocated. Do not free it.
	 */
	char *(*ReadString)(void);

	/**
	 * @brief Reads a vector (float) from the last received network message.
	 * @return The vector.
	 */
	vec_t (*ReadVector)(void);

	/**
	 * @brief Reads a positional vector from the last received network message.
	 */
	void (*ReadPosition)(vec3_t pos);

	/**
	 * @brief Reads a 32 bit precision directional vector from the last received network message.
	 */
	void (*ReadDir)(vec3_t dir);

	/**
	 * @brief Reads a 16 bit precision angle from the last received network message.
	 */
	vec_t (*ReadAngle)(void);

	/**
	 * @brief Reads a 16 bit precision angle triplet from the last received network message.
	 */
	void (*ReadAngles)(vec3_t angles);

	/**
	 * @}
	 */

	/**
	 * @return The entities string for the currently loaded level.
	 */
	const char *(*EntityString)(void);

	/**
	 * @defgroup collision Collision model
	 * @{
	 */

	/**
	 * @return The contents mask at the specified point.
	 * @remarks This checks the world model and all known solid entities.
	 */
	int32_t (*PointContents)(const vec3_t point);

	/**
	 * @brief Traces from `start` to `end`, clipping to all known solids matching the given `contents` mask.
	 * @param start The trace start point.
	 * @param end The trace end point.
	 * @param mins The trace mins, or `NULL` for point trace.
	 * @param maxs The trace maxs, or `NULL` for point trace.
	 * @param skip The entity number to skip (typically our own client).
	 * @param contents Solids matching this mask will clip the returned trace.
	 * @return A trace result.
	 */
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const uint16_t skip,
	                    const int32_t contents);

	/**
	 * @param p The point to check.
	 * @param model The model to check within, or `NULL` for the world model.
	 * @return The BSP leaf at the given point `p`, in the given `model`.
	 */
	const r_bsp_leaf_t *(*LeafForPoint)(const vec3_t p, const r_bsp_model_t *model);

	/**
	 * @return True if `leaf` is in the potentially hearable set for the current frame.
	 */
	_Bool (*LeafHearable)(const r_bsp_leaf_t *leaf);

	/**
	 * @return True if `leaf` is in the potentially visible set for the current frame.
	 */
	_Bool (*LeafVisible)(const r_bsp_leaf_t *leaf);

	/**
	 * @}
	 */

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
	vec_t (*KeyState)(button_t *key, uint32_t cmd_msec);

	/**
	 * @brief Loads a sound sample by the given name.
	 * @param name The sample name or alias (e.g. `"weapons/bfg/fire"`, `"*players/common/gurp"`).
	 * @return The loaded sample.
	 */
	s_sample_t *(*LoadSample)(const char *name);

	/**
	 * @brief Precache all sound samples for a given player model.
	 */
	void (*LoadClientSamples)(const char *model);

	/**
	 * @brief Adds a sound sample to the playback queue.
	 * @param play The play sample.
	 */
	void (*AddSample)(const s_play_sample_t *play);

	/**
	 * @brief Resolves an RGB floating point color from the specified palette index.
	 * @param c The palette index.
	 * @param color The RGB floating point color.
	 */
	void (*ColorFromPalette)(uint8_t c, vec3_t color);

	/**
	 * @brief Query if a box is visible on screen.
	 * @param mins The min bounds point.
	 * @param maxs The max bounds point.
	 * @return Returns true if the specified bounding box is completely culled by the
	 * view frustum, false otherwise.
	 */
	_Bool (*CullBox)(const vec3_t mins, const vec3_t maxs);

	/**
	 * @brief Query if a sphere is visible on screen.
	 * @param point The central point of the sphere.
	 * @param radius The radius of the sphere.
	 * @return Returns true if the specified sphere is completely culled by the
	 * view frustum, false otherwise.
	 */
	_Bool (*CullSphere)(const vec3_t point, const vec_t radius);

	/**
	 * @brief Sets the drawing color.
	 * @param color The RGBA drawing color.
	 */
	void (*Color)(const vec4_t color);

	/**
	 * @brief Loads the image by `name` into the SDL_Surface `surface`.
	 * @param name The image name (e.g. `"pics/ch1"`).
	 * @param surface The surface pointer to return.
	 * @return True on success, false on error.
	 */
	_Bool (*LoadSurface)(const char *name, SDL_Surface **surface);

	/**
	 * @brief Loads the image with the given name and type.
	 * @param name The image name, e.g. `"pics/health_i"`.
	 * @param type The image type, e.g. `IT_PIC`.
	 * @return The image.
	 * @remarks This function never returns `NULL`, but instead will return the null texture.
	 */
	r_image_t *(*LoadImage)(const char *name, r_image_type_t type);

	/**
	 * @brief Creates a blank state for an atlas and returns it.
	 * @param name The name to give to the atlas, e.g. `"my_atlas_is_cool"`
	 * @return The atlas that has been created.
	 * @remarks Start with this function, add images to it with AddImageToAtlas, then call StitchAtlas when
	 * you are ready. You can then query positions with GetAtlasImageFromAtlas.
	 */
	r_atlas_t *(*CreateAtlas)(const char *name);

	/**
	 * @brief Add an image to the list of images for this atlas.
	 * @param atlas The atlas to add an image to.
	 * @param image The image to add to the atlas.
	 */
	void (*AddImageToAtlas)(r_atlas_t *atlas, const r_image_t *image);

	/**
	 * @brief Resolve an atlas image from an atlas and image.
	 * @param atlas The atlas to fetch the stitched image from.
	 * @param image The original image you wish to query.
	 * @return An r_atlas_image_t which contains the stitched coordinates of that image.
	 */
	const r_atlas_image_t *(*GetAtlasImageFromAtlas)(const r_atlas_t *atlas, const r_image_t *image);

	/**
	 * @brief Compiles the specified atlas.
	 * @param atlas The atlas to stitch together and produce the image for.
	 */
	void (*CompileAtlas)(r_atlas_t *atlas);

	/**
	 * @brief Loads the material with the given name.
	 * @param name The material name, e.g. `"objects/rocket/skin"`.
	 * @param context The asset context, e.g. `ASSET_CONTEXT_PLAYERS`.
	 * @return The material.
	 */
	r_material_t *(*LoadMaterial)(const char *name, cm_asset_context_t context);

	/**
	 * @brief Loads all materials defined in the given file.
	 * @param path The materials file path, e.g. `"materials/torn.mat"`.
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
	 * @brief Adds an entity to the scene for the current frame.
	 * @return The added entity.
	 */
	r_entity_t *(*AddEntity)(const r_entity_t *e);

	/**
	 * @brief Adds a linked entity to the scene for the current frame.
	 * @param parent The entity to which this entity is to be linked (e.g. a client).
	 * @param model The linked entity model.
	 * @param tag_name The MD3 tag name to resolve in the parent for link alignment.
	 * @return The linked entity.
	 * @remarks The parent entity must have been previously returned by `AddEntity`.
	 */
	r_entity_t *(*AddLinkedEntity)(const r_entity_t *parent, const r_model_t *model, const char *tag_name);

	/**
	 * @brief Sets the model-view-projection matrix for the given entity.
	 * @param e The entity.
	 */
	void (*SetMatrixForEntity)(r_entity_t *e);

	/**
	 * @brief Returns the desired tag structure, or `NULL`.
	 * @param mod The model to check for the specified tag.
	 * @param name The name of the tag.
	 * @param frame The frame to fetch the tag on.
	 * @return The tag structure.
	 */
	const r_model_tag_t *(*MeshModelTag)(const r_model_t *mod, const char *name, const int32_t frame);

	/**
	 * @brief Change the matrix identified by "id" with the values from "matrix".
	 * @param id The matrix ID.
	 * @param matrix The new matrix.
	 */
	void (*SetMatrix)(const r_matrix_id_t id, const matrix4x4_t *matrix);

	/**
	 * @brief Fetch the matrix identified by "id" and stick it in "matrix".
	 * @param id The matrix ID.
	 * @param matrix The matrix to store the active matrix in.
	 */
	void (*GetMatrix)(const r_matrix_id_t id, matrix4x4_t *matrix);

	/**
	 * @brief Push the active matrix into the stack
	 */
	void (*PushMatrix)(const r_matrix_id_t id);

	/**
	 * @brief Pop a saved matrix from the stack
	 */
	void (*PopMatrix)(const r_matrix_id_t id);

	/**
	 * @brief Draws the mesh model diffuse pass for the given entity.
	 */
	void (*DrawMeshModel)(const r_entity_t *e);

	/**
	 * @brief Draws the mesh model materials for the given entity.
	 */
	void (*DrawMeshModelMaterials)(const r_entity_t *e);

	/**
	 * @brief Toggle the state of depth testing.
	 */
	void (*EnableDepthTest)(_Bool enable);

	/**
	 * @brief Set the range of depth testing.
	 */
	void (*DepthRange)(double znear, double zfar);

	/**
	 * @brief Toggle the specified texunit.
	 */
	void (*EnableTexture)(const r_texunit_id_t tex, _Bool enable);

	/**
	 * @brief Change the rendering viewport.
	 */
	void (*SetViewport)(GLint x, GLint y, GLsizei width, GLsizei height, _Bool force);

	/**
	 * @brief Adds an instantaneous light to the scene for the current frame.
	 */
	void (*AddLight)(const r_light_t *l);

	/**
	 * @brief Adds a particle to the scene for the current frame.
	 */
	void (*AddParticle)(const r_particle_t *p);

	/**
	 * @brief Adds a sustained light to the scene.
	 */
	void (*AddSustainedLight)(const r_sustained_light_t *s);

	/**
	 * @brief Add a stain to the scene.
	 */
	void (*AddStain)(const r_stain_t *s);

	/**
	 * @defgroup draw-2d 2D drawing
	 * @{
	 */

	/**
	 * @brief Draws an image in orthographic projection on the screen.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param scale The image scale.
	 * @param image The image.
	 */
	void (*DrawImage)(r_pixel_t x, r_pixel_t y, vec_t scale, const r_image_t *image);

	/**
	 * @brief Draws an image with an arbitrary size on the screen.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param x The width, in pixels.
	 * @param y The height, in pixels.
	 * @param image The image.
	 */
	void (*DrawImageResized)(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const r_image_t *image);

	/**
	 * @brief Draws a filled rectangle in orthographic projection on the screen.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param w The width, in pixels.
	 * @param h The height, in pixels.
	 * @param c The color palette index.
	 * @param a The alpha component.
	 */
	void (*DrawFill)(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int32_t c, vec_t a);

	/**
	 * @brief Binds the font by `name`, optionally returning the character width and height.
	 * @param name The font name (e.g. `"small"`).
	 * @param cw The optional return pointer for the character width.
	 * @param ch The optional return pointer for the character height.
	 */
	void (*BindFont)(const char *name, r_pixel_t *cw, r_pixel_t *ch);

	/**
	 * @return The width of the string `s` in pixels, using the currently bound font.
	 */
	r_pixel_t (*StringWidth)(const char *s);

	/**
	 * @brief Draws the string `s` at the given coordinates.
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param s The string.
	 * @param color The color palette index.
	 * @return The number of visible characters drawn.
	 */
	size_t (*DrawString)(r_pixel_t x, r_pixel_t y, const char *s, int32_t color);

	/**
	 * @}
	 */

} cg_import_t;

/**
 * @brief The client game export struct exports client game functionality to the engine.
 */
typedef struct cg_export_s {

	uint16_t api_version;
	uint16_t protocol;

	void (*Init)(void);
	void (*Shutdown)(void);
	void (*ClearState)(void);
	void (*UpdateMedia)(void);
	void (*UpdateConfigString)(uint16_t index);
	_Bool (*ParseMessage)(int32_t cmd);
	void (*Look)(pm_cmd_t *cmd);
	void (*Move)(pm_cmd_t *cmd);
	void (*Interpolate)(const cl_frame_t *frame);
	_Bool (*UsePrediction)(void);
	void (*PredictMovement)(const GList *cmds);
	void (*UpdateLoading)(const cl_loading_t loading);
	void (*UpdateView)(const cl_frame_t *frame);
	void (*UpdateScreen)(const cl_frame_t *frame);
	void (*UpdateDiscord)(void);

} cg_export_t;

#endif /* __CGAME_H__ */
