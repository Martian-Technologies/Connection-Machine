#ifndef savesettings_h
#define savesettings_h

#include "../backend/settings/keybind.h"
#include "../backend/settings/settings.h"
#include "../backend/settings/settingsMap.h"

class SaveSettings {
public:
    SaveSettings();
    void save();
    void load();
};

#endif