#include "gui/mainWindow/mainWindow.h"

void callCreatePopup(MainWindow& mainWindow, const std::string& message, const std::vector<std::pair<std::string, std::function<void ()>>>& buttons) {
	mainWindow.createPopup(message, buttons);
}
