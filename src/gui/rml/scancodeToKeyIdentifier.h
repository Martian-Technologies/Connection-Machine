#ifndef scancodeToKeyIdentifier_h
#define scancodeToKeyIdentifier_h

#include <SDL3/SDL_scancode.h>
#include <RmlUi/Core/Input.h>

namespace RmlSDL {

    Rml::Input::KeyIdentifier ScancodeToKeyIdentifier(SDL_Scancode sc);

    SDL_Scancode KeyIdentifierToScancode(Rml::Input::KeyIdentifier ki);

    Rml::Input::KeyIdentifier SDLKeyToKeyIdentifier(int sdlkey);

    Rml::Input::KeyIdentifier ScancodeToKeyIdentifierBasedOnLayout(SDL_Scancode scancode);

    Rml::Input::KeyIdentifier TransformKeyIdentifierForLayout(Rml::Input::KeyIdentifier ki);

} // namespace RmlSDL

#endif /* scancodeToKeyIdentifier_h */
