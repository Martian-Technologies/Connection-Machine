/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019-2023 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "rmlSystemInterface.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Element.h>
 // #include "gui/sdl/SDL_gesture.h"

RmlSystemInterface::RmlSystemInterface()
{
	logInfo("Initializing RmlUI System Interface...");
#if SDL_MAJOR_VERSION >= 3
	cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	cursor_move = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
	cursor_pointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
	cursor_resize = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
	cursor_cross = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	cursor_text = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
	cursor_unavailable = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
#else
	cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	cursor_move = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
	cursor_pointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	cursor_resize = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
	cursor_cross = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	cursor_text = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
	cursor_unavailable = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
#endif
}

RmlSystemInterface::~RmlSystemInterface()
{
#if SDL_MAJOR_VERSION >= 3
	auto DestroyCursor = [](SDL_Cursor* cursor) { SDL_DestroyCursor(cursor); };
#else
	auto DestroyCursor = [](SDL_Cursor* cursor) { SDL_FreeCursor(cursor); };
#endif

	DestroyCursor(cursor_default);
	DestroyCursor(cursor_move);
	DestroyCursor(cursor_pointer);
	DestroyCursor(cursor_resize);
	DestroyCursor(cursor_cross);
	DestroyCursor(cursor_text);
	DestroyCursor(cursor_unavailable);
}

void RmlSystemInterface::SetWindow(SDL_Window* in_window)
{
	window = in_window;
}

double RmlSystemInterface::GetElapsedTime()
{
	static const Uint64 start = SDL_GetPerformanceCounter();
	static const double frequency = double(SDL_GetPerformanceFrequency());
	return double(SDL_GetPerformanceCounter() - start) / frequency;
}

void RmlSystemInterface::SetMouseCursor(const Rml::String& cursor_name)
{
	SDL_Cursor* cursor = nullptr;

	if (cursor_name.empty() || cursor_name == "arrow")
		cursor = cursor_default;
	else if (cursor_name == "move")
		cursor = cursor_move;
	else if (cursor_name == "pointer")
		cursor = cursor_pointer;
	else if (cursor_name == "resize")
		cursor = cursor_resize;
	else if (cursor_name == "cross")
		cursor = cursor_cross;
	else if (cursor_name == "text")
		cursor = cursor_text;
	else if (cursor_name == "unavailable")
		cursor = cursor_unavailable;
	else if (Rml::StringUtilities::StartsWith(cursor_name, "rmlui-scroll"))
		cursor = cursor_move;

	if (cursor)
		SDL_SetCursor(cursor);
}

bool RmlSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
	switch (type) {
	case Rml::Log::Type::LT_INFO: {
		logInfo(message, "RmlUi");
		break;
	}
	case Rml::Log::Type::LT_ERROR: {
		logError(message, "RmlUi");
		break;
	}
	case Rml::Log::Type::LT_WARNING: {
		logWarning(message, "RmlUi");
		break;
	}
	default: {
		logInfo(message, "RmlUi - MISC");
	}
	}

	return true;
}

void RmlSystemInterface::SetClipboardText(const Rml::String& text)
{
	SDL_SetClipboardText(text.c_str());
}

void RmlSystemInterface::GetClipboardText(Rml::String& text)
{
	char* raw_text = SDL_GetClipboardText();
	text = Rml::String(raw_text);
	SDL_free(raw_text);
}

void RmlSystemInterface::ActivateKeyboard(Rml::Vector2f caret_position, float line_height)
{
	if (window)
	{
		const SDL_Rect rect = { int(caret_position.x), int(caret_position.y), 1, int(line_height) };
		SDL_SetTextInputArea(window, &rect, 0);
		SDL_StartTextInput(window);
	}
}

void RmlSystemInterface::DeactivateKeyboard()
{
	if (window)
	{
		SDL_StopTextInput(window);
	}
}

