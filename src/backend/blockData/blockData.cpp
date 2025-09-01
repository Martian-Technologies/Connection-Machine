#include "blockData.h"

BlockData::BlockData(BlockType blockType, DataUpdateEventManager* dataUpdateEventManager) : blockType(blockType), dataUpdateEventManager(dataUpdateEventManager) {
	dataUpdateEventManager->sendEvent<BlockType>("newBlockType", blockType);
}

void BlockData::setDefaultData(bool defaultData) noexcept {
	if (defaultData == this->defaultData) return;
	bool sentPre = false;
	if (defaultData) {
		for (auto connection : connections) {
			dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataRemoveConnection", { blockType, connection.first });
			dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, connection.first });
		}
		if (getSize() != Size(1)) {
			dataUpdateEventManager->sendEvent<std::pair<BlockType, Size>>("preBlockSizeChange", { blockType, Size(1) });
			sentPre = true;
		}
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, 0 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, 1 });
	}
	this->defaultData = defaultData;
	blockSize = Size(1);

	if (defaultData) {
		if (sentPre) {
			dataUpdateEventManager->sendEvent<std::pair<BlockType, Size>>("postBlockSizeChange", { blockType, Size(1) });
		}
		for (auto connection : connections) {
			dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataRemoveConnection", { blockType, connection.first });
			dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connection.first });
		}
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, 0 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, 1 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataConnectionNameSet", { blockType, 0 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataConnectionNameSet", { blockType, 1 });
		connections.clear();
	}
	sendBlockDataUpdate();
}

void BlockData::setSize(Size size) noexcept {
	if (getSize() == size) return;
	dataUpdateEventManager->sendEvent<std::pair<BlockType, Size>>("preBlockSizeChange", { blockType, size });
	blockSize = size;
	dataUpdateEventManager->sendEvent<std::pair<BlockType, Size>>("postBlockSizeChange", { blockType, getSize() });
	sendBlockDataUpdate();
}

void BlockData::setName(const std::string& name) noexcept {
	this->name = name;
	dataUpdateEventManager->sendEvent<std::pair<BlockType, std::string>>("blockNameChange", { blockType, name });
	sendBlockDataUpdate();
}

// trys to set a connection input in the block. Returns success.
void BlockData::removeConnection(connection_end_id_t connectionId) noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return;
	dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataRemoveConnection", { blockType, connectionId });
	bool isInput = iter->second.second;
	connections.erase(iter);
	inputConnectionCount -= isInput;
	dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataRemoveConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}
void BlockData::setConnectionInput(Vector vector, connection_end_id_t connectionId) noexcept {
	dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, connectionId });
	connections[connectionId] = { vector, true };
	inputConnectionCount++;
	dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}
// trys to set a connection output in the block. Returns success.
void BlockData::setConnectionOutput(Vector vector, connection_end_id_t connectionId) noexcept {
	dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, connectionId });
	connections[connectionId] = { vector, false };
	dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}

void BlockData::setConnectionIdName(connection_end_id_t connectionId, const std::string& name) {
	connectionIdNames.set(connectionId, name);
	dataUpdateEventManager->sendEvent(
		"blockDataConnectionNameSet",
		DataUpdateEventManager::EventDataWithValue<std::pair<BlockType, connection_end_id_t>>({ blockType, connectionId })
	);
}
