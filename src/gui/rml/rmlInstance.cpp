#include "rmlInstance.h"

#include <RmlUi/Core.h>

#include "backend/settings/settings.h"

RmlInstance::RmlInstance(RmlSystemInterface* systemInterface, RmlRenderInterface* renderInterface)
	: renderInterface(renderInterface) {

	logInfo("Initializing RmlUI...");

	Rml::SetSystemInterface(systemInterface);
	Rml::SetRenderInterface(renderInterface);

	if (!Rml::Initialise()) {
		throwFatalError("Could not initialize RmlUI.");
	}

	Rml::LoadFontFace(*Settings::get<SettingType::FILE_PATH>("Appearance/Font"));
}

RmlInstance::~RmlInstance() {
	logInfo("Shutting down RmlUI...");
	renderInterface->setWindowToRenderOn(0);
	Rml::Shutdown();
}
