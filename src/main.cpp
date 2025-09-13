#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

#include "app.h"
#include "backend/settings/keybind.h"
#include "backend/settings/settings.h"
#include "backend/settings/settingsMap.h"
#include "computerAPI/directoryManager.h"

int main(int argc, char* argv[]) {
	try {
		// Set up directory manager
		DirectoryManager::findDirectories();

		// register settings
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save", Keybind(Keybind::KeyId::KI_S, Keybind::KeyMod::KM_CTRL));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save As", Keybind(Keybind::KeyId::KI_S, Keybind::KeyMod::KM_CTRL | Keybind::KeyMod::KM_SHIFT));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Open", Keybind(Keybind::KeyId::KI_O, Keybind::KeyMod::KM_CTRL));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/New", Keybind(Keybind::KeyId::KI_N, Keybind::KeyMod::KM_CTRL));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Undo", Keybind(Keybind::KeyId::KI_Z, Keybind::KeyMod::KM_CTRL));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Redo", Keybind(Keybind::KeyId::KI_Z, Keybind::KeyMod::KM_CTRL | Keybind::KeyMod::KM_SHIFT));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Copy", Keybind(Keybind::KeyId::KI_C, Keybind::KeyMod::KM_CTRL));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Paste", Keybind(Keybind::KeyId::KI_V, Keybind::KeyMod::KM_CTRL));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/State Changer", Keybind(Keybind::KeyId::KI_I));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Connection", Keybind(Keybind::KeyId::KI_C));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Move", Keybind(Keybind::KeyId::KI_M));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Mode Changer", Keybind(Keybind::KeyId::KI_T));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Placement", Keybind(Keybind::KeyId::KI_P));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Selection Maker", Keybind(Keybind::KeyId::KI_S));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CCW", Keybind(Keybind::KeyId::KI_Q));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CW", Keybind(Keybind::KeyId::KI_E));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Confirm", Keybind(Keybind::KeyId::KI_E));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tool Invert Mode", Keybind(Keybind::KeyId::KI_Q));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Zoom", Keybind(Keybind::KeyId::KI_UNKNOWN, Keybind::KeyMod::KM_SHIFT));
		Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Toggle Fullscreen", Keybind(Keybind::KeyId::KI_F11));
		Settings::registerSetting<SettingType::BOOL>("Keybinds/Camera/Scroll Panning", true);
		Settings::registerSetting<SettingType::UINT>("Simulation/Max Thread Count", std::thread::hardware_concurrency() / 2);

		App app;
		app.runLoop();

	} catch (const std::exception& e) {
		// Top level fatal error catcher, logs issue
		logFatalError("Exiting Connection Machine because of fatal error: '{}'", "", e.what());
		return EXIT_FAILURE;
	}

	logInfo("Exiting Connection Machine...");
	MainRenderer::kill();
	return EXIT_SUCCESS;
}
