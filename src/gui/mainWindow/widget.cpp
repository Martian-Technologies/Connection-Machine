#include "widget.h"

#include "mainWindow.h"

Environment& Widget::getEnvironment() const { return mainWindow.getEnvironment(); }
Backend& Widget::getBackend() const { return mainWindow.getEnvironment().getBackend(); }

nlohmann::json Widget::dumpWidgetState() const {
	nlohmann::json json;
	json["name"] = getWidgetIdStr();
	{
		nlohmann::json guiValues;
		std::lock_guard lock(guiValuesMux);
		for (const auto& [key, value] : this->guiValues) {
			guiValues[key] = value->dumpState();
		}
		json["guiValues"] = guiValues;
	}
	json["widgetSubclass"] = dumpState();
	return json;
}