bool RmlSDL::InputEventHandler(Rml::Context* context, SDL_Window* window, SDL_Event& ev, float windowScalingSize)
{
#if SDL_MAJOR_VERSION >= 3
#define RMLSDL_WINDOW_EVENTS_BEGIN
#define RMLSDL_WINDOW_EVENTS_END
	auto GetKey = [](const SDL_Event& event) { return event.key.key; };
	auto GetScancode = [](const SDL_Event& event) { return event.key.scancode; };
	constexpr auto event_mouse_motion = SDL_EVENT_MOUSE_MOTION;
	constexpr auto event_mouse_down = SDL_EVENT_MOUSE_BUTTON_DOWN;
	constexpr auto event_mouse_up = SDL_EVENT_MOUSE_BUTTON_UP;
	constexpr auto event_mouse_wheel = SDL_EVENT_MOUSE_WHEEL;
	constexpr auto event_key_down = SDL_EVENT_KEY_DOWN;
	constexpr auto event_key_up = SDL_EVENT_KEY_UP;
	constexpr auto event_text_input = SDL_EVENT_TEXT_INPUT;
	constexpr auto event_window_size_changed = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
	constexpr auto event_window_leave = SDL_EVENT_WINDOW_MOUSE_LEAVE;
	constexpr auto event_drop_file = SDL_EVENT_DROP_FILE;
	constexpr auto event_drop_file_mouse_motion = SDL_EVENT_DROP_POSITION;
	// constexpr auto event_gesture = GESTURE_MULTIGESTURE;
	constexpr auto rmlsdl_true = true;
	constexpr auto rmlsdl_false = false;
#else
	(void)window;
#define RMLSDL_WINDOW_EVENTS_BEGIN \
	case SDL_WINDOWEVENT:              \
	{                                  \
		switch (ev.window.event)       \
		{
#define RMLSDL_WINDOW_EVENTS_END \
		}                            \
		}                            \
		break;
	auto GetKey = [](const SDL_Event& event) { return event.key.keysym.sym; };
	auto GetScancode = [](const SDL_Event& event) { return event.key.keysym.scancode; };
	constexpr auto event_mouse_motion = SDL_MOUSEMOTION;
	constexpr auto event_mouse_down = SDL_MOUSEBUTTONDOWN;
	constexpr auto event_mouse_up = SDL_MOUSEBUTTONUP;
	constexpr auto event_mouse_wheel = SDL_MOUSEWHEEL;
	constexpr auto event_key_down = SDL_KEYDOWN;
	constexpr auto event_key_up = SDL_KEYUP;
	constexpr auto event_text_input = SDL_TEXTINPUT;
	constexpr auto event_window_size_changed = SDL_WINDOWEVENT_SIZE_CHANGED;
	constexpr auto event_window_leave = SDL_WINDOWEVENT_LEAVE;
	constexpr auto rmlsdl_true = SDL_TRUE;
	constexpr auto rmlsdl_false = SDL_FALSE;
#endif

	bool result = true;
	switch (ev.type) {
		// case event_gesture:
		// {
		// 	Gesture_MultiGestureEvent *mgesture = (Gesture_MultiGestureEvent *)&ev;
		// 	//Rotation detected
		// 	if( SDL_fabs( mgesture->dTheta ) > 3.14 / 180.0 )
		// 	{
		// 		// touchLocation.x = mgesture->x * gScreenRect.w;
		// 		// touchLocation.y = mgesture->y * gScreenRect.h;
		// 		// currentTexture = &gRotateTexture;
		// 	}
		// 	//Pinch detected
		// 	else if( SDL_fabs( mgesture->dDist ) > 0.002 ) {
		// 		// logInfo(mgesture->dDist);
		// 		Rml::Dictionary dict;
		// 		dict["delta"] = mgesture->dDist;
		// 		context->GetHoverElement()->DispatchEvent(pinchEventId, dict);
		// 		// mgesture->dDist
		// 	}
		// }
		// break;
	case event_mouse_motion:
	{
		result = context->ProcessMouseMove(int(ev.motion.x * windowScalingSize), int(ev.motion.y * windowScalingSize), GetKeyModifierState());
	}
	break;
	case event_mouse_down:
	{
		result = context->ProcessMouseButtonDown(ConvertMouseButton(ev.button.button), GetKeyModifierState());
		SDL_CaptureMouse(rmlsdl_true);
	}
	break;
	case event_mouse_up:
	{
		SDL_CaptureMouse(rmlsdl_false);
		result = context->ProcessMouseButtonUp(ConvertMouseButton(ev.button.button), GetKeyModifierState());
	}
	break;
	case event_mouse_wheel:
	{
		result = context->ProcessMouseWheel(Rml::Vector2f(-ev.wheel.x * windowScalingSize, -ev.wheel.y * windowScalingSize), GetKeyModifierState());
	}
	break;
	case event_key_down:
	{
		result = context->ProcessKeyDown(ScancodeToKeyIdentifier(GetScancode(ev)), GetKeyModifierState());
		if (GetScancode(ev) == SDL_SCANCODE_RETURN || GetScancode(ev) == SDL_SCANCODE_RETURN2 || GetScancode(ev) == SDL_SCANCODE_KP_ENTER)
			result &= context->ProcessTextInput(Rml::String("\n"));
	}
	break;
	case event_key_up:
	{
		result = context->ProcessKeyUp(ScancodeToKeyIdentifier(GetScancode(ev)), GetKeyModifierState());
	}
	break;
	case event_text_input:
	{
		result = context->ProcessTextInput(Rml::String(&ev.text.text[0]));
	}
	break;

	RMLSDL_WINDOW_EVENTS_BEGIN

	case event_window_size_changed:
	{
		Rml::Vector2i dimensions(ev.window.data1, ev.window.data2);
		context->SetDimensions(dimensions);
	}
	break;
	case event_window_leave:
	{
		context->ProcessMouseLeave();
	}
	break;

#if SDL_MAJOR_VERSION >= 3
	case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
	{
		const float display_scale = SDL_GetWindowDisplayScale(window);
		context->SetDensityIndependentPixelRatio(display_scale);
	}
	break;
	#endif
	case event_drop_file:
	{
		Rml::Element* target = context->GetElementAtPoint(Rml::Vector2f(ev.drop.x * windowScalingSize, ev.drop.y * windowScalingSize));
		if (!target) break;

		const char* filePath = ev.drop.data;
		Rml::Dictionary parameters;
		parameters["file_path"] = filePath;

		result = target->DispatchEvent(RmlSDL::getRmlDropFileEventId(), parameters);
	}
	break;
	case event_drop_file_mouse_motion:
	{
		result = context->ProcessMouseMove(int(ev.drop.x * windowScalingSize), int(ev.drop.y * windowScalingSize), GetKeyModifierState());
	}
	break;

	RMLSDL_WINDOW_EVENTS_END

	default: break;
	}

	return result;
}

int RmlSDL::ConvertMouseButton(int button) {
	switch (button) {
	case SDL_BUTTON_LEFT: return 0;
	case SDL_BUTTON_RIGHT: return 1;
	case SDL_BUTTON_MIDDLE: return 2;
	default: return 3;
	}
}

int RmlSDL::GetKeyModifierState() {
	SDL_Keymod sdl_mods = SDL_GetModState();

	constexpr auto mod_ctrl = SDL_KMOD_CTRL;
	constexpr auto mod_gui = SDL_KMOD_GUI;
	constexpr auto mod_shift = SDL_KMOD_SHIFT;
	constexpr auto mod_alt = SDL_KMOD_ALT;
	constexpr auto mod_num = SDL_KMOD_NUM;
	constexpr auto mod_caps = SDL_KMOD_CAPS;

	int retval = 0;

#ifdef __APPLE__
	if (sdl_mods & mod_ctrl)
		retval |= Rml::Input::KM_META;

	if (sdl_mods & mod_gui)
		retval |= Rml::Input::KM_CTRL;
#else
	if (sdl_mods & mod_ctrl)
		retval |= Rml::Input::KM_CTRL;

	if (sdl_mods & mod_gui)
		retval |= Rml::Input::KM_META;
#endif

	if (sdl_mods & mod_shift)
		retval |= Rml::Input::KM_SHIFT;

	if (sdl_mods & mod_alt)
		retval |= Rml::Input::KM_ALT;

	if (sdl_mods & mod_num)
		retval |= Rml::Input::KM_NUMLOCK;


	if (sdl_mods & mod_caps)
		retval |= Rml::Input::KM_CAPSLOCK;

	return retval;
}

Rml::EventId rmlDropFileEventId = Rml::EventId::Invalid;

Rml::EventId RmlSDL::getRmlDropFileEventId() {
	if (rmlDropFileEventId == Rml::EventId::Invalid)
		rmlDropFileEventId = Rml::RegisterEventType("dropFile", true, true);
	return rmlDropFileEventId;
}