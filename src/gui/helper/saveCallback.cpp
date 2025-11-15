#include "computerAPI/circuits/circuitFileManager.h"

void SaveCallback(void* userData, const char* const* filePaths, int filter) {
	std::pair<CircuitFileManager*, std::string>* data = (std::pair<CircuitFileManager*, std::string>*)userData;
	if (filePaths && filePaths[0]) {
		std::string filePath = filePaths[0];
		if (data->first->getSavePath(data->second) != nullptr)
			logWarning("This circuit " + data->second + " will be saved with a new UUID");
		data->first->saveToFile(filePath, data->second);
	} else {
		std::cout << "File dialog canceled." << std::endl;
	}
	delete data;
}
