#ifndef circuitManager_h
#define circuitManager_h

#include "backend/proceduralCircuits/proceduralCircuitManager.h"
#include "backend/blockData/blockDataManager.h"
#include "circuitBlockDataManager.h"
#include "util/uuid.h"
#include "circuit.h"

class EvaluatorManager;
class GeneratedCircuit;
class ParsedCircuit;

class CircuitManager {
public:
	CircuitManager(DataUpdateEventManager* dataUpdateEventManager, EvaluatorManager* evaluatorManager, CircuitFileManager* fileManager);
	// Circuit
	inline SharedCircuit getCircuit(circuit_id_t id) {
		auto iter = circuits.find(id);
		if (iter == circuits.end()) return nullptr;
		return iter->second;
	}
	inline const SharedCircuit getCircuit(circuit_id_t id) const {
		auto iter = circuits.find(id);
		if (iter == circuits.end()) return nullptr;
		return iter->second;
	}
	inline SharedCircuit getCircuit(const std::string& uuid) {
		auto iter = UUIDToCircuits.find(uuid);
		if (iter == UUIDToCircuits.end()) return nullptr;
		return iter->second;
	}
	inline const SharedCircuit getCircuit(const std::string& uuid) const {
		auto iter = UUIDToCircuits.find(uuid);
		if (iter == UUIDToCircuits.end()) return nullptr;
		return iter->second;
	}
	inline const std::map<circuit_id_t, SharedCircuit>& getCircuits() const { return circuits; }

	inline circuit_id_t createNewCircuit(bool createEval = true) {
		return createNewCircuit("circuit" + std::to_string(lastId + 1), generate_uuid_v4(), createEval);
	}
	circuit_id_t createNewCircuit(const std::string& name, const std::string& uuid = generate_uuid_v4(), bool createEval = true);
	inline void destroyCircuit(circuit_id_t id) {
		auto iter = circuits.find(id);
		if (iter != circuits.end()) {
			// circuitBlockDataManager.removeCircuitBlockData(id);
			UUIDToCircuits.erase(iter->second->getUUID());
			circuits.erase(iter);
		}
	}

	inline ProceduralCircuitManager* getProceduralCircuitManager() { return &proceduralCircuitManager; }
	inline const ProceduralCircuitManager* getProceduralCircuitManager() const { return &proceduralCircuitManager; }

	// Block Data
	inline BlockDataManager* getBlockDataManager() { return &blockDataManager; }
	inline const BlockDataManager* getBlockDataManager() const { return &blockDataManager; }
	inline CircuitBlockDataManager* getCircuitBlockDataManager() { return &circuitBlockDataManager; }
	inline const CircuitBlockDataManager* getCircuitBlockDataManager() const { return &circuitBlockDataManager; }

	inline BlockType setupBlockData(circuit_id_t circuitId) {
		auto iter = circuits.find(circuitId);
		if (iter == circuits.end()) return BlockType::NONE;
		SharedCircuit circuit = iter->second;
		// Block Data
		BlockType blockType = circuit->getBlockType();
		if (blockType == BlockType::NONE) {
			blockType = blockDataManager.addBlock();
		}
		auto blockData = blockDataManager.getBlockData(blockType);
		if (!blockData) {
			logError("Did not find newly created block data with block type: {}", "CircuitManager", std::to_string(blockType));
			return BlockType::NONE;
		}
		blockData->setDefaultData(false);
		blockData->setPrimitive(false);
		blockData->setPath("Custom");
		blockData->setSize(Size(1));

		// Circuit Block Data
		circuitBlockDataManager.newCircuitBlockData(circuitId, blockType);
		circuit->setBlockType(blockType);

		return blockType;
	}

	inline BlockType setupBlockData(circuit_id_t circuitId, const std::string& proceduralCircuitUUID) {
		auto iter = circuits.find(circuitId);
		if (iter == circuits.end()) return BlockType::NONE;
		SharedCircuit circuit = iter->second;
		// Block Data
		BlockType blockType = circuit->getBlockType();
		if (blockType == BlockType::NONE) {
			blockType = blockDataManager.addBlock();
			auto blockData = blockDataManager.getBlockData(blockType);
			if (!blockData) {
				logError("Did not find newly created block data with block type: {}", "CircuitManager", std::to_string(blockType));
				return BlockType::NONE;
			}
			blockData->setDefaultData(false);
			blockData->setPrimitive(false);
			blockData->setPath("Custom");
			blockData->setSize(Size(1));
		}

		// Circuit Block Data
		CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(circuitId);
		if (!circuitBlockData) {
			circuitBlockDataManager.newCircuitBlockData(circuitId, blockType, proceduralCircuitUUID);
		} else {
			circuitBlockData->setProceduralCircuitUUID(proceduralCircuitUUID);
		}
		circuit->setBlockType(blockType);

		return blockType;
	}

	circuit_id_t createNewCircuit(const ParsedCircuit& parsedCircuit, bool createEval = true);
	circuit_id_t createNewCircuit(const GeneratedCircuit& generatedCircuit, bool createEval = true);
	void updateExistingCircuit(circuit_id_t circuitId, const GeneratedCircuit* generatedCircuit);

	// Iterator
	typedef std::map<circuit_id_t, SharedCircuit>::iterator iterator;
	typedef std::map<circuit_id_t, SharedCircuit>::const_iterator const_iterator;

	inline iterator begin() { return circuits.begin(); }
	inline iterator end() { return circuits.end(); }
	inline const_iterator begin() const { return circuits.begin(); }
	inline const_iterator end() const { return circuits.end(); }
	inline int getCircuitCount() const { return circuits.size(); }

	void connectListener(void* object, CircuitDiffListenerFunction func, unsigned int priority = 100) {
		for (auto& [id, circuit] : circuits) {
			circuit->connectListener(object, func, priority);
		}
		listenerFunctions[object] = {priority, func};
	}
	void disconnectListener(void* object) {
		for (auto& [id, circuit] : circuits) {
			circuit->disconnectListener(object);
		}
	}

	template<class T>
	void linkedFunctionForUpdates(const DataUpdateEventManager::EventData* eventData) {
		auto eventWithData = eventData->cast<std::pair<BlockType, T>>();
		if (!eventWithData) return;
		SharedCircuit circuit = getCircuit(circuitBlockDataManager.getCircuitId(eventWithData->get().first));
		if (!circuit) return;
		circuit->addEdit();
	}

private:
	circuit_id_t getNewCircuitId() { return ++lastId; }

	BlockDataManager blockDataManager;
	CircuitBlockDataManager circuitBlockDataManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
	ProceduralCircuitManager proceduralCircuitManager;
	DataUpdateEventManager* dataUpdateEventManager;
	EvaluatorManager* evaluatorManager;

	circuit_id_t lastId = 0;
	std::map<circuit_id_t, SharedCircuit> circuits;
	std::map<std::string, SharedCircuit> UUIDToCircuits;
	std::map<void*, std::pair<unsigned int, CircuitDiffListenerFunction>> listenerFunctions;
};

#endif /* circuitManager_h */
