#include "saveSettings.h"
#include "directoryManager.h"
#include "../backend/settings/keybind.h"

SaveSettings::SaveSettings() {
    Settings::registerListener([this](const std::string& key){
        save();
    });
}

void SaveSettings::save() {
    SettingsMap& settings = Settings::getSettingsMap();
    
    std::filesystem::path path = DirectoryManager::getConfigDirectory() / "stored_settings.txt";
    std::ofstream out(path);
    if (!out) {
        logWarning("Failed to open file: {}", "SaveSettings", path.string());
        return;
    }

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
                out << std::quoted(std::to_string(settings.get<SettingType::KEYBIND>(key)->getKeyCombined()));
                break;
            }
            case SettingType::FILE_PATH: {
                out << std::quoted(*settings.get<SettingType::FILE_PATH>(key));
                break;
            }
            case SettingType::VOID:
            default:
                out << "<void>";
                break;
        }

        out << "\n";
    }
}

// NOTE: pass by reference so we actually modify the caller's map
void SaveSettings::load() {
    std::filesystem::path path = DirectoryManager::getConfigDirectory() / "stored_settings.txt";
    std::ifstream in(path);
    if (!in) {
        // First run: no file yet
        return;
    }

    std::string key, eq, value;
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
            case SettingType::STRING:
                Settings::set<SettingType::STRING>(key, value);
                break;
            case SettingType::INT:
                try {
                    Settings::set<SettingType::INT>(key, std::stoi(value));
                } catch (...) {}
                break;
            case SettingType::UINT:
                try {
                    Settings::set<SettingType::UINT>(key, static_cast<unsigned int>(std::stoul(value)));
                } catch (...) {}
                break;
            case SettingType::DECIMAL:
                try {
                    Settings::set<SettingType::DECIMAL>(key, std::stod(value));
                } catch (...) {}
                break;
            case SettingType::BOOL:
                std::string keyCopy = key;
                std::transform(keyCopy.begin(), keyCopy.end(), keyCopy.begin(), ::tolower);
                Settings::set<SettingType::BOOL>(key, keyCopy == "1" || keyCopy == "true" || keyCopy == "yes" || keyCopy == "on");
                break;
            case SettingType::KEYBIND:
                try {
                    Settings::set<SettingType::KEYBIND>(key, Keybind(std::stoi(value)));
                } catch (...) {}
                break;
            case SettingType::FILE_PATH:
                Settings::set<SettingType::FILE_PATH>(key, value);
                break;
            case SettingType::VOID:
            default:
                // Ignore void/unknown types
                break;
        }
    }
}

