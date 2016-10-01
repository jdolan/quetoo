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

#define CGAME_API_VERSION 2

/**
 * @brief The client game import struct imports engine functionailty to the client game.
 */
typedef struct cg_import_s {

	/**
	 * @brief The client structure.
	 */
	cl_client_t *client;

	/**
	 * @brief The renderer context.
	 */
	r_context_t *context;

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
	 * @brief Prints a formatted debug message the the configured consoles.
	 *
	 * @remarks If the `debug` cvar is unset, this function will simply return.
	 */
	void (*Debug_)(const char *func, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

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
	 *
	 * @return A newly allocated block of managed memory under the given `tag`.
	 */
	void *(*Malloc)(size_t size, mem_tag_t tag);

	/**
	 * @return A newly allocated block of managed memory, linked to `parent`.
	 *
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
	 *
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
	 *
	 * @param path The file path (e.g. `"maps/torn.bsp"`).
	 */
	file_t *(*OpenFile)(const char *path);

	/**
	 * @brief Seeks to the specified offset.
	 *
	 * @param file The file.
	 * @param offset The offset.
	 *
	 * @return True on success, false on error.
	 */
	_Bool (*SeekFile)(file_t *file, size_t offset);

	/**
	 * @brief Reads from the specified file.
	 *
	 * @param file The file.
	 * @param buffer The buffer into which to read.
	 * @param size The size of the objects to read.
	 * @param count The count of the objects to read.
	 *
	 * @return The number of objects read, or -1 on failure.
	 */
	int64_t (*ReadFile)(file_t *file, void *buffer, size_t size, size_t count);

	/**
	 * @brief Closes the specified file.
	 *
	 * @param file The file.
	 *
	 * @return True on success, false on error.
	 */
	_Bool (*CloseFile)(file_t *file);

	/**
	 * @brief Loads the file resource at `path` into the buffer pointed to by `buffer`.
	 *
	 * @param path The file path (Quake path, e.g. `"pics/ch1"`).
	 * @param buffer The destination buffer, which is allocated by this function.
	 *
	 * @return The size of the file, in bytes, or `-1` on error.
	 */
	int64_t (*LoadFile)(const char *path, void **buffer);

	/**
	 * @brief Frees a buffer previously returned from `LoadFile`.
	 *
	 * @param buffer The buffered file contents.
	 */
	void (*FreeFile)(void *buffer);

	/**
	 * @brief Enumerates files matching `pattern`, calling the given function.
	 *
	 * @param pattern A Unix glob style pattern.
	 * @param enumerator The enumerator function.
	 * @param data User data.
	 */
	void (*EnumerateFiles)(const char *pattern, Fs_EnumerateFunc enumerator, void *data);

	/**
	 * @}
	 *
	 * @defgroup console-variables Console variables & commands
	 * @{
	 */

	/**
	 * @brief Resolves a console variable, creating it if not found.
	 *
	 * @param name The variable name.
	 * @param value The variable value string.
	 * @param flags The variable flags (e.g. CVAR_ARCHIVE).
	 * @param desc The variable description for builtin console help.
	 *
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
	 *
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
	 * @brief Registers and returns a console command.
	 *
	 * @param name The command name (e.g. `"wave"`).
	 * @param function The command function.
	 * @param flags The command flags (e.g. CMD_CGAME).
	 * @param desc The command description for builtin console help.
	 *
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
	 * @brief Adds the specified ViewController to the user interface.
	 */
	void (*AddViewControler)(ViewController *viewController);

	/**
	 * @brief Removes the specified ViewController from the user interface.
	 */
	void (*RemoveViewController)(ViewController *viewController);

	/**
	 * @brief Resolves the next key after `from` that references `bind`.
	 *
	 * @param from The key to search from (SDL_SCANCODE_UNKNOWN to begin).
	 * @param bind The key binding to search for.
	 *
	 * @return The next key bound to `bind`.
	 */
	SDL_Scancode (*KeyForBind)(SDL_Scancode from, const char *bind);

	/**
	 * @return The human readable name of the given `key`.
	 */
	const char *(*KeyName)(SDL_Scancode key);

	/**
	 * @brief Binds or unbinds `key`.
	 *
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
	 *
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
	 *
	 * @return The character.
	 */
	int32_t (*ReadChar)(void);

	/**
	 * @brief Reads a byte from the last received network message.
	 *
	 * @return The byte.
	 */
	int32_t (*ReadByte)(void);

	/**
	 * @brief Reads a short integer from the last received network message.
	 *
	 * @return The short integer.
	 */
	int32_t (*ReadShort)(void);

	/**
	 * @brief Reads a long integer from the last received network message.
	 *
	 * @return The long integer.
	 */
	int32_t (*ReadLong)(void);

	/**
	 * @brief Reads a NULL-terminated string from the last received network message.
	 *
	 * @return The string.
	 *
	 * @remarks The returned value is statically allocated. Do not free it.
	 */
	char *(*ReadString)(void);

	/**
	 * @brief Reads a vector (float) from the last received network message.
	 *
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
	 *
	 * @remarks This checks the world model and all known solid entities.
	 */
	int32_t (*PointContents)(const vec3_t point);

	/**
	 * @brief Traces from `start` to `end`, clipping to all known solids matching the given `contents` mask.
	 *
	 * @param start The trace start point.
	 * @param end The trace end point.
	 * @param mins The trace mins, or `NULL` for point trace.
	 * @param maxs The trace maxs, or `NULL` for point trace.
	 * @param skip The entity number to skip (typically our own client).
	 * @param contents Solids matching this mask will clip the returned trace.
	 *
	 * @return A trace result.
	 */
	cm_trace_t (*Trace)(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const uint16_t skip, const int32_t contents);

	/**
	 * @param p The point to check.
	 * @param model The model to check within, or `NULL` for the world model.
	 *
	 * @return The BSP leaf at the given point `p`, in the given `model`.
	 */
	const r_bsp_leaf_t * (*LeafForPoint)(const vec3_t p, const r_bsp_model_t *model);

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
	 * @brief Loads a sound sample by the given name.
	 *
	 * @param name The sample name or alias (e.g. `"weapons/bfg/fire"`, `"*players/common/gurp"`).
	 *
	 * @return The loaded sample.
	 */
	s_sample_t *(*LoadSample)(const char *name);

	/**
	 * @brief Adds a sound sample to the playback queue.
	 *
	 * @param play The play sample.
	 */
	void (*AddSample)(const s_play_sample_t *play);

	/**
	 * @brief Resolves an RGB floating point color from the specified palette index.
	 *
	 * @param c The palette index.
	 * @param color The RGB floating point color.
	 */
	void (*ColorFromPalette)(uint8_t c, vec3_t color);

	/**
	 * @brief Sets the drawing color.
	 *
	 * @param color The RGBA drawing color.
	 */
	void (*Color)(const vec4_t color);

	/**
	 * @brief Loads the image by `name` into the SDL_Surface `surface`.
	 *
	 * @param name The image name (e.g. `"pics/ch1"`).
	 * @param surface The surface pointer to return.
	 *
	 * @return True on success, false on error.
	 */
	_Bool (*LoadSurface)(const char *name, SDL_Surface **surface);

	/**
	 * @brief Loads the image with the given name and type.
	 *
	 * @param name The image name, e.g. `"pics/health_i"`.
	 * @param type The image type, e.g. `IT_PIC`.
	 *
	 * @return The image.
	 *
	 * @remarks This function never returns `NULL`, but instead will return the null texture.
	 */
	r_image_t *(*LoadImage)(const char *name, r_image_type_t type);

	/**
	 * @brief Loads the material with the given diffuse texture name.
	 *
	 * @param name The diffuse texture name, e.g. `"models/objects/rocket/skin"`.
	 *
	 * @return The material.
	 */
	r_material_t *(*LoadMaterial)(const char *diffuse);

	/**
	 * @brief Loads the model with the given name.
	 *
	 * @param name The model name (e.g. `"models/objects/rocket/tris"`).
	 *
	 * @return The model.
	 */
	r_model_t *(*LoadModel)(const char *name);

	/**
	 * @return The world model for the currently loaded level.
	 */
	r_model_t *(*WorldModel)(void);

	/**
	 * @brief Adds a corona to the scene for the current frame.
	 */
	void (*AddCorona)(const r_corona_t *c);

	/**
	 * @brief Adds an entity to the scene for the current frame.
	 *
	 * @return The added entity.
	 */
	const r_entity_t *(*AddEntity)(const r_entity_t *e);

	/**
	 * @brief Adds a linked entity to the scene for the current frame.
	 *
	 * @param parent The entity to which this entity is to be linked (e.g. a client).
	 * @param model The linked entity model.
	 * @param tag_name The MD3 tag name to resolve in the parent for link alignment.
	 *
	 * @return The linked entity.
	 *
	 * @remarks The parent entity must have been previously returned by `AddEntity`.
	 */
	const r_entity_t *(*AddLinkedEntity)(const r_entity_t *parent, const r_model_t *model, const char *tag_name);

	/**
	 * @brief Sets the model-view-projection matrix for the given entity.
	 *
	 * @param e The entity.
	 */
	void (*SetMatrixForEntity)(r_entity_t *e);

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
	 * @defgroup draw-2d 2D drawing
	 * @{
	 */

	/**
	 * @brief Draws an image in orthographic projection on the screen.
	 *
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param scale The image scale.
	 * @param image The image.
	 */
	void (*DrawImage)(r_pixel_t x, r_pixel_t y, vec_t scale, const r_image_t *image);

	/**
	 * @brief Draws a filled rectangle in orthographic projection on the screen.
	 *
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
	 *
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
	 *
	 * @param x The x coordinate, in pixels.
	 * @param y The y coordinate, in pixels.
	 * @param s The string.
	 * @param color The color palette index.
	 *
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
	void (*LoadClient)(cl_client_info_t *cl, const char *s);

	_Bool (*ParseMessage)(int32_t cmd);
	void (*PredictMovement)(const GList *cmds);
	void (*UpdateView)(const cl_frame_t *frame);
	void (*PopulateView)(const cl_frame_t *frame);
	void (*DrawFrame)(const cl_frame_t *frame);
	
} cg_export_t;

#endif /* __CGAME_H__ */
