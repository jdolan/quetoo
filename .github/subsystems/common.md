# Common Systems (common/)

Core engine systems used by both client and server: console, cvars, filesystem, memory management, threading.

## Location

`src/common/` - Shared common code

## Core Systems

### Console (console.c/h)

**Text-based command and output system**:
- Command registration: `Cmd_Add()`, `Cmd_Remove()`
- Command execution: `Cmd_Execute()`
- Output: `Com_Print()`, `Com_Warn()`, `Com_Error()`
- Command completion, history

**Usage**:
```c
// Register command
Cmd_Add("mycommand", MyCommand_f, "Description");

// Command function
static void MyCommand_f(void) {
    const char *arg = Cmd_Argv(1);
    Com_Print("You said: %s\n", arg);
}

// Execute from string
Cmd_Execute("mycommand hello");
```

### Console Variables (cvar.c/h)

**Configuration variables**:
```c
typedef struct cvar_s {
    char *name;
    char *string;             // String value
    char *default_string;     // Default value
    int32_t integer;          // Integer value
    float value;              // Float value
    uint32_t flags;           // CVAR_ARCHIVE, CVAR_USER_INFO, etc.
    bool modified;            // Changed since last check
    struct cvar_s *next;
} cvar_t;
```

**Flags**:
- `CVAR_ARCHIVE` - Save to config file
- `CVAR_USER_INFO` - Sent to server (name, skin, etc.)
- `CVAR_SERVER_INFO` - Server broadcasts to clients (hostname, etc.)
- `CVAR_LATCH` - Changes take effect on next map load
- `CVAR_NO_SET` - Cannot be changed by user

**Usage**:
```c
// Create/get cvar
cvar_t *my_var = Cvar_Add("my_setting", "1", CVAR_ARCHIVE, "My setting");

// Read value
if (my_var->integer) {
    // Setting is enabled
}

// Change value
Cvar_Set("my_setting", "0");
```

### Filesystem (filesystem.c/h)

**Virtual filesystem** using PhysicsFS:
- Loads files from directories and .pk3 (zip) archives
- Search path: base directory → mod directory → .pk3 archives
- Write protection (can only write to user directory)

**Key functions**:
```c
// Load entire file into memory
int64_t Fs_Load(const char *filename, void **buffer);

// Open file for reading
file_t *Fs_OpenRead(const char *filename);

// Read from file
int64_t Fs_Read(file_t *file, void *buffer, size_t size, size_t count);

// Check if file exists
bool Fs_Exists(const char *filename);

// Enumerate files
void Fs_Enumerate(const char *path, Fs_EnumerateFunc func, void *data);
```

**Search paths**:
1. `~/quetoo/default/` (user writable)
2. `/usr/local/share/quetoo/default/` (system install)
3. Files in `.pk3` archives (maps, models, sounds)

### Memory Management (mem.c/h)

**Tagged memory allocation**:
```c
// Allocate with tag
void *Mem_Malloc(size_t size, mem_tag_t tag);

// Free specific pointer
void Mem_Free(void *ptr);

// Free all memory with tag
void Mem_FreeTag(mem_tag_t tag);

// Linked allocation (freed with parent)
void *Mem_Link(void *ptr, const void *parent);
```

**Tags** (for subsystem cleanup):
- `MEM_TAG_COMMON` - Common systems
- `MEM_TAG_RENDERER` - Renderer
- `MEM_TAG_COLLISION` - Collision model
- `MEM_TAG_GAME` - Game module
- `MEM_TAG_CGAME` - CGame module
- etc.

**Memory buffers** (mem_buf.c/h):
```c
// Dynamic byte buffer (grows as needed)
typedef struct {
    byte *data;
    size_t size;      // Allocated size
    size_t position;  // Read/write position
} mem_buf_t;

Mem_WriteBuffer(&buf, data, len);  // Append data
Mem_ReadBuffer(&buf, data, len);   // Read data
```

### Threading (thread.c/h)

**Cross-platform threading** (pthreads on Unix, Windows threads):
```c
// Create thread
thread_t *Thread_Create(ThreadRunFunc run, void *data);

// Wait for thread
void Thread_Wait(thread_t *t);

// Mutexes
thread_lock_t *Thread_CreateLock(void);
void Thread_Lock(thread_lock_t *lock);
void Thread_Unlock(thread_lock_t *lock);
```

**Usage**:
- Background loading (maps, models)
- Network I/O
- Sound mixing

### System Interface (sys.c/h)

**Platform-specific functions**:
```c
// Get current time (milliseconds)
uint32_t Sys_Milliseconds(void);

// Sleep
void Sys_Sleep(uint32_t millis);

// Open URL in browser
void Sys_OpenUrl(const char *url);

// Get environment variable
const char *Sys_GetEnv(const char *var);
```

### Image Loading (image.c/h)

**Texture loading** (TGA, PNG, JPG):
```c
typedef struct {
    byte *pixels;       // RGBA data
    uint32_t width;
    uint32_t height;
} SDL_Surface;

SDL_Surface *Img_LoadImage(const char *filename);
void Img_WriteJPEG(const char *filename, byte *data, uint32_t width, uint32_t height);
```

