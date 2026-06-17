#include "gui/mainWindow/mainWindow.h"
#include <SDL3/SDL_init.h>
#define SDL_MAIN_USE_CALLBACKS 1
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
	Settings::registerSetting<SettingType::FILE_PATH>("Appearance/Font",
		DirectoryManager::shortenPath(DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf")
	);
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save", Keybind(ImGuiKey::ImGuiKey_S | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Save As", Keybind(ImGuiKey::ImGuiKey_S | ImGuiKey::ImGuiMod_Ctrl | ImGuiKey::ImGuiMod_Shift));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/Open", Keybind(ImGuiKey::ImGuiKey_O | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/New", Keybind(ImGuiKey::ImGuiKey_N | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Start Stop", Keybind(ImGuiKey::ImGuiKey_Space));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Step Forward", Keybind(ImGuiKey::ImGuiKey_RightArrow));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Step Back", Keybind(ImGuiKey::ImGuiKey_LeftArrow));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Skip Forward", Keybind(ImGuiKey::ImGuiKey_RightArrow | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Skip Back", Keybind(ImGuiKey::ImGuiKey_LeftArrow | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Increase Speed", Keybind(ImGuiKey::ImGuiKey_UpArrow));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Decrease Speed", Keybind(ImGuiKey::ImGuiKey_DownArrow));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Reset Simulation", Keybind(ImGuiKey::ImGuiKey_R | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Simulation/Transverse Simulation", Keybind(ImGuiKey::ImGuiMod_Shift));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Undo", Keybind(ImGuiKey::ImGuiKey_Z | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Redo", Keybind(ImGuiKey::ImGuiKey_Z | ImGuiKey::ImGuiMod_Ctrl  | ImGuiKey::ImGuiMod_Shift));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Copy", Keybind(ImGuiKey::ImGuiKey_C | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Paste", Keybind(ImGuiKey::ImGuiKey_V | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Pick Block", Keybind(ImGuiKey::ImGuiKey_None | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/State Changer", Keybind(ImGuiKey::ImGuiKey_I));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Connection", Keybind(ImGuiKey::ImGuiKey_C));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Tensor Connect", Keybind(ImGuiKey::ImGuiKey_R));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Move", Keybind(ImGuiKey::ImGuiKey_M));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Mode Changer", Keybind(ImGuiKey::ImGuiKey_T));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Placement", Keybind(ImGuiKey::ImGuiKey_P));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Area Placement", Keybind(ImGuiKey::ImGuiKey_A));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Selection Maker", Keybind(ImGuiKey::ImGuiKey_S));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Cycle Mode", Keybind(ImGuiKey::ImGuiKey_Tab));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tools/Cycle Mode Back", Keybind(ImGuiKey::ImGuiKey_Tab | ImGuiKey::ImGuiMod_Shift));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CCW", Keybind(ImGuiKey::ImGuiKey_Q));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Rotate CW", Keybind(ImGuiKey::ImGuiKey_E));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Flip", Keybind(ImGuiKey::ImGuiKey_W));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Confirm", Keybind(ImGuiKey::ImGuiKey_E));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Editing/Tool Invert Mode", Keybind(ImGuiKey::ImGuiKey_Q));
	// Settings::registerSetting<SettingType::KEYBIND>("Keybinds/File/(DEBUG) Test Circuit", Keybind(ImGuiKey::ImGuiKey_L));
