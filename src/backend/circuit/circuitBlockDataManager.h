#ifndef circuitBlockDataManager_h
#define circuitBlockDataManager_h

#include "circuitBlockData.h"

class CircuitBlockDataManager {
public:
	CircuitBlockDataManager(DataUpdateEventManager* dataUpdateEventManager) : dataUpdateEventManager(dataUpdateEventManager), dataUpdateEventReceiver(dataUpdateEventManager) {
		dataUpdateEventReceiver.linkFunction("blockDataRemoveConnection", [this](const DataUpdateEventManager::EventData* eventData) {
			auto eventWithData = eventData->cast<std::pair<BlockType, connection_end_id_t>>();
			if (!eventWithData) return;
			CircuitBlockData* data = getCircuitBlockData(getCircuitId(eventWithData->get().first));
			if (data) data->removeConnectionIdPosition(eventWithData->get().second);
		});
	}

	void newCircuitBlockData(circuit_id_t circuitId, BlockType blockType) {
		auto pair = circuitBlockData.try_emplace(circuitId, circuitId, dataUpdateEventManager);
		if (pair.second) {
			pair.first->second.setBlockType(blockType);
			blockTypeToCircuitId[blockType] = circuitId;
		} else {
			auto iter = blockTypeToCircuitId.find(blockType);
			if (iter == blockTypeToCircuitId.end()) {
				blockTypeToCircuitId[blockType] = circuitId;
			} else {
				if (iter->second != circuitId) {
					logError("Can not link 2 circuits to the same block type. Circuit id: {} Block type:{}", "CircuitBlockDataManager", circuitId, blockType);
				}
			}
			if (pair.first->second.getProceduralCircuitUUID() != std::nullopt) {
				logError("CircuitBlockData cant lose proceduralCircuitUUID. Circuit id: {} Procedural circuit UUID: \"{}\"", "CircuitBlockDataManager", circuitId, pair.first->second.getProceduralCircuitUUID().value());
			}
		}
	}

	void newCircuitBlockData(circuit_id_t circuitId, BlockType blockType, const std::string& proceduralCircuitUUID) {
		auto pair = circuitBlockData.try_emplace(circuitId, circuitId, dataUpdateEventManager, proceduralCircuitUUID);
		if (pair.second) {
			pair.first->second.setBlockType(blockType);
			blockTypeToCircuitId[blockType] = circuitId;
		} else {
			auto iter = blockTypeToCircuitId.find(blockType);
			if (iter == blockTypeToCircuitId.end()) {
				blockTypeToCircuitId[blockType] = circuitId;
			} else {
				if (iter->second != circuitId) {
					logError("Can not link 2 circuits to the same block type. Circuit id: {} Block type: {}", "CircuitBlockDataManager", circuitId, blockType);
				}
			}
			if (pair.first->second.getProceduralCircuitUUID() == std::nullopt) {
				logError("CircuitBlockData cant gain proceduralCircuitUUID. Circuit id: {} Procedural circuit UUID: \"{}\"", "CircuitBlockDataManager", circuitId, proceduralCircuitUUID);
			} else if (pair.first->second.getProceduralCircuitUUID().value() != proceduralCircuitUUID) {
				logError(
					"CircuitBlockData cant change proceduralCircuitUUID. Current procedural circuit UUID: \"{}\" Attempted procedural circuit UUID: \"{}\"", "CircuitBlockDataManager",
					pair.first->second.getProceduralCircuitUUID().value(),
					proceduralCircuitUUID
				);
			}
		}
	}
	// void removeCircuitBlockData(circuit_id_t circuitId) {
	// 	auto iter = circuitBlockData.find(circuitId);
	// 	if (iter == circuitBlockData.end()) {
	// 		circuitBlockData.erace(iter);
	// 	}
	// 	(circuitBlockData.emplace(std::pair<circuit_id_t, CircuitBlockData>(circuitId, {circuitId, dataUpdateEventManager}))).first->second.setBlockType(blockType);
	// 	blockTypeToCircuitId[blockType] = circuitId;
	// }
	CircuitBlockData* getCircuitBlockData(circuit_id_t circuitId) {
		auto iter = circuitBlockData.find(circuitId);
		if (iter == circuitBlockData.end()) return nullptr;
		return &(iter->second);
	}
	const CircuitBlockData* getCircuitBlockData(circuit_id_t circuitId) const {
		auto iter = circuitBlockData.find(circuitId);
		if (iter == circuitBlockData.end()) return nullptr;
		return &(iter->second);
	}
	circuit_id_t getCircuitId(BlockType blockType) const {
		auto iter = blockTypeToCircuitId.find(blockType);
		if (iter == blockTypeToCircuitId.end()) return 0; // there is never a circuit with id 0
		return iter->second;
	}

private:
	DataUpdateEventManager* dataUpdateEventManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	std::map<BlockType, circuit_id_t> blockTypeToCircuitId;
	std::map<circuit_id_t, CircuitBlockData> circuitBlockData;

};

#endif /* circuitBlockDataManager_h */
