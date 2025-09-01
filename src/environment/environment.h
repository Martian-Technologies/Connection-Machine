#ifndef environment_h
#define environment_h

#include "backend/backend.h"
#include "computerAPI/circuits/circuitFileManager.h"
#include "computerAPI/fileListener/fileListener.h"
#include "blockRenderDataFeeder.h"

class Environment {
public:
	Environment() : backend(&circuitFileManager), circuitFileManager(&backend.getCircuitManager()),
					fileListener(std::chrono::milliseconds(200)), blockRenderDataFeeder(&backend) {
		backend.getBlockDataManager()->initializeDefaults();
	}

	const Backend& getBackend() const { return backend; }
	Backend& getBackend() { return backend; }

	const CircuitFileManager& getCircuitFileManager() const { return circuitFileManager; }
	CircuitFileManager& getCircuitFileManager() { return circuitFileManager; }

	const FileListener& getFileListener() const { return fileListener; }
	FileListener& getFileListener() { return fileListener; }
	const BlockRenderDataFeeder& getBlockRenderDataFeeder() const { return blockRenderDataFeeder; }
	BlockRenderDataFeeder& getBlockRenderDataFeeder() { return blockRenderDataFeeder; }

private:
	Backend backend;
	CircuitFileManager circuitFileManager;
	FileListener fileListener;
	BlockRenderDataFeeder blockRenderDataFeeder;

};

#endif /* environment_h */
