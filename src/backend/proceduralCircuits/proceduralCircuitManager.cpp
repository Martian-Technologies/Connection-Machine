#include "proceduralCircuitManager.h"

ProceduralCircuitManager::ProceduralCircuitManager(CircuitManager& circuitManager, DataUpdateEventManager& dataUpdateEventManager, CircuitFileManager& fileManager) :
	circuitManager(circuitManager), dataUpdateEventManager(dataUpdateEventManager), dataUpdateEventReceiver(dataUpdateEventManager), fileManager(fileManager) {
	dataUpdateEventReceiver.linkFunction("proceduralCircuitPathUpdate", [this](const DataUpdateEventManager::EventData* eventData) {
		auto data = eventData->cast<std::string>();
		if (data) {
			SharedProceduralCircuit proceduralCircuit = getProceduralCircuit(data->get());
			for (auto iter = pathToUUID.begin(); iter != pathToUUID.end(); ++iter) {
				if (iter->second == proceduralCircuit->getUUID()) {
					pathToUUID.erase(iter);
					pathToUUID.emplace(proceduralCircuit->getPath(), proceduralCircuit->getUUID());
					return;
				}
			}
		}
	});
}

const std::string* ProceduralCircuitManager::createWasmProceduralCircuit(wasmtime::Module wasmModule) {
	WasmProceduralCircuit::WasmInstance wasmInstance(wasmModule, circuitManager, fileManager);
	if (!wasmInstance.isValid()) {
		logError("createWasmProceduralCircuit failed because wasmInstance was not valid.", "ProceduralCircuitManager");
		return nullptr;
	}
	SharedProceduralCircuit proceduralCircuit = getProceduralCircuit(wasmInstance.getUUID());
	if (proceduralCircuit) {
		SharedWasmProceduralCircuit wasmProceduralCircuit = std::static_pointer_cast<WasmProceduralCircuit>(proceduralCircuit);
		if (wasmProceduralCircuit) {
			logInfo("WasmProceduralCircuit with UUID {} already exists. Update wasm in WasmProceduralCircuit", "ProceduralCircuitManager", wasmInstance.getUUID());
			wasmProceduralCircuit->setWasm(std::move(wasmInstance));
			return &(wasmProceduralCircuit->getUUID());
		} else {
			logError("Non Wasm ProceduralCircuit with UUID {} already exists. Can't create WasmProceduralCircuit.", "ProceduralCircuitManager", wasmInstance.getUUID());
		}
	} else {
		SharedWasmProceduralCircuit wasmProceduralCircuit = std::make_shared<WasmProceduralCircuit>(circuitManager, dataUpdateEventManager, std::move(wasmInstance));
		pathToUUID.emplace(wasmProceduralCircuit->getPath(), wasmProceduralCircuit->getUUID());
		proceduralCircuits.emplace(wasmProceduralCircuit->getUUID(), wasmProceduralCircuit);
		dataUpdateEventManager.sendEvent<std::string>("proceduralCircuitPathUpdate", wasmProceduralCircuit->getUUID());
		return &(wasmProceduralCircuit->getUUID());
	}
	return nullptr;
}

const std::string* ProceduralCircuitManager::getProceduralCircuitUUID(const std::string& path) const {
	auto iter = pathToUUID.find(path);
	return (iter == pathToUUID.end()) ? nullptr : &(iter->second);
}

SharedProceduralCircuit ProceduralCircuitManager::getProceduralCircuit(const std::string& uuid) {
	auto iter = proceduralCircuits.find(uuid);
	if (iter == proceduralCircuits.end()) return nullptr;
	return iter->second;
}

const SharedProceduralCircuit ProceduralCircuitManager::getProceduralCircuit(const std::string& uuid) const {
	auto iter = proceduralCircuits.find(uuid);
	if (iter == proceduralCircuits.end()) return nullptr;
	return iter->second;
}

nlohmann::json ProceduralCircuitManager::dumpState() const {
	nlohmann::json stateJson;
	stateJson["pathToUUID"] = pathToUUID;
	stateJson["proceduralCircuits"] = nlohmann::json::object();
	for (const auto& pair : proceduralCircuits) {
		stateJson["proceduralCircuits"][pair.first] = pair.second->dumpState();
	}
	return stateJson;
}
