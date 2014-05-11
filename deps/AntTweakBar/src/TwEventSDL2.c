//  ---------------------------------------------------------------------------
//
//  @file       TwEventSDL.c
//  @brief      Helper: 
//              translate and re-send mouse and keyboard events 
//              from SDL event loop to AntTweakBar
//  
//  @author     Philippe Decaudin
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  note:       AntTweakBar.dll does not need to link with SDL, 
//              it just needs some definitions for its helper functions.
//
//  ---------------------------------------------------------------------------

#include <SDL2/SDL.h>
#include <AntTweakBar.h>

typedef struct {
	int width, height;
	int window_width, window_height;
} TwSDLState;

static TwSDLState SDLState;

/*
 * @brief Handle libSDL2 events for AntTweakBar.
 * @param sdlEvent The SDL_Event pointer, cast to (void *).
 * @return 0 if the event was not swallowed, non-zero otherwise.
 */
int TW_CALL TwEventSDL(const void *sdlEvent, unsigned char majorVersion __attribute__((unused)),
		unsigned char minorVersion __attribute__((unused))) {

	const SDL_Event *event = (const SDL_Event *) sdlEvent;

	if (event == NULL)
		return 0;

	int handled = 0;

	switch (event->type) {

		case SDL_TEXTINPUT:
			for (size_t i = 0; i < strlen(event->text.text); i++) {
				handled += TwKeyPressed(event->text.text[i], TW_KMOD_NONE);
			}
			break;

		case SDL_KEYDOWN: {
			int key = 0, mod = 0;

			switch (event->key.keysym.sym) {
				case SDLK_BACKSPACE:
					key = TW_KEY_BACKSPACE;
					break;
				case SDLK_TAB:
					key = TW_KEY_TAB;
					break;
				case SDLK_CLEAR:
					key = TW_KEY_CLEAR;
					break;
				case SDLK_RETURN:
				case SDLK_KP_ENTER:
					key = TW_KEY_RETURN;
					break;
				case SDLK_PAUSE:
					key = TW_KEY_PAUSE;
					break;
				case SDLK_ESCAPE:
					key = TW_KEY_ESCAPE;
					break;
				case SDLK_SPACE:
					key = TW_KEY_SPACE;
					break;
				case SDLK_DELETE:
					key = TW_KEY_DELETE;
					break;
				case SDLK_UP:
					key = TW_KEY_UP;
					break;
				case SDLK_DOWN:
					key = TW_KEY_DOWN;
					break;
				case SDLK_RIGHT:
					key = TW_KEY_RIGHT;
					break;
				case SDLK_LEFT:
					key = TW_KEY_LEFT;
					break;
				case SDLK_INSERT:
					key = TW_KEY_INSERT;
					break;
				case SDLK_HOME:
					key = TW_KEY_HOME;
					break;
				case SDLK_END:
					key = TW_KEY_END;
					break;
				case SDLK_PAGEUP:
					key = TW_KEY_PAGE_UP;
					break;
				case SDLK_PAGEDOWN:
					key = TW_KEY_PAGE_DOWN;
					break;

				case SDLK_F1:
				case SDLK_F2:
				case SDLK_F3:
				case SDLK_F4:
				case SDLK_F5:
				case SDLK_F6:
				case SDLK_F7:
				case SDLK_F8:
				case SDLK_F9:
				case SDLK_F10:
				case SDLK_F11:
				case SDLK_F12:
					key = event->key.keysym.sym - SDLK_F1 + TW_KEY_F1;
					break;

				case SDLK_LCTRL:
				case SDLK_LSHIFT:
				case SDLK_LALT:
				case SDLK_LGUI:
				case SDLK_RCTRL:
				case SDLK_RSHIFT:
				case SDLK_RALT:
				case SDLK_RGUI:
					key = event->key.keysym.sym;
					break;
			}

			switch (event->key.keysym.mod) {
				case KMOD_LSHIFT:
				case KMOD_RSHIFT:
					mod = TW_KMOD_SHIFT;
					break;
				case KMOD_LCTRL:
				case KMOD_RCTRL:
					mod = TW_KMOD_CTRL;
					break;
				case KMOD_LALT:
				case KMOD_RALT:
					mod = TW_KMOD_ALT;
					break;
				case KMOD_LGUI:
				case KMOD_RGUI:
					mod = TW_KMOD_META;
					break;
				default:
					mod = TW_KMOD_NONE;
			}

			if (key != 0)
				handled = TwKeyPressed(key, mod);
		}
			break;

		case SDL_MOUSEMOTION:
			if (SDLState.width && SDLState.height) {
				const int mx = event->motion.x * (SDLState.width / SDLState.window_width);
				const int my = event->motion.y * (SDLState.height / SDLState.window_height);

				handled = TwMouseMotion(mx, my);
			}
			break;

		case SDL_MOUSEBUTTONUP:
			handled = TwMouseButton(TW_MOUSE_RELEASED, (TwMouseButtonID) event->button.button);
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (event->button.button == 4 || event->button.button == 5) {
				static int s_WheelPos = 0;
				if (event->button.button == 4)
					++s_WheelPos;
				else
					--s_WheelPos;
				handled = TwMouseWheel(s_WheelPos);
			} else {
				handled = TwMouseButton(TW_MOUSE_PRESSED, (TwMouseButtonID) event->button.button);
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event->window.event) {

				case SDL_WINDOWEVENT_SHOWN:
				case SDL_WINDOWEVENT_SIZE_CHANGED: {
					SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
					SDL_GL_GetDrawableSize(window, &SDLState.width, &SDLState.height);

					TwWindowSize(SDLState.width, SDLState.height);

					SDL_GetWindowSize(window, &SDLState.window_width, &SDLState.window_height);
				}
					break;
			}
			break;
	}

	return handled;
}
