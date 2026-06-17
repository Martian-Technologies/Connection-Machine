#include "settings.h"

SettingsMap& Settings::getSettingsMap() {
	static SettingsMap settingsMap;
	return settingsMap;
}

std::mutex& Settings::getSettingsMapReadLock() {
	static std::mutex settingsMux;
	return settingsMux;
}
