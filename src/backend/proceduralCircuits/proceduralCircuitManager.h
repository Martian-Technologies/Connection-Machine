#ifndef proceduralCircuitManager_h
#define proceduralCircuitManager_h

#include "proceduralCircuit.h"
#include "wasmProceduralCircuit.h"

#include "util/uuid.h"

class CircuitFileManager;

class ProceduralCircuitManager {
public:
	ProceduralCircuitManager(CircuitManager& circuitManager, DataUpdateEventManager& dataUpdateEventManager, CircuitFileManager& fileManager);
	ProceduralCircuitManager(const ProceduralCircuitManager&) = delete;
    ProceduralCircuitManager& operator=(const ProceduralCircuitManager&) = delete;

	template<class ProceduralCircuitType>
	const std::string* createProceduralCircuit(const std::string& name, const std::string& uuid = generate_uuid_v4()) {
		std::shared_ptr<ProceduralCircuitType> proceduralCircuit = std::make_shared<ProceduralCircuitType>(circuitManager, dataUpdateEventManager, name, uuid);
		pathToUUID.emplace(proceduralCircuit->getPath(), uuid);
		proceduralCircuits.emplace(uuid, proceduralCircuit);
		dataUpdateEventManager.sendEvent<std::string>("proceduralCircuitPathUpdate", proceduralCircuit->getUUID());
		return &(proceduralCircuit->getUUID());
	}

	const std::string* createWasmProceduralCircuit(wasmtime::Module wasmModule);

	const std::string* getProceduralCircuitUUID(const std::string& path) const;

	SharedProceduralCircuit getProceduralCircuit(const std::string& uuid);
	const SharedProceduralCircuit getProceduralCircuit(const std::string& uuid) const;
	template<class ProceduralCircuitType>
	inline std::shared_ptr<ProceduralCircuitType> getProceduralCircuit(const std::string& uuid) { return std::static_pointer_cast<ProceduralCircuitType>(getProceduralCircuit(uuid)); }
	template<class ProceduralCircuitType>
	inline const std::shared_ptr<ProceduralCircuitType> getProceduralCircuit(const std::string& uuid) const { return std::static_pointer_cast<ProceduralCircuitType>(getProceduralCircuit(uuid)); }

	inline const std::map<std::string, SharedProceduralCircuit>& getProceduralCircuits() const { return proceduralCircuits; }

	nlohmann::json dumpState() const;

private:
	CircuitManager& circuitManager;
	CircuitFileManager& fileManager;
	DataUpdateEventManager& dataUpdateEventManager;

	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	std::map<std::string, SharedProceduralCircuit> proceduralCircuits;
	std::map<std::string, std::string> pathToUUID;
};


template<>
inline const std::string* ProceduralCircuitManager::createProceduralCircuit<WasmProceduralCircuit>(const std::string& name, const std::string& uuid) {
	logError("WasmProceduralCircuit cant be made with createProceduralCircuit. Call createWasmProceduralCircuit() instead.", "ProceduralCircuitManager");
	return nullptr;
}

#endif /* proceduralCircuitManager_h */
