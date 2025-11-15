#ifndef circuitBlockData_h
#define circuitBlockData_h

#include "backend/container/block/connectionEnd.h"
#include "util/bidirectionalMultiSecondKeyMap.h"
#include "backend/dataUpdateEventManager.h"
#include "backend/position/position.h"
#include "circuit.h"

class CircuitBlockData {
public:
	CircuitBlockData(circuit_id_t id, DataUpdateEventManager& dataUpdateEventManager) : id(id), dataUpdateEventManager(dataUpdateEventManager) { }
	CircuitBlockData(circuit_id_t id, DataUpdateEventManager& dataUpdateEventManager, const std::string& proceduralCircuitUUID) :
		id(id), dataUpdateEventManager(dataUpdateEventManager), proceduralCircuitUUID(proceduralCircuitUUID) { }
	CircuitBlockData(const CircuitBlockData&) = delete;
    CircuitBlockData& operator=(const CircuitBlockData&) = delete;

	inline void setProceduralCircuitUUID(const std::string& proceduralCircuitUUID) { this->proceduralCircuitUUID.emplace(proceduralCircuitUUID); }
	inline const std::optional<std::string>& getProceduralCircuitUUID() const { return proceduralCircuitUUID; }

	inline void setBlockType(BlockType blockType) { this->blockType = blockType; }
	inline BlockType getBlockType() const { return blockType; }

	inline void removeConnectionIdPosition(connection_end_id_t endId) {
		const Position* posPtr = connectionIdPosition.get(endId);
		if (!posPtr) return;
		Position pos = *posPtr;
		connectionIdPosition.remove(endId);
		dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, Position>>(
			"circuitBlockDataConnectionPositionRemove",
			{ blockType, endId, pos }
		);
	}
	inline void setConnectionIdPosition(connection_end_id_t endId, Position position) {
		connectionIdPosition.set(endId, position);
		dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>(
			"circuitBlockDataConnectionPositionSet",
			{ blockType, endId }
		);
	}
	inline const Position* getConnectionIdToPosition(connection_end_id_t endId) const {
		return connectionIdPosition.get(endId);
	}
	inline const BidirectionalMultiSecondKeyMap<connection_end_id_t, Position>::constIteratorPairT2 getConnectionPositionToId(Position position) const {
		return connectionIdPosition.get(position);
	}
	nlohmann::json dumpState() const {
		nlohmann::json stateJson;
		stateJson["circuitId"] = id;
		stateJson["blockType"] = blocktype_to_string(blockType);
		if (proceduralCircuitUUID.has_value()) {
			stateJson["proceduralCircuitUUID"] = proceduralCircuitUUID.value();
		} else {
			stateJson["proceduralCircuitUUID"] = nullptr;
		}
		nlohmann::json connectionPositionsJson = nlohmann::json::object();
		for (const auto& pair : connectionIdPosition.getT2Map()) {
			connectionPositionsJson[std::to_string(pair.first.get())] = pair.second.toString();
		}
		stateJson["connectionPositions"] = connectionPositionsJson;
		return stateJson;
	}

private:
	BidirectionalMultiSecondKeyMap<connection_end_id_t, Position> connectionIdPosition;
	DataUpdateEventManager& dataUpdateEventManager;
	BlockType blockType;
	circuit_id_t id;
	std::optional<std::string> proceduralCircuitUUID = std::nullopt;

};

#endif /* circuitBlockData_h */