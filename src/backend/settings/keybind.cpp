#include "keybind.h"
#include "settings.h"
#include "settingsMap.h"

#ifdef BUILDING_APP
#include "gui/rml/scancodeToKeyIdentifier.h"
#endif

Keybind::KeyId Keybind::transformKeyIdForLayout(Keybind::KeyId keyid) const {
#ifdef BUILDING_APP
    if (!(*Settings::get<SettingType::BOOL>("Keybinds/Settings/Match Keyboard Layout"))) {
#endif
        return keyid;
#ifdef BUILDING_APP
    }
    Rml::Input::KeyIdentifier ki = static_cast<Rml::Input::KeyIdentifier>(keyid);
    Rml::Input::KeyIdentifier transformed_ki = RmlSDL::TransformKeyIdentifierForLayout(ki);
    return static_cast<KeyId>(transformed_ki);
#endif
};