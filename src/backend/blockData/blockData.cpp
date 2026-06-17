#include "blockData.h"

BlockData::BlockData(BlockType blockType, DataUpdateEventManager& dataUpdateEventManager) : blockType(blockType), dataUpdateEventManager(dataUpdateEventManager) { }

BlockData::BlockDataCopy BlockData::getBlockDataCopy() const {
	BlockData::BlockDataCopy copy;
	copy.blockType = blockType;
	copy.primitive = primitive;
	copy.placeable = placeable;
	copy.bus = bus;
	copy.name = name;
	copy.path = path;
	copy.blockSize = blockSize;
	copy.inputConnectionCount = inputConnectionCount;
	copy.outputConnectionCount = outputConnectionCount;
	copy.connections = connections;
	copy.virtualConnections = virtualConnections;
	copy.connectionIdNames = connectionIdNames;
	copy.renderData = renderData;
	return copy;
}

void BlockData::sendBlockDataUpdate() { dataUpdateEventManager.sendEvent("blockDataUpdate", blockType); }

void BlockData::setPrimitive(bool primitive) {
	this->primitive = primitive;
	sendBlockDataUpdate();
}

void BlockData::setIsBus(bool bus) {
	this->bus = bus;
	sendBlockDataUpdate();
}

void BlockData::setSize(Size size) {
	if (getSize() == size) return;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, Size>>("preBlockSizeChange", { blockType, size });
	blockSize = size;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, Size>>("postBlockSizeChange", { blockType, getSize() });
	sendBlockDataUpdate();
}

void BlockData::setIsPlaceable(bool placeable) {
	this->placeable = placeable;
	sendBlockDataUpdate();
}

void BlockData::setName(const std::string& name) {
	this->name = name;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, std::string>>("blockNameChange", { blockType, name });
	sendBlockDataUpdate();
}

void BlockData::setPath(const std::string& path) {
	this->path = path;
	sendBlockDataUpdate();
}

connection_end_id_t BlockData::getNewConnectionId() const {
	connection_end_id_t maxId = 0;
	for (auto connection : connections) {
		if (maxId < connection.first)
			maxId = connection.first;
	}
	return maxId.get() + 1;
}

void BlockData::removeConnection(connection_end_id_t connectionId) {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataRemoveConnection", { blockType, connectionId });
	if (iter->second.portType == ConnectionData::PortType::INPUT) {
		inputConnectionCount--;
	} else if (iter->second.portType == ConnectionData::PortType::OUTPUT) {
		outputConnectionCount--;
	}
	connections.erase(iter);
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataRemoveConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}

void BlockData::setConnectionInput(Vector positionOnBlock, connection_end_id_t connectionId) {
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, ConnectionData::PortType>>(
		"preBlockDataSetConnection",
		{ blockType, connectionId, ConnectionData::PortType::INPUT }
	);
	connections.insert_or_assign(connectionId, ConnectionData(positionOnBlock, ConnectionData::PortType::INPUT));
	inputConnectionCount++;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}

void BlockData::setConnectionOutput(Vector positionOnBlock, connection_end_id_t connectionId) {
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, ConnectionData::PortType>>(
		"preBlockDataSetConnection",
		{ blockType, connectionId, ConnectionData::PortType::OUTPUT }
	);
	connections.insert_or_assign(connectionId, ConnectionData(positionOnBlock, ConnectionData::PortType::OUTPUT));
	outputConnectionCount++;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}

void BlockData::setConnectionBidirectional(Vector positionOnBlock, connection_end_id_t connectionId) {
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, ConnectionData::PortType>>(
		"preBlockDataSetConnection",
		{ blockType, connectionId, ConnectionData::PortType::BIDIRECTIONAL }
	);
	connections.insert_or_assign(connectionId, ConnectionData(positionOnBlock, ConnectionData::PortType::BIDIRECTIONAL));
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}

void BlockData::setConnectionIdName(connection_end_id_t connectionId, const std::string& name) {
	connectionIdNames.set(connectionId, name);
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>(
		"blockDataConnectionNameSet",
		std::pair<BlockType, connection_end_id_t>({ blockType, connectionId })
	);
	sendBlockDataUpdate();
}

std::optional<std::string> BlockData::getConnectionIdToName(connection_end_id_t connectionId) const {
	const std::string* str = connectionIdNames.get(connectionId);
	if (str) return *str;
	return std::nullopt;
}

void BlockData::setConnectionPortOffset(connection_end_id_t connectionId, FVector offset) {
	if (!FSize(1).containsVector(offset)) {
		logError("Can't set connection port offset to vector {} that makes it leave its cell", "BlockData", offset);
		return;
	}
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return;
	iter->second.portOffset = offset;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, connectionId });
	sendBlockDataUpdate();
}

