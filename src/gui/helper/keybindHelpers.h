#ifndef keybindHelpers_h
#define keybindHelpers_h

#include "imgui/imgui.h"
class Keybind;
struct SDL_Window;

std::set<ImGuiKey> getPressedKeys(SDL_Window& sdlWindow);
std::set<ImGuiKey> getHeldKeys(SDL_Window& sdlWindow);

bool isPressingKeybind(const Keybind& keybinds, const std::set<ImGuiKey>& pressedKeys);

#endif /* keybindHelpers_h */
