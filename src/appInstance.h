#ifndef appInstance_h
#define appInstance_h

#include "backend/backend.h"
#include "computerAPI/circuits/circuitFileManager.h"
#include "computerAPI/fileListener/fileListener.h"

class AppInstance {
public:
	AppInstance() : backend(&circuitFileManager), circuitFileManager(&backend.getCircuitManager()), fileListener(std::chrono::milliseconds(200)) { }

	const Backend& getBackend() const { return backend; }
	Backend& getBackend() { return backend; }

	const CircuitFileManager& getCircuitFileManager() const { return circuitFileManager; }
	CircuitFileManager& getCircuitFileManager() { return circuitFileManager; }

	const FileListener& getFileListener() const { return fileListener; }
	FileListener& getFileListener() { return fileListener; }

private:
	Backend backend;
	CircuitFileManager circuitFileManager;
	FileListener fileListener;

};

#endif /* appInstance_h */
