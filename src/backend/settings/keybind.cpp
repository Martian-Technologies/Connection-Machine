#include "keybind.h"
// #include "settings.h"
// #include "settingsMap.h"

// #ifdef BUILDING_APP
// #include "gui/rml/scancodeToKeyIdentifier.h"
// #endif

std::map<std::string, ImGuiKey> Keybind::stringToKeyId{};

ImGuiKey Keybind::transformKeyIdForLayout(ImGuiKey key) const {
#ifndef BUILDING_APP
	return key;
#else
    // if (!(*Settings::get<SettingType::BOOL>("Keybinds/Settings/Match Keyboard Layout"))) {
		// return key;
    // }
	return key;
    // Rml::Input::KeyIdentifier ki = static_cast<Rml::Input::KeyIdentifier>(keyid);
    // Rml::Input::KeyIdentifier transformed_ki = RmlSDL::TransformKeyIdentifierForLayout(ki);
    // return static_cast<Keybind>(transformed_ki);
#endif
};

std::string Keybind::toString(bool convertForLayoutForceDisable) const {
	ImGuiKey keyConvert = convertForLayoutForceDisable ? key : transformKeyIdForLayout(key);
	std::string keyString;
	ImGuiKey keyNoMods = (ImGuiKey)(keyConvert & ~ImGuiKey::ImGuiMod_Mask_);
	if (keyNoMods >= ImGuiKey_NamedKey_BEGIN && keyNoMods < ImGuiKey_NamedKey_END)
		keyString += ImGui::GetKeyName(keyNoMods);
	if (keyConvert & ImGuiKey::ImGuiMod_Ctrl) {
		if (!keyString.empty()) keyString += " + ";
		keyString += "Ctrl";
	}
	if (keyConvert & ImGuiKey::ImGuiMod_Alt) {
		if (!keyString.empty()) keyString += " + ";
		keyString += "Alt";
	}
	if (keyConvert & ImGuiKey::ImGuiMod_Shift) {
		if (!keyString.empty()) keyString += " + ";
		keyString += "Shift";
	}
	if (keyConvert & ImGuiKey::ImGuiMod_Super) {
		if (!keyString.empty()) keyString += " + ";
		keyString += "Super";
	}
	return keyString;
}