#ifdef __APPLE__
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Toggle Fullscreen", Keybind(ImGuiKey::ImGuiKey_F | ImGuiKey::ImGuiMod_Super  | ImGuiKey::ImGuiMod_Shift));
#else
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Toggle Fullscreen", Keybind(ImGuiKey::ImGuiKey_F11));
#endif
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Increase UI Scale", Keybind(ImGuiKey::ImGuiKey_Equal | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Decrease UI Scale", Keybind(ImGuiKey::ImGuiKey_Minus | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Window/Reset UI Scale", Keybind(ImGuiKey::ImGuiKey_0 | ImGuiKey::ImGuiMod_Ctrl));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Tutorial/Stop", Keybind(ImGuiKey::ImGuiKey_2 | ImGuiKey::ImGuiMod_Alt));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Tutorial/DebugForceCompleteStep", Keybind(ImGuiKey::ImGuiKey_3 | ImGuiKey::ImGuiMod_Alt));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Zoom", Keybind(ImGuiKey::ImGuiKey_None | ImGuiKey::ImGuiMod_Shift));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Home", Keybind(ImGuiKey::ImGuiKey_F));
	Settings::registerSetting<SettingType::KEYBIND>("Keybinds/Camera/Pan", Keybind(ImGuiKey::ImGuiKey_None | ImGuiKey::ImGuiMod_Alt));
	Settings::registerSetting<SettingType::BOOL>("Keybinds/Camera/Scroll Panning", true);
	Settings::registerSetting<SettingType::BOOL>("Keybinds/Settings/Match Keyboard Layout", true);
	Settings::registerSetting<SettingType::BOOL>("Preferences/Editing/Pick Block Copy Orientation", true);
	Settings::registerSetting<SettingType::BOOL>("Preferences/Simulation/Show Confirmation for Reset Simulation", true);
	Settings::registerSetting<SettingType::DECIMAL>("Appearance/UI Scale", 1.0);
	Settings::registerSetting<SettingType::UINT>("Simulation/Max Thread Count", std::thread::hardware_concurrency() / 2);
	Settings::registerSetting<SettingType::DECIMAL>("Appearance/Corner Log/Message Timeout", 3.f);
	SaveSettings save;
	save.load();

	// stbi_uc w;
	// stbi_loadf_from_memory(const stbi_uc *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels)

}

SDL_AppResult SDL_AppInit(void**, int argc, char* argv[]) {
#ifdef MAIN_TRY_CATCH
	try {
#endif
		// Set up directory manager
		DirectoryManager::findDirectories(); // if this fails we wont get logs :(

		// if command line args are "--cli" run cli app
		// TODO: proper arg parsing
		if (argc > 1) {
			std::string firstArg = argv[1];
			if (firstArg == "--cli") {
				logInfo("Starting Connection Machine in CLI mode...");
				CliApp cliApp;
				logInfo("Exiting Connection Machine CLI...");
				return SDL_AppResult::SDL_APP_SUCCESS;
			} else if (firstArg == "--version") {
				logInfo("Connection Machine Version: {}", "", getCurrentVersion().toString());
				return SDL_AppResult::SDL_APP_SUCCESS;
			} else {
				logInfo("Unknown command argument \"{}\". Use '--cli' or '--version'", "", firstArg);
				return SDL_AppResult::SDL_APP_FAILURE;
			}
		}

		// regular app
		registerSettings();

		App::makeWindow<MainWindow>();
		App::init();
		return SDL_AppResult::SDL_APP_CONTINUE;
#ifdef MAIN_TRY_CATCH
	} catch (const std::exception& e) {
		logFatalError("Exiting Connection Machine because of fatal error: '{}'", "", e.what());
		return SDL_AppResult::SDL_APP_FAILURE;
	}
#endif
}

SDL_AppResult SDL_AppEvent(void* s, SDL_Event* event) {
#ifdef MAIN_TRY_CATCH
	try {
#endif
		App::handleEvent(*event);
		return SDL_AppResult::SDL_APP_CONTINUE;
#ifdef MAIN_TRY_CATCH
	} catch (const std::exception& e) {
		logFatalError("Exiting Connection Machine because of fatal error: '{}'", "", e.what());
		return SDL_AppResult::SDL_APP_FAILURE;
	}
#endif
}

SDL_AppResult SDL_AppIterate(void*) {
#ifdef MAIN_TRY_CATCH
	try {
#endif
		return App::iterate();
#ifdef MAIN_TRY_CATCH
	} catch (const std::exception& e) {
		logFatalError("Exiting Connection Machine because of fatal error: '{}'", "", e.what());
		return SDL_AppResult::SDL_APP_FAILURE;
	}
#endif
}

void SDL_AppQuit(void* s, SDL_AppResult result) {
#ifdef MAIN_TRY_CATCH
	try {
#endif
		App::kill();
#ifdef MAIN_TRY_CATCH
	} catch (const std::exception& e) {
		logFatalError("Exiting Connection Machine because of fatal error: '{}'", "", e.what());
	}
#endif
}
