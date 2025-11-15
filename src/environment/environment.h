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
	Environment(bool loadBlockRenderDataFeeder) :
		backend(circuitFileManager),
		circuitFileManager(backend.getCircuitManager()),
		fileListener(std::chrono::milliseconds(200)) {
#ifndef CLI
		if (loadBlockRenderDataFeeder) {
			blockRenderDataFeeder.emplace(backend);
		}
#endif // CLI
		backend.getBlockDataManager().initializeDefaults();
		logInfo("Environment initialized", "Environment");
	}
	Environment(const Environment&) = delete;
	Environment& operator=(const Environment&) = delete;


	const Backend& getBackend() const { return backend; }
	Backend& getBackend() { return backend; }

	const CircuitFileManager& getCircuitFileManager() const { return circuitFileManager; }
	CircuitFileManager& getCircuitFileManager() { return circuitFileManager; }

	const FileListener& getFileListener() const { return fileListener; }
	FileListener& getFileListener() { return fileListener; }

	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		stateJson["backend"] = backend.dumpState();
		stateJson["circuitFileManager"] = circuitFileManager.dumpState();
		stateJson["fileListener"] = fileListener.dumpState();
		return stateJson;
	}
#ifndef CLI
	const BlockRenderDataFeeder& getBlockRenderDataFeeder() const { return blockRenderDataFeeder.value(); }
	BlockRenderDataFeeder& getBlockRenderDataFeeder() { return blockRenderDataFeeder.value(); }
#endif
private:
	Backend backend;
	CircuitFileManager circuitFileManager;
	FileListener fileListener;
#ifndef CLI
	std::optional<BlockRenderDataFeeder> blockRenderDataFeeder;
#endif
};

#endif /* environment_h */
