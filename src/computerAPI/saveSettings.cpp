#include "saveSettings.h"
#include "../backend/settings/keybind.h"
#include "directoryManager.h"

SaveSettings::SaveSettings() {
	Settings::registerListener([this](const std::string& key) { save(); });
}

void SaveSettings::save() {
	SettingsMap& settings = Settings::getSettingsMap();

	std::filesystem::path path = DirectoryManager::getConfigDirectory() / "stored_settings.txt";
	std::ofstream out(path);
	if (!out) {
		logWarning("Failed to open file: {}", "SaveSettings", path.string());
		return;
	}
	out << "version_1\n";
	for (const auto& pair : settings.getAllKeys()) {
		const std::string& key = pair.first;
		const auto& entry = pair.second;

		out << std::quoted(key) << " = ";

		switch (entry->getType()) {
		case SettingType::STRING: {
			out << std::quoted(*settings.get<SettingType::STRING>(key));
			break;
		}
		case SettingType::INT: {
			out << std::quoted(std::to_string(*settings.get<SettingType::INT>(key)));
			break;
		}
		case SettingType::UINT: {
			out << std::quoted(std::to_string(*settings.get<SettingType::UINT>(key)));
			break;
		}
		case SettingType::DECIMAL: {
			out << std::quoted(std::to_string(*settings.get<SettingType::DECIMAL>(key)));
			break;
		}
		case SettingType::BOOL: {
			out << std::quoted(*settings.get<SettingType::BOOL>(key) ? "true" : "false");
			break;
		}
		case SettingType::KEYBIND: {
			out << std::quoted(settings.get<SettingType::KEYBIND>(key)->toString());
			break;
		}
		case SettingType::FILE_PATH: {
			out << std::quoted(*settings.get<SettingType::FILE_PATH>(key));
			break;
		}
		case SettingType::VOID:
		default: out << "<void>"; break;
		}

		out << "\n";
	}
}

// NOTE: pass by reference so we actually modify the caller's map
void SaveSettings::load() {
	logInfo("Loading settings", "SaveSettings");
	std::filesystem::path path = DirectoryManager::getConfigDirectory() / "stored_settings.txt";
	std::ifstream in(path);
	if (!in) {
		this->save();
		return;
	}

	std::string key, eq, value;
	std::string dummy;
	std::getline(in, dummy);
	while (in >> std::quoted(key) >> eq >> std::quoted(value)) {
		if (eq != "=") {
			logWarning("Malformed settings line in {}", "SaveSettings", path.string());
			continue;
		}

		if (!Settings::hasKey(key)) {
			// Unknown key: skip instead of crashing
			continue;
		}

		switch (Settings::getType(key)) {
		case SettingType::STRING: Settings::set<SettingType::STRING>(key, value); break;
		case SettingType::INT: {
			int number = std::stoi(value);
			try {
				Settings::set<SettingType::INT>(key, number);
			} catch (...) { }
			break;
		}
		case SettingType::UINT: {
			unsigned int number = std::stoul(value);
			try {
				Settings::set<SettingType::UINT>(key, number);
			} catch (...) { }
			break;
		}
		case SettingType::DECIMAL: {
			double number = std::stod(value);
			try {
				Settings::set<SettingType::DECIMAL>(key, number);
			} catch (...) { }
			break;
		}
		case SettingType::BOOL: {
			std::string valueCopy = value;
			std::transform(valueCopy.begin(), valueCopy.end(), valueCopy.begin(), ::tolower);
			Settings::set<SettingType::BOOL>(key, valueCopy == "1" || valueCopy == "true" || valueCopy == "yes" || valueCopy == "on");
			break;
		}
		case SettingType::KEYBIND: try { Settings::set<SettingType::KEYBIND>(key, Keybind(value));
			} catch (...) { }
			break;
		case SettingType::FILE_PATH: Settings::set<SettingType::FILE_PATH>(key, value); break;
		case SettingType::VOID:
		default:
			// Ignore void/unknown types
			break;
		}
	}
}
