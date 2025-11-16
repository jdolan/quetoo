# SDL2 to SDL3 Migration Summary

This document summarizes the changes made to migrate the Quetoo codebase from SDL2 to SDL3.

## Overview

48 files were modified across the codebase to support SDL3. The migration was performed following the official SDL3 migration guide: https://wiki.libsdl.org/SDL3/README-migration

## Header Changes

All SDL headers now use the `SDL3/` prefix:
- `<SDL.h>` → `<SDL3/SDL.h>`
- `<SDL_video.h>` → `<SDL3/SDL_video.h>`
- `<SDL_audio.h>` → `<SDL3/SDL_audio.h>`
- `<SDL_assert.h>` → `<SDL3/SDL_assert.h>`
- `<SDL_image.h>` → `<SDL3_image/SDL_image.h>` (for SDL_image library)

## Type Renames

### Atomic Types
- `SDL_atomic_t` → `SDL_AtomicInt`

### Thread Types  
- `SDL_mutex` → `SDL_Mutex`
- `SDL_cond` → `SDL_Condition`
- `SDL_threadID` → `SDL_ThreadID`

### I/O Types
- `SDL_RWops` → `SDL_IOStream`

## Function Renames

### Atomic Operations
- `SDL_AtomicAdd()` → `SDL_AddAtomicInt()`
- `SDL_AtomicGet()` → `SDL_GetAtomicInt()`
- `SDL_AtomicSet()` → `SDL_SetAtomicInt()`
- `SDL_AtomicLock()` → `SDL_LockSpinlock()`
- `SDL_AtomicUnlock()` → `SDL_UnlockSpinlock()`

### Condition Variables
- `SDL_CreateCond()` → `SDL_CreateCondition()`
- `SDL_DestroyCond()` → `SDL_DestroyCondition()`
- `SDL_CondWait()` → `SDL_WaitCondition()`
- `SDL_CondSignal()` → `SDL_SignalCondition()`
- `SDL_CondBroadcast()` → `SDL_BroadcastCondition()`

### I/O Operations (SDL_RWops → SDL_IOStream)
- `SDL_RWFromFile()` → `SDL_IOFromFile()`
- `SDL_RWFromConstMem()` → `SDL_IOFromConstMem()`
- `SDL_RWclose()` → `SDL_CloseIO()`
- `SDL_RWread()` → `SDL_ReadIO()`
- `SDL_RWwrite()` → `SDL_WriteIO()`
- `SDL_RWseek()` → `SDL_SeekIO()`
- `SDL_RWtell()` → `SDL_TellIO()`

### Display Functions
- `SDL_GetNumVideoDisplays()` → `SDL_GetDisplays()` (now returns array, takes int* parameter)
- `SDL_GetWindowDisplayIndex()` → `SDL_GetDisplayForWindow()`
- `SDL_GetNumDisplayModes()` → `SDL_GetFullscreenDisplayModes()` (now returns array)

### Window Functions
- `SDL_SetWindowGrab()` → `SDL_SetWindowMouseGrab()`

### Surface Functions
- `SDL_ConvertSurfaceFormat()` → `SDL_ConvertSurface()`
- `SDL_FreeSurface()` → `SDL_DestroySurface()`
- `SDL_FillRect()` → `SDL_FillSurfaceRect()`
- `SDL_BlitScaled()` → `SDL_BlitSurfaceScaled()`
- `SDL_GetClipRect()` → `SDL_GetSurfaceClipRect()`
- `SDL_HasColorKey()` → `SDL_SurfaceHasColorKey()`

### SDL_image Functions
- `IMG_LoadTyped_RW()` → `IMG_LoadTyped_IO()`

## Event System Changes

### Event Type Renames
Window events are now top-level events instead of being nested under SDL_WINDOWEVENT:

- `SDL_WINDOWEVENT` → Individual `SDL_EVENT_WINDOW_*` events
- `SDL_WINDOWEVENT_CLOSE` → `SDL_EVENT_WINDOW_CLOSE_REQUESTED`
- `SDL_WINDOWEVENT_EXPOSED` → `SDL_EVENT_WINDOW_EXPOSED`
- `SDL_WINDOWEVENT_SIZE_CHANGED` → `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`

Keyboard and mouse events:
- `SDL_KEYDOWN` → `SDL_EVENT_KEY_DOWN`
- `SDL_KEYUP` → `SDL_EVENT_KEY_UP`
- `SDL_TEXTINPUT` → `SDL_EVENT_TEXT_INPUT`
- `SDL_MOUSEMOTION` → `SDL_EVENT_MOUSE_MOTION`
- `SDL_MOUSEBUTTONDOWN` → `SDL_EVENT_MOUSE_BUTTON_DOWN`
- `SDL_MOUSEBUTTONUP` → `SDL_EVENT_MOUSE_BUTTON_UP`
- `SDL_MOUSEWHEEL` → `SDL_EVENT_MOUSE_WHEEL`
- `SDL_QUIT` → `SDL_EVENT_QUIT`