### Atlas (atlas.c/h)

**Texture atlas** (pack small images into one large texture):
```c
atlas_t *Atlas_Create(uint32_t size);  // Create 4096x4096 atlas
atlas_image_t *Atlas_Insert(atlas_t *atlas, const SDL_Surface *surface);
```

**Benefits**:
- Reduce texture switches (performance)
- Better for small UI icons, particles

### RGB9E5 (rgb9e5.c/h)

**Shared exponent color format**:
- HDR color in 32 bits (9 bits per channel + 5 bit exponent)
- Used for HDR textures, lightmaps

## Common Utilities

### Debug Output
```c
Com_Print("Info: %s\n", msg);              // Always printed
Com_Debug(DEBUG_RENDERER, "Render: %s\n", msg);  // Only if debug flag set
Com_Warn("Warning: %s\n", msg);            // Yellow warning
Com_Error(ERROR_DROP, "Error: %s\n", msg); // Disconnect/drop to console
Com_Error(ERROR_FATAL, "Fatal: %s\n", msg);// Exit game
```

### Debug Masks
```c
DEBUG_COLLISION   = (1 << 0),
DEBUG_NET         = (1 << 1),
DEBUG_RENDERER    = (1 << 2),
DEBUG_SERVER      = (1 << 3),
DEBUG_CLIENT      = (1 << 4),
DEBUG_CGAME       = (1 << 5),
// ...
```

Set with: `developer 1` and `debug_mask 4` (renderer debug only)

### Assertions
```c
assert(ptr != NULL);  // Debug builds only
```

## Configuration Files

### config.cfg
Auto-generated, stores cvar values:
```
// This file is automatically generated
set name "Player"
set fov "110"
set sensitivity "3.0"
// ...
```

Loaded at startup, saved on shutdown.

### autoexec.cfg
User-created, runs commands at startup:
```
// My custom binds
bind f "messagemode"
bind tab "+scores"
// Custom settings
set r_max_fps 144
```

## File Formats

### .pk3 Files
- ZIP archives containing game data
- Maps: `maps/*.bsp`
- Models: `models/**/*.md3`
- Textures: `textures/**/*.tga`
- Sounds: `sounds/**/*.wav`

### Search Order
1. Files in filesystem (loose files)
2. Files in `.pk3` archives (most recent first)

Override game files by placing in user directory.

## Initialization Order

1. `Com_Init()` - Initialize common systems
2. Memory, filesystem, console, cvars
3. Parse command line (`+set var value`, `+map dm1`)
4. Load config.cfg
5. Execute autoexec.cfg
6. Initialize subsystems (client, server, etc.)
7. Execute command line commands

## Performance Notes

### Memory
- `Mem_Malloc()` is NOT a simple wrapper - tracks allocations
- Use `Mem_FreeTag()` to free entire subsystems at once
- Link allocations to parent for automatic cleanup

### Filesystem
- `Fs_Load()` caches files in memory
- Repeated loads are fast (cached)
- .pk3 access is slower than loose files (decompression)

### Console
- Command lookup is linear scan (don't add thousands of commands)
- Command completion is expensive (avoid in hot paths)

## Common Patterns

### Subsystem Initialization
```c
void MySystem_Init(void) {
    // Allocate subsystem data
    my_data = Mem_Malloc(sizeof(*my_data), MEM_TAG_MYSYSTEM);
    
    // Register commands
    Cmd_Add("mycommand", MyCommand_f, "Help text");
    
    // Register cvars
    my_cvar = Cvar_Add("my_setting", "1", CVAR_ARCHIVE, "Description");
}

void MySystem_Shutdown(void) {
    // Free all subsystem memory
    Mem_FreeTag(MEM_TAG_MYSYSTEM);
}
```

### Loading Data File
```c
void *buf;
const int64_t len = Fs_Load("data/myfile.dat", &buf);
if (len == -1) {
    Com_Warn("Failed to load myfile.dat\n");
    return;
}

// Parse buf...

Mem_Free(buf);
```

### Enumerating Files
```c
static void EnumerateCallback(const char *filename, void *data) {
    Com_Print("Found: %s\n", filename);
}

Fs_Enumerate("maps/*.bsp", EnumerateCallback, NULL);
```

## Console Commands

### Built-in
```
quit              # Exit game
cmdlist           # List all commands
cvarlist          # List all cvars
bind <key> <cmd>  # Bind key to command
unbind <key>      # Remove key binding
exec <file>       # Execute config file
set <var> <val>   # Set cvar value
```

## Common Issues

### File Not Found
- Check search paths with `path` command
- Ensure .pk3 is in correct directory
- Case-sensitive on Linux/macOS

### Memory Leaks
- Always free allocated memory
- Use `Mem_Link()` for auto-cleanup
- Check `mem_stats` command for leak detection

### Command Not Found
- Command not registered (check `cmdlist`)
- Typo in command name
- Command from unloaded module (game/cgame)