void BlockData::setConnectionBitConfiguration(connection_end_id_t connectionId, std::variant<unsigned int, std::vector<unsigned int>> bitConfiguration) {
	if (std::holds_alternative<std::vector<unsigned int>>(bitConfiguration) && std::get<std::vector<unsigned int>>(bitConfiguration).empty()) {
		logError("Cant set the bit configuration of a connection to be empty", "BlockData");
	} else if (std::holds_alternative<unsigned int>(bitConfiguration) && std::get<unsigned int>(bitConfiguration) == 0) {
		logError("Cant set the bit configuration of a connection to be 0", "BlockData");
	}
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return;
	unsigned int bitWidth = std::holds_alternative<unsigned int>(bitConfiguration) ? std::get<unsigned int>(bitConfiguration)
							: std::get<std::vector<unsigned int>>(bitConfiguration).size();
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, unsigned int>>(
		"preBlockDataPortBitConfigurationSet",
		{
			blockType,
			connectionId,
			bitWidth
		}
	);
	iter->second.bitConfiguration = bitConfiguration;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, connection_end_id_t, unsigned int>>("blockDataPortBitConfigurationSet", { blockType, connectionId, bitWidth });
	sendBlockDataUpdate();
}

nlohmann::json BlockData::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json blockJson;
	blockJson["blockType"] = blocktype_to_string(blockType);
	blockJson["primitive"] = primitive;
	blockJson["placeable"] = placeable;
	blockJson["bus"] = bus;
	blockJson["name"] = name;
	blockJson["path"] = path;
	blockJson["blockSize"] = blockSize.toString();
	blockJson["inputConnectionCount"] = inputConnectionCount;
	blockJson["outputConnectionCount"] = outputConnectionCount;
	blockJson["connections"] = nlohmann::json::object();
	for (auto& pair : connections) {
		blockJson["connections"][std::to_string(pair.first.get())] = pair.second.dumpState();
	}
	blockJson["connectionIdNames"] = nlohmann::json::object();
	for (auto& pair : connectionIdNames.getT2Map()) {
		blockJson["connectionIdNames"][std::to_string(pair.first.get())] = pair.second;
	}
	return blockJson;
}

nlohmann::json BlockData::ConnectionData::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json connectionJson;
	connectionJson["positionOnBlock"] = positionOnBlock.toString();
	switch (portType) {
	case PortType::INPUT: connectionJson["portType"] = "INPUT"; break;
	case PortType::OUTPUT: connectionJson["portType"] = "OUTPUT"; break;
	case PortType::BIDIRECTIONAL: connectionJson["portType"] = "BIDIRECTIONAL"; break;
	case PortType::NONE: connectionJson["portType"] = "NONE"; break;
	default: connectionJson["portType"] = "UNKNOWN"; break;
	}
	connectionJson["portOffset"] = portOffset.toString();
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		connectionJson["bitConfiguration"] = std::get<unsigned int>(bitConfiguration);
	} else {
		connectionJson["bitConfiguration"] = std::get<std::vector<unsigned int>>(bitConfiguration);
	}
	return connectionJson;
}

nlohmann::json BlockData::VirtualConnectionData::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json connectionJson;
	connectionJson["bitWidth"] = std::to_string(bitWidth);
	return connectionJson;
}

void BlockData::setVirtualConnection(virtual_connection_id_t virtualConnectionId, unsigned int bitWidth) {
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, virtual_connection_id_t, unsigned int>>("preBlockDataSetVirtualConnection", { blockType, virtualConnectionId, bitWidth });
	virtualConnections.insert_or_assign(virtualConnectionId, VirtualConnectionData(bitWidth));
	dataUpdateEventManager.sendEvent<std::pair<BlockType, virtual_connection_id_t>>("blockDataSetVirtualConnection", { blockType, virtualConnectionId });
	sendBlockDataUpdate();
}

void BlockData::removeVirtualConnection(virtual_connection_id_t virtualConnectionId) {
	auto iter = virtualConnections.find(virtualConnectionId);
	if (iter == virtualConnections.end()) return;
	dataUpdateEventManager.sendEvent<std::pair<BlockType, virtual_connection_id_t>>("preBlockDataRemoveVirtualConnection", { blockType, virtualConnectionId });
	virtualConnections.erase(iter);
	dataUpdateEventManager.sendEvent<std::pair<BlockType, virtual_connection_id_t>>("blockDataRemoveVirtualConnection", { blockType, virtualConnectionId });
	sendBlockDataUpdate();
}

// ------------------------------- Render Block Data -------------------------------

void BlockData::moveRenderData(unsigned int before, unsigned int after) {
	assert(before < renderData.size());
	assert(after < renderData.size());
	if (before == after) return;
	if (before < after) {
		std::rotate(renderData.begin() + before, renderData.begin() + before + 1, renderData.begin() + after);
	} else {
		std::rotate(renderData.begin() + after, renderData.begin() + before, renderData.begin() + before + 1);
	}
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, unsigned int>>("blockDataMoveRenderData", { blockType, before, after });
	sendBlockDataUpdate();
}

void BlockData::removeRenderData(unsigned int index) {
	assert(index < renderData.size());
	renderData.erase(renderData.begin() + index);
	dataUpdateEventManager.sendEvent<std::pair<BlockType, unsigned int>>("blockDatarRemoveRenderData", { blockType, index });
	sendBlockDataUpdate();
}