### Event Structure Changes

**Key Events:**
The `keysym` field has been removed, with its members promoted to the key event directly:
- `event->key.keysym.scancode` → `event->key.scancode`
- `event->key.keysym.sym` → `event->key.key`
- `event->key.keysym.mod` → `event->key.mod`

**Window Events:**
Window events are no longer nested. Code that checked `event->window.event` must now check `event->type` directly:
```c
// SDL2:
case SDL_WINDOWEVENT:
  if (event->window.event == SDL_WINDOWEVENT_CLOSE) { ... }

// SDL3:
case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
  // Handle directly
```

To check for any window event, use:
```c
case SDL_EVENT_WINDOW_FIRST ... SDL_EVENT_WINDOW_LAST:
```

## Window Flags Changes

- `SDL_WINDOW_FULLSCREEN_DESKTOP` → `SDL_WINDOW_FULLSCREEN`
- `SDL_WINDOW_ALLOW_HIGHDPI` → Removed (always enabled in SDL3)
- `SDL_WINDOW_INPUT_GRABBED` → `SDL_WINDOW_MOUSE_GRABBED`
- `SDL_WINDOW_INPUT_FOCUS` → `SDL_WINDOW_KEYBOARD_FOCUS`

## Pixel Format Changes

- `SDL_PIXELFORMAT_RGBA` → `SDL_PIXELFORMAT_RGBA8888`
- `SDL_PIXELFORMAT_RGBA32` → `SDL_PIXELFORMAT_RGBA8888`
- `SDL_PIXELFORMAT_RGB` → `SDL_PIXELFORMAT_RGB888`

## Display Mode API Changes

### SDL_GetDisplays()
Returns an array instead of a count. Usage:
```c
int num_displays = 0;
SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
if (displays) {
    // Use displays[i] for each display
    SDL_free(displays);
}
```

### SDL_GetFullscreenDisplayModes()
Returns an array of pointers instead of requiring iteration:
```c
int num_modes = 0;
SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display_id, &num_modes);
if (modes) {
    for (int i = 0; i < num_modes; i++) {
        SDL_DisplayMode *mode = modes[i];
        // Use mode->w, mode->h, mode->refresh_rate (now float)
    }
    SDL_free(modes);
}
```

### SDL_GetDesktopDisplayMode()
Now returns a const pointer instead of filling a structure:
```c
const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(display_id);
if (mode) {
    int w = mode->w;
    int h = mode->h;
}
```

## SDL_IOStream API Changes

SDL_IOStream no longer has direct function pointer members. Instead of:
```c
rwops->size(rwops)
rwops->seek(rwops, offset, whence)
rwops->read(rwops, ptr, size, maxnum)
rwops->write(rwops, ptr, size, num)
```

Use the API functions:
```c
SDL_GetIOSize(rwops)
SDL_SeekIO(rwops, offset, whence)
SDL_ReadIO(rwops, ptr, size)
SDL_WriteIO(rwops, ptr, size)
SDL_TellIO(rwops)
```

## Return Value Changes

Many functions that returned negative error codes now return bool:
- `SDL_InitSubSystem()` - Check with `if (!SDL_InitSubSystem(...))` instead of `if (SDL_InitSubSystem(...) < 0)`
- `SDL_SetRelativeMouseMode()` - Returns bool
- `SDL_ShowCursor()` - Returns bool

## Notable Behavioral Changes

1. **Display IDs:** SDL3 uses `SDL_DisplayID` instead of display indices. This is a more stable identifier but may require adjusting code that stored display indices.

2. **Refresh Rate:** Display mode refresh rates are now floats instead of integers.

3. **Window Events:** Window events are now top-level, reducing nesting in event handling code.

4. **High DPI:** Always enabled in SDL3, no need for SDL_WINDOW_ALLOW_HIGHDPI flag.

## Files Modified

Total: 48 files across the following areas:
- Main entry point and system code
- Client renderer and context
- Client input and key handling  
- Sound system
- UI components
- Common libraries (threading, memory, image loading)
- Collision detection
- Tools (quemap)
- Tests

## Testing Recommendations

After migration, thoroughly test:
1. Window creation and display mode selection
2. Event handling (keyboard, mouse, window events)
3. Fullscreen mode switching
4. Multi-display configurations
5. High DPI displays
6. Threading and synchronization
7. File I/O operations
8. Sound system initialization
9. Image loading

## References

- SDL3 Migration Guide: https://wiki.libsdl.org/SDL3/README-migration
- SDL3 API Documentation: https://wiki.libsdl.org/SDL3/
