#include "saveSettings.h"
#include "directoryManager.h"
#include "../backend/settings/keybind.h"

void update_settings(const SettingsMap& settings) {
    std::filesystem::path path = DirectoryManager::getConfigDirectory() / "stored_settings.txt";
    std::ofstream out(path);
    if (!out) {
        logWarning("Failed to open file: {}", "SaveSettings", path.string());
        return;
    }

    for (const auto& pair : settings.getAllKeys()) {
        const std::string& key = pair.first;
        const auto& entry = pair.second;

        out << key << " = ";

        switch (entry->getType()) {
            case SettingType::STRING: {
                if (auto val = settings.get<SettingType::STRING>(key)) out << *val;
                break;
            }
            case SettingType::INT: {
                if (auto val = settings.get<SettingType::INT>(key)) out << *val;
                break;
            }
            case SettingType::UINT: {
                if (auto val = settings.get<SettingType::UINT>(key)) out << *val;
                break;
            }
            case SettingType::DECIMAL: {
                if (auto val = settings.get<SettingType::DECIMAL>(key)) out << *val;
                break;
            }
            case SettingType::BOOL: {
                if (auto val = settings.get<SettingType::BOOL>(key)) out << (*val ? "true" : "false");
                break;
            }
            case SettingType::KEYBIND: {
                if (auto val = settings.get<SettingType::KEYBIND>(key)) out << val->getKeyCombined();
                break;
            }
            case SettingType::FILE_PATH: {
                if (auto val = settings.get<SettingType::FILE_PATH>(key)) out << *val;
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

static inline void trim(std::string& s) {
    auto not_space = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

static inline bool parse_bool(const std::string& v) {
    std::string t; t.reserve(v.size());
    for (unsigned char c : v) t.push_back(static_cast<char>(std::tolower(c)));
    return (t == "1" || t == "true" || t == "yes" || t == "on");
}

// NOTE: pass by reference so we actually modify the caller's map
void setup_settings(SettingsMap& settingsMap) {
    std::filesystem::path path = DirectoryManager::getConfigDirectory() / "stored_settings.txt";
    std::ifstream in(path);
    if (!in) {
        // first run: no file yet = fine
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        trim(key); trim(value);

        const SettingType type = settingsMap.getType(key);
        if (type == SettingType::VOID) {
            // unknown or unregistered key — skip
            continue;
        }

        try {
            switch (type) {
                case SettingType::STRING:
                    settingsMap.set<SettingType::STRING>(key, value);
                    break;

                case SettingType::INT:
                    settingsMap.set<SettingType::INT>(key, std::stoi(value));
                    break;

                case SettingType::UINT:
                    settingsMap.set<SettingType::UINT>(key, static_cast<unsigned int>(std::stoul(value)));
                    break;

                case SettingType::DECIMAL:
                    settingsMap.set<SettingType::DECIMAL>(key, std::stod(value));
                    break;

                case SettingType::BOOL:
                    settingsMap.set<SettingType::BOOL>(key, parse_bool(value));
                    break;

                case SettingType::KEYBIND: {
                    // value is raw keyCombined
                    unsigned int combined = static_cast<unsigned int>(std::stoul(value));
                    settingsMap.set<SettingType::KEYBIND>(key, Keybind(combined));
                    break;
                }

                case SettingType::FILE_PATH:
                    settingsMap.set<SettingType::FILE_PATH>(key, value);
                    break;

                case SettingType::VOID:
                default:
                    break;
            }
        } catch (const std::exception&) {
            // malformed value; skip this line (optionally log)
            // logWarning("Malformed setting line: {}", "SetupSettings", line);
        }
    }
}