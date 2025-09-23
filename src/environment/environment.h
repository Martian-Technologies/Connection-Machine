#ifndef environment_h
#define environment_h

#include "backend/backend.h"
#include "computerAPI/circuits/circuitFileManager.h"
#include "computerAPI/fileListener/fileListener.h"
#ifndef CLI
#include "blockRenderDataFeeder.h"
#endif

class Environment {
public:
#ifdef CLI
	Environment() : backend(&circuitFileManager), circuitFileManager(&backend.getCircuitManager()),
					fileListener(std::chrono::milliseconds(200)) {
#else
	Environment() : backend(&circuitFileManager), circuitFileManager(&backend.getCircuitManager()),
					fileListener(std::chrono::milliseconds(200)), blockRenderDataFeeder(&backend) {
#endif
		backend.getBlockDataManager()->initializeDefaults();
	}

	const Backend& getBackend() const { return backend; }
	Backend& getBackend() { return backend; }

	const CircuitFileManager& getCircuitFileManager() const { return circuitFileManager; }
	CircuitFileManager& getCircuitFileManager() { return circuitFileManager; }

	const FileListener& getFileListener() const { return fileListener; }
	FileListener& getFileListener() { return fileListener; }
#ifndef CLI
	const BlockRenderDataFeeder& getBlockRenderDataFeeder() const { return blockRenderDataFeeder; }
	BlockRenderDataFeeder& getBlockRenderDataFeeder() { return blockRenderDataFeeder; }
#endif
private:
	Backend backend;
	CircuitFileManager circuitFileManager;
	FileListener fileListener;
#ifndef CLI
	BlockRenderDataFeeder blockRenderDataFeeder;
#endif
};

#endif /* environment_h */