void BlockData::setBlockTexturePath(unsigned int index, const std::string& texturePath) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).path == texturePath) return;
	std::get<BlockTextureData>(renderData[index]).path = texturePath;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, std::string>>("blockDataTexturePathChange", { blockType, index, texturePath });
	sendBlockDataUpdate();
}

void BlockData::setBlockUseFullTexture(unsigned int index, bool useFullTexture) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).useFullTexture == useFullTexture) return;
	std::get<BlockTextureData>(renderData[index]).useFullTexture = useFullTexture;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, bool>>("blockDataTextureUseFullTextureChange", { blockType, index, useFullTexture });
	sendBlockDataUpdate();
}

void BlockData::setBlockTextureTopLeft(unsigned int index, Vec2Int topLeft) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).topLeft == topLeft) return;
	std::get<BlockTextureData>(renderData[index]).topLeft = topLeft;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, Vec2Int>>("blockDataTextureTopLeftChange", { blockType, index, topLeft });
	sendBlockDataUpdate();
}

void BlockData::setBlockTextureSize(unsigned int index, Vec2Int size) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).size == size) return;
	std::get<BlockTextureData>(renderData[index]).size = size;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, Vec2Int>>("blockDataTextureSizeChange", { blockType, index, size });
	sendBlockDataUpdate();
}

void BlockData::setBlockRenderState(unsigned int index, bool renderState) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).renderState == renderState) return;
	std::get<BlockTextureData>(renderData[index]).renderState = renderState;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, bool>>("blockDataTextureRenderStateChange", { blockType, index, renderState });
	sendBlockDataUpdate();
}

void BlockData::setBlockTextureVirtualConnection(unsigned int index, virtual_connection_id_t virtualConnectionId) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).virtualConnectionId == virtualConnectionId) return;
	std::get<BlockTextureData>(renderData[index]).virtualConnectionId = virtualConnectionId;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, std::optional<virtual_connection_id_t>>>("blockDataTextureVirtualConnectionChange", { blockType, index, virtualConnectionId });
	sendBlockDataUpdate();
}

void BlockData::setBlockTextureStateOffset(unsigned int index, Vec2Int stateOffset) {
	assert(index < renderData.size());
	assert(std::holds_alternative<BlockTextureData>(renderData[index]));
	if (std::get<BlockTextureData>(renderData[index]).stateOffset == stateOffset) return;
	std::get<BlockTextureData>(renderData[index]).stateOffset = stateOffset;
	dataUpdateEventManager.sendEvent<std::tuple<BlockType, unsigned int, Vec2Int>>("blockDataTextureStateOffsetChange", { blockType, index, stateOffset });
	sendBlockDataUpdate();
}


// void BlockData::setTexturePath(const std::string& texturePath) {
// 	if (this->texturePath == texturePath) return; // what is this going to do...
// 	this->texturePath = texturePath;
// 	dataUpdateEventManager.sendEvent<std::pair<BlockType, std::string>>("blockDataTextureChange", { blockType, texturePath });
// 	sendBlockDataUpdate();
// }

// void BlockData::setTextureVirtualConnection(std::optional<virtual_connection_id_t> textureVirtualConnection) {
// 	if (this->textureVirtualConnection == textureVirtualConnection) return;
// 	this->textureVirtualConnection = textureVirtualConnection;
// 	dataUpdateEventManager.sendEvent<std::pair<BlockType, std::optional<virtual_connection_id_t>>>("blockDataTextureVirtualConnectionChange", { blockType, textureVirtualConnection });
// 	sendBlockDataUpdate();
// }

// void BlockData::setUsesTileMapTexture(bool usesTileMapTexture) {
// 	if (this->usesTileMapTexture == usesTileMapTexture) return;
// 	this->usesTileMapTexture = usesTileMapTexture;
// 	dataUpdateEventManager.sendEvent<std::pair<BlockType, bool>>("blockDataUsesTileMapTextureChange", { blockType, usesTileMapTexture });
// 	sendBlockDataUpdate();
// }

// void BlockData::setSmallestTextureCord(Vec2Int smallestTextureCord) {
// 	if (this->smallestTextureCord == smallestTextureCord) return;
// 	this->smallestTextureCord = smallestTextureCord;
// 	dataUpdateEventManager.sendEvent<std::pair<BlockType, Vec2Int>>("blockDataSmallestTextureCordChange", { blockType, smallestTextureCord });
// 	sendBlockDataUpdate();
// }

// void BlockData::setTextureBlockStateOffset(Vec2Int textureBlockStateOffset) {
// 	if (this->textureBlockStateOffset == textureBlockStateOffset) return;
// 	this->textureBlockStateOffset = textureBlockStateOffset;
// 	dataUpdateEventManager.sendEvent<std::pair<BlockType, Vec2Int>>("blockDataTextureBlockStateOffsetChange", { blockType, textureBlockStateOffset });
// 	sendBlockDataUpdate();
// }
