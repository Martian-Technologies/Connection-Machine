#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

#include "app.h"
#include "cli/cliApp.h"
#include "backend/settings/keybind.h"
#include "backend/settings/settings.h"
#include "backend/settings/settingsMap.h"
#include "computerAPI/directoryManager.h"
#include "computerAPI/saveSettings.h"
#include "util/version.h"

void registerSettings() {
	logInfo("Registering settings", "Main");
	Settings::registerSetting<SettingType::FILE_PATH>("Appearance/Font", (DirectoryManager::getResourceDirectory() / "gui/fonts/monaspace.otf").generic_string());
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save", Keybind(Keybind::KeyId::KI_S, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save As", Keybind(Keybind::KeyId::KI_S, Keybind::KeyMod::KM_CTRL | Keybind::KeyMod::KM_SHIFT));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Open", Keybind(Keybind::KeyId::KI_O, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/New", Keybind(Keybind::KeyId::KI_N, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Start Stop", Keybind(Keybind::KeyId::KI_SPACE));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Step Forward", Keybind(Keybind::KeyId::KI_RIGHT));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Step Back", Keybind(Keybind::KeyId::KI_LEFT));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Skip Forward", Keybind(Keybind::KeyId::KI_RIGHT, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Skip Back", Keybind(Keybind::KeyId::KI_LEFT, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Increase Speed", Keybind(Keybind::KeyId::KI_UP));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Decrease Speed", Keybind(Keybind::KeyId::KI_DOWN));
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
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Cycle Mode", Keybind(Keybind::KeyId::KI_TAB));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Cycle Mode Back", Keybind(Keybind::KeyId::KI_TAB, Keybind::KeyMod::KM_SHIFT));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CCW", Keybind(Keybind::KeyId::KI_Q));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CW", Keybind(Keybind::KeyId::KI_E));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Confirm", Keybind(Keybind::KeyId::KI_E));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tool Invert Mode", Keybind(Keybind::KeyId::KI_Q));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/(DEBUG) Test Circuit", Keybind(Keybind::KeyId::KI_L));
#ifdef __APPLE__
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Toggle Fullscreen", Keybind(Keybind::KeyId::KI_F, Keybind::KeyMod::KM_META | Keybind::KeyMod::KM_SHIFT));
#else
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Toggle Fullscreen", Keybind(Keybind::KeyId::KI_F11));
#endif
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Increase UI Scale", Keybind(Keybind::KeyId::KI_OEM_PLUS, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Decrease UI Scale", Keybind(Keybind::KeyId::KI_OEM_MINUS, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Reset UI Scale", Keybind(Keybind::KeyId::KI_0, Keybind::KeyMod::KM_CTRL));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Tutorial/Start", Keybind(Keybind::KeyId::KI_J));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Tutorial/Stop", Keybind(Keybind::KeyId::KI_K));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Zoom", Keybind(Keybind::KeyId::KI_UNKNOWN, Keybind::KeyMod::KM_SHIFT));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Home", Keybind(Keybind::KeyId::KI_F));
	Settings::registerSetting<SettingType::BOOL>("Keybinds/Camera/Scroll Panning", true);
	Settings::registerSetting<SettingType::BOOL>("Keybinds/Settings/Match Keyboard Layout", true);
	Settings::registerSetting<SettingType::DECIMAL>("Appearance/UI Scale", 1.0);
	Settings::registerSetting<SettingType::UINT>("Simulation/Max Thread Count", std::thread::hardware_concurrency() / 2);
	Settings::registerSetting<SettingType::DECIMAL>("Corner Log/Message Timeout", 3.f);
	SaveSettings save;
	save.load();
	// set font again incase another font was loaded because other fonts wont work for now
	Settings::set<SettingType::FILE_PATH>("Appearance/Font", (DirectoryManager::getResourceDirectory() / "gui/fonts/monaspace.otf").generic_string());

	// std::shared_ptr<Font> font = Freetype::get().loadFont(*Settings::get<SettingType::FILE_PATH>("Appearance/Font"));
	// logInfo(font->getFontFamily());

	// stbi_uc w;
	// stbi_loadf_from_memory(const stbi_uc *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels)

}

int main(int argc, char* argv[]) {
#ifdef MAIN_TRY_CATCH
	try {
#endif
		// if command line args are "--cli" run cli app
		// TODO: proper arg parsing
		if (argc > 1) {
			std::string firstArg = argv[1];
			if (firstArg == "--cli") {
				logInfo("Starting Connection Machine in CLI mode...");
				CliApp cliApp;
				logInfo("Exiting Connection Machine CLI...");
				return EXIT_SUCCESS;
			} else if (firstArg == "--version") {
				logInfo("Connection Machine Version: {}", "", getCurrentVersion().toString());
				return EXIT_SUCCESS;
			}
		}
		// Set up directory manager
		DirectoryManager::findDirectories();
		registerSettings();

		App::get().runLoop();
		App::kill();
#ifdef MAIN_TRY_CATCH
	} catch (const std::exception& e) {
		// Top level fatal error catcher, logs issue
		logFatalError("Exiting Connection Machine because of fatal error: '{}'", "", e.what());
		return EXIT_FAILURE;
	}
#endif

	logInfo("Exiting Connection Machine...");
	return EXIT_SUCCESS;
}
