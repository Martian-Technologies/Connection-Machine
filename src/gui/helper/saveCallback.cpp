#include "computerAPI/circuits/circuitFileManager.h"
#include "gui/mainWindow/mainWindow.h"

void SaveCallback(void* userData, const char* const* filePaths, int filter) {
	std::pair<MainWindow&, std::string>* data = (std::pair<MainWindow&, std::string>*)userData;
	if (filePaths && filePaths[0]) {
		std::string filePath = filePaths[0];
		if (data->first.getEnvironment().getCircuitFileManager().getSavePath(data->second) != nullptr) {
			logWarning("This circuit " + data->second + " will be saved with a new UUID");
			data->first.log("This circuit {} will be saved with a new UUID", data->second);
		}
		data->first.getEnvironment().getCircuitFileManager().saveToFile(filePath, data->second);
	} else {
		logInfo("File dialog canceled.");
	}
	delete data;
}

void SaveCallback_NoDelete(void* userData, const char* const* filePaths, int filter) {
	std::pair<MainWindow&, std::string>* data = (std::pair<MainWindow&, std::string>*)userData;
	if (filePaths && filePaths[0]) {
		std::string filePath = filePaths[0];
		if (data->first.getEnvironment().getCircuitFileManager().getSavePath(data->second) != nullptr) {
			logWarning("This circuit " + data->second + " will be saved with a new UUID");
			data->first.log("This circuit {} will be saved with a new UUID", data->second);
		}
		data->first.getEnvironment().getCircuitFileManager().saveToFile(filePath, data->second);
	} else {
		logInfo("File dialog canceled.");
	}
}
