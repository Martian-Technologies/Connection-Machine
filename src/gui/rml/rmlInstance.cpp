#include "rmlInstance.h"

#include <RmlUi/Core.h>

#include "backend/settings/settings.h"
#include "computerAPI/directoryManager.h"

RmlInstance::RmlInstance(RmlSystemInterface* systemInterface, RmlRenderInterface* renderInterface)
	: renderInterface(renderInterface) {
  
	logInfo("Initializing RmlUI...");

	Rml::SetSystemInterface(systemInterface);
	Rml::SetRenderInterface(renderInterface);

	if (!Rml::Initialise()) {
		throwFatalError("Could not initialize RmlUI.");
	}

	Settings::registerSetting<SettingType::FILE_PATH>("Appearance/Font", (DirectoryManager::getResourceDirectory() / "gui/fonts/monaspace.otf").generic_string());

	Rml::LoadFontFace(*Settings::get<SettingType::FILE_PATH>("Appearance/Font"));
}

RmlInstance::~RmlInstance() {
	logInfo("Shutting down RmlUI...");
	renderInterface->setWindowToRenderOn(0);
	Rml::Shutdown();
}
