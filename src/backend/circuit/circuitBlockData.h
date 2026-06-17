#ifndef circuitBlockData_h
#define circuitBlockData_h

#include "backend/circuit/circuitDefs.h"
#include "backend/container/block/connectionEnd.h"
#include "util/bidirectionalMultiSecondKeyMap.h"
#include "backend/dataUpdateEventManager.h"
#include "backend/position/position.h"

class CircuitBlockData {
public:
	struct CircuitBlockDataCopy {
		BidirectionalMultiSecondKeyMap<connection_end_id_t, Position> connectionIdPosition;
		BlockType blockType;
		circuit_id_t id;
		std::optional<std::string> proceduralCircuitUUID = std::nullopt;
	};

	CircuitBlockDataCopy getCircuitBlockDataCopy() const {
		CircuitBlockDataCopy out;
		out.connectionIdPosition = connectionIdPosition;
		out.blockType = blockType;
		out.id = id;
		out.proceduralCircuitUUID = proceduralCircuitUUID;
		return out;
	}

	CircuitBlockData(circuit_id_t id, DataUpdateEventManager& dataUpdateEventManager) : id(id), dataUpdateEventManager(dataUpdateEventManager) { }
	CircuitBlockData(circuit_id_t id, DataUpdateEventManager& dataUpdateEventManager, const std::string& proceduralCircuitUUID) :
		id(id), dataUpdateEventManager(dataUpdateEventManager), proceduralCircuitUUID(proceduralCircuitUUID) { }
	CircuitBlockData(const CircuitBlockData&) = delete;
    CircuitBlockData& operator=(const CircuitBlockData&) = delete;

	void sendCircuitBlockDataUpdate() { dataUpdateEventManager.sendEvent("circuitBlockDataUpdate", std::make_pair(blockType, id)); }

	inline void setProceduralCircuitUUID(const std::string& proceduralCircuitUUID) {
		this->proceduralCircuitUUID.emplace(proceduralCircuitUUID);
		sendCircuitBlockDataUpdate();
	}
	inline const std::optional<std::string>& getProceduralCircuitUUID() const { return proceduralCircuitUUID; }

	inline void setBlockType(BlockType blockType) {
		this->blockType = blockType;
		sendCircuitBlockDataUpdate();
	}
	inline BlockType getBlockType() const { return blockType; }

	inline void removeConnectionIdPosition(connection_end_id_t endId) {
		const Position* posPtr = connectionIdPosition.get(endId);
		if (!posPtr) return;
		Position position = *posPtr;
		connectionIdPosition.remove(endId);
		dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, Position>>(
			"circuitBlockDataConnectionPositionRemove",
			{ blockType, endId, position }
		);
		sendCircuitBlockDataUpdate();
	}
	inline void setConnectionIdPosition(connection_end_id_t endId, Position position) {
		connectionIdPosition.set(endId, position);
		dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, Position>>(
			"circuitBlockDataConnectionPositionSet",
			{ blockType, endId, position }
		);
		sendCircuitBlockDataUpdate();
	}
	inline const Position* getConnectionIdToPosition(connection_end_id_t endId) const {
		return connectionIdPosition.get(endId);
	}
	inline const BidirectionalMultiSecondKeyMap<connection_end_id_t, Position>::constIteratorPairT2 getConnectionPositionToId(Position position) const {
		return connectionIdPosition.get(position);
	}
	nlohmann::json dumpState() const /* GCOVR_EXCL_FUNCTION */ {
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
	BlockType blockType = BlockType::NONE;
	circuit_id_t id;
	std::optional<std::string> proceduralCircuitUUID = std::nullopt;

};

#endif /* circuitBlockData_h */