#ifndef blockData_h
#define blockData_h

#include "backend/container/block/connectionEnd.h"
#include "backend/dataUpdateEventManager.h"
#include "backend/position/position.h"
#include "util/bidirectionalMultiSecondKeyMap.h"
#include "util/vec2.h"

class BlockData {
	friend class BlockDataManager;
public:
	struct ConnectionData {
		enum PortType {
			INPUT,
			OUTPUT,
			BIDIRECTIONAL,
			NONE
		};
		ConnectionData(Vector positionOnBlock, PortType portType, std::variant<unsigned int, std::vector<unsigned int>> bitConfiguration = static_cast<unsigned int>(1)) :
			positionOnBlock(positionOnBlock), portType(portType), bitConfiguration(bitConfiguration) { }
		Vector positionOnBlock = Vector(0, 0);
		PortType portType = PortType::INPUT;
		FVector portOffset = FVector(0.5f, 0.5f);

		/* --------- bitConfiguration --------- */
		std::variant<unsigned int, std::vector<unsigned int>> bitConfiguration;
		inline unsigned int getBitWidth() const noexcept;
		inline unsigned int getFirstLaneId() const noexcept;
		// TODO: improve the performance of the below functions by improving the data structure used to store bitConfiguration
		inline unsigned int getLaneId(unsigned int index) const noexcept;
		inline unsigned int getMaxLaneId() const noexcept;
		unsigned int getMaxLaneIndex() const noexcept { return getBitWidth() - 1; }
		inline bool containsLaneId(unsigned int laneId) const noexcept;
		inline unsigned int getIndexOfLaneId(unsigned int laneId) const noexcept;
		nlohmann::json dumpState() const;
	};

	BlockData(BlockType blockType, DataUpdateEventManager& dataUpdateEventManager);

	inline void sendBlockDataUpdate() { dataUpdateEventManager.sendEvent("blockDataUpdate"); }

	void setDefaultData(bool defaultData) noexcept;
	inline bool isDefaultData() const noexcept { return defaultData; }

	void setPrimitive(bool primitive) noexcept;
	inline bool isPrimitive() const noexcept { return primitive; }

	void setIsBus(bool bus) noexcept;
	inline bool isBus() const noexcept { return bus; }

	void setSize(Size size) noexcept;
	inline Size getSize() const noexcept { return blockSize; }
	inline Size getSize(Orientation orientation) const noexcept { return orientation * blockSize; }

	inline BlockType getBlockType() const { return blockType; }

	void setIsPlaceable(bool placeable) noexcept;
	inline bool isPlaceable() const noexcept { return placeable; }

	void setName(const std::string& name) noexcept;
	inline const std::string& getName() const noexcept { return name; }
	void setPath(const std::string& path) noexcept;
	inline const std::string& getPath() const noexcept { return path; }
	void setTexturePath(const std::string& texturePath) noexcept;
	inline const std::string& getTexturePath() const noexcept { return texturePath; }
	void setTextureTileSize(Vec2Int tileSize) noexcept;
	Vec2Int getTextureTileSize() const noexcept { return textureTileSize; }
	void setTextureSmallestCordTile(Vec2Int smallestCordTile) noexcept;
	Vec2Int getTextureSmallestCordTile() const noexcept { return textureSmallestCordTile; }
	void setTextureBlockTileSize(Vec2Int blockSizeInTiles) noexcept;
	Vec2Int getTextureBlockTileSize() const noexcept { return textureBlockTileSize; }
	void setTextureBlockStateOffset(Vec2Int textureBlockStateOffset) noexcept;
	Vec2Int getTextureBlockStateOffset() const noexcept { return textureBlockStateOffset; }
	void setUsesTileMapTexture(bool usesTileMapTexture) noexcept;
	bool getUsesTileMapTexture() const noexcept { return usesTileMapTexture; }

	void removeConnection(connection_end_id_t connectionId) noexcept;
	// trys to set a input connection in the block.
	void setConnectionInput(Vector positionOnBlock, connection_end_id_t connectionId) noexcept;
	// trys to set a output connection in the block.
	void setConnectionOutput(Vector positionOnBlock, connection_end_id_t connectionId) noexcept;
	// trys to set a bidirectional connection in the block.
	void setConnectionBidirectional(Vector positionOnBlock, connection_end_id_t connectionId) noexcept;

	inline std::optional<connection_end_id_t> getInputConnectionId(Vector positionOnBlock) const noexcept;
	inline std::optional<connection_end_id_t> getOutputConnectionId(Vector positionOnBlock) const noexcept;
	inline std::optional<connection_end_id_t> getInputConnectionId(Vector vector, Orientation orientation) const noexcept;
	inline std::optional<connection_end_id_t> getOutputConnectionId(Vector vector, Orientation orientation) const noexcept;
	inline std::optional<connection_end_id_t> getBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept;
	inline std::optional<connection_end_id_t> getInputOrBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept;
	inline std::optional<connection_end_id_t> getOutputOrBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept;
	inline std::optional<Vector> getConnectionVector(connection_end_id_t connectionId) const noexcept;
	inline std::optional<Vector> getConnectionVector(connection_end_id_t connectionId, Orientation orientation) const noexcept;
	inline connection_end_id_t getConnectionCount() const noexcept;
	inline connection_end_id_t getInputConnectionCount() const noexcept;
	inline connection_end_id_t getOutputConnectionCount() const noexcept;
	inline connection_end_id_t getBidirectionalConnectionCount() const noexcept;
	inline bool connectionExists(connection_end_id_t connectionId) const noexcept;
	inline bool isConnectionInput(connection_end_id_t connectionId) const noexcept;
	inline bool isConnectionOutput(connection_end_id_t connectionId) const noexcept;
	inline bool isConnectionBidirectional(connection_end_id_t connectionId) const noexcept;
	inline bool isConnectionInputOrBidirectional(connection_end_id_t connectionId) const noexcept;
	inline bool isConnectionOutputOrBidirectional(connection_end_id_t connectionId) const noexcept;
	inline ConnectionData::PortType getConnectionPortType(connection_end_id_t connectionId) const noexcept;
	inline const std::unordered_map<connection_end_id_t, ConnectionData>& getConnections() const noexcept;
	inline const ConnectionData* getConnectionData(connection_end_id_t connectionId) const noexcept;
	void setConnectionIdName(connection_end_id_t connectionId, const std::string& name);
	std::optional<std::string> getConnectionIdToName(connection_end_id_t connectionId) const;
	// connection_end_id_t getConnectionNameToId(const std::string& connectionName) const;
	void setConnnectionPortOffset(connection_end_id_t connectionId, FVector offset);
	inline std::optional<FVector> getConnectionPortOffset(connection_end_id_t connectionId) const noexcept;
	inline std::optional<FVector> getConnectionPortOffset(connection_end_id_t connectionId, Orientation orientation) const noexcept;
	inline unsigned int getConnectionBitWidth(connection_end_id_t connectionId) const noexcept;
	void setConnectionBitConfiguration(connection_end_id_t connectionId, std::variant<unsigned int, std::vector<unsigned int>> bitConfiguration) noexcept;
	inline const std::variant<unsigned int, std::vector<unsigned int>>* getConnectionBitConfiguration(connection_end_id_t connectionId) const noexcept;
	inline const std::vector<connection_end_id_t> getPortsWithLaneId(unsigned int laneId) const noexcept;
	inline unsigned int getLaneCount() const noexcept;
	inline bool hasBlockState() const noexcept;
	nlohmann::json dumpState() const;

private:
	BlockType blockType;
	bool defaultData = true;
	bool primitive = true; // true if defined by default (And, Or, Xor...)
	bool placeable = true;
	bool bus = false;
	std::string name = "Unnamed Block";
	std::string path = "Basic";
	std::string texturePath = "";
	bool usesTileMapTexture = false;
	Vec2Int textureTileSize = { 0, 0 }; // mean that the whole texture is 1 tile.
	Vec2Int textureSmallestCordTile = { 0, 0 };
	Vec2Int textureBlockTileSize = { 1, 1 };
	Vec2Int textureBlockStateOffset = { 0, 256 };
	Size blockSize = Size(1);
	unsigned int inputConnectionCount = 0;
	unsigned int outputConnectionCount = 0;
	std::unordered_map<connection_end_id_t, ConnectionData> connections;
	BidirectionalMultiSecondKeyMap<connection_end_id_t, std::string> connectionIdNames;
	DataUpdateEventManager& dataUpdateEventManager;
};

// defaults for good connection pos
constexpr float edgeDistance = 0.48f;
constexpr float sideShift = 0.25f;

/* --------- BlockData::ConnectionData --------- */

inline unsigned int BlockData::ConnectionData::getBitWidth() const noexcept {
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		return std::get<unsigned int>(bitConfiguration);
	} else return std::get<std::vector<unsigned int>>(bitConfiguration).size();
}
inline unsigned int BlockData::ConnectionData::getFirstLaneId() const noexcept {
	if (std::holds_alternative<unsigned int>(bitConfiguration)) return 0;
	else return std::get<std::vector<unsigned int>>(bitConfiguration).at(0);
}
// TODO: improve the performance of the below functions by improving the data structure used to store bitConfiguration
inline unsigned int BlockData::ConnectionData::getLaneId(unsigned int index) const noexcept {
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		assert(index < std::get<unsigned int>(bitConfiguration));
		return index;
	} else return std::get<std::vector<unsigned int>>(bitConfiguration).at(index);
}
inline unsigned int BlockData::ConnectionData::getMaxLaneId() const noexcept {
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		return std::get<unsigned int>(bitConfiguration) - 1;
	} else {
		auto& vec = std::get<std::vector<unsigned int>>(bitConfiguration);
		return *std::max_element(vec.begin(), vec.end());
	}
}
inline bool BlockData::ConnectionData::containsLaneId(unsigned int laneId) const noexcept {
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		return laneId < std::get<unsigned int>(bitConfiguration);
	} else {
		auto& vec = std::get<std::vector<unsigned int>>(bitConfiguration);
		return std::find(vec.begin(), vec.end(), laneId) != vec.end();
	}
}
inline unsigned int BlockData::ConnectionData::getIndexOfLaneId(unsigned int laneId) const noexcept {
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		if (laneId >= std::get<unsigned int>(bitConfiguration)) return std::get<unsigned int>(bitConfiguration);
		return laneId;
	} else {
		auto& vec = std::get<std::vector<unsigned int>>(bitConfiguration);
		auto iter = std::find(vec.begin(), vec.end(), laneId);
		if (iter == vec.end()) return vec.size();
		return std::distance(vec.begin(), iter);
	}
}

/* --------- BlockData --------- */

inline std::optional<connection_end_id_t> BlockData::getInputConnectionId(Vector positionOnBlock) const noexcept {
	if (defaultData) {
		if (positionOnBlock.dx == 0 && positionOnBlock.dy == 0) return connection_end_id_t(0);
		return std::nullopt;
	}
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == positionOnBlock && pair.second.portType == ConnectionData::PortType::INPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getOutputConnectionId(Vector positionOnBlock) const noexcept {
	if (defaultData) {
		if (positionOnBlock.dx == 0 && positionOnBlock.dy == 0) return connection_end_id_t(1);
		return std::nullopt;
	}
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == positionOnBlock && pair.second.portType == ConnectionData::PortType::OUTPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getInputConnectionId(Vector vector, Orientation orientation) const noexcept {
	if (defaultData) {
		if (vector.dx == 0 && vector.dy == 0) return connection_end_id_t(0);
		return std::nullopt;
	}
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && pair.second.portType == ConnectionData::PortType::INPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getOutputConnectionId(Vector vector, Orientation orientation) const noexcept {
	if (defaultData) {
		if (vector.dx == 0 && vector.dy == 0) return connection_end_id_t(1);
		return std::nullopt;
	}
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && pair.second.portType == ConnectionData::PortType::OUTPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept {
	if (defaultData) return std::nullopt;
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && pair.second.portType == ConnectionData::PortType::BIDIRECTIONAL) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getInputOrBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept {
	if (defaultData) {
		if (vector.dx == 0 && vector.dy == 0) return connection_end_id_t(0);
		return std::nullopt;
	}
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && (pair.second.portType == ConnectionData::PortType::INPUT || pair.second.portType == ConnectionData::PortType::BIDIRECTIONAL)) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getOutputOrBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept {
	if (defaultData) {
		if (vector.dx == 0 && vector.dy == 0) return connection_end_id_t(1);
		return std::nullopt;
	}
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && (pair.second.portType == ConnectionData::PortType::OUTPUT || pair.second.portType == ConnectionData::PortType::BIDIRECTIONAL)) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<Vector> BlockData::getConnectionVector(connection_end_id_t connectionId) const noexcept {
	if (defaultData) {
		if (connectionId <= connection_end_id_t(1)) return Vector(0);
		return std::nullopt;
	}
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return std::nullopt;
	return iter->second.positionOnBlock;
}
inline std::optional<Vector> BlockData::getConnectionVector(connection_end_id_t connectionId, Orientation orientation) const noexcept {
	if (defaultData) {
		if (connectionId <= connection_end_id_t(1)) return Vector(0);
		return std::nullopt;
	}
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return std::nullopt;
	return orientation.transformVectorWithArea(iter->second.positionOnBlock, blockSize);
}
inline connection_end_id_t BlockData::getConnectionCount() const noexcept {
	if (defaultData) return connection_end_id_t(2);
	return connection_end_id_t(connections.size());
}
inline connection_end_id_t BlockData::getInputConnectionCount() const noexcept {
	if (defaultData) return connection_end_id_t(1);
	return connection_end_id_t(inputConnectionCount);
}
inline connection_end_id_t BlockData::getOutputConnectionCount() const noexcept {
	if (defaultData) return connection_end_id_t(1);
	return connection_end_id_t(outputConnectionCount);
}
inline connection_end_id_t BlockData::getBidirectionalConnectionCount() const noexcept {
	if (defaultData) return connection_end_id_t(0);
	return connection_end_id_t(connections.size() - inputConnectionCount - outputConnectionCount);
}
inline bool BlockData::connectionExists(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return connectionId <= connection_end_id_t(1);
	return connections.contains(connectionId);
}
inline bool BlockData::isConnectionInput(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return connectionId == connection_end_id_t(0);
	auto iter = connections.find(connectionId);
	return iter != connections.end() && iter->second.portType == ConnectionData::PortType::INPUT;
}
inline bool BlockData::isConnectionOutput(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return connectionId == connection_end_id_t(1);
	auto iter = connections.find(connectionId);
	return iter != connections.end() && iter->second.portType == ConnectionData::PortType::OUTPUT;
}
inline bool BlockData::isConnectionBidirectional(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return false;
	auto iter = connections.find(connectionId);
	return iter != connections.end() && iter->second.portType == ConnectionData::PortType::BIDIRECTIONAL;
}
inline bool BlockData::isConnectionInputOrBidirectional(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return connectionId == connection_end_id_t(0);
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return false;
	return iter->second.portType != ConnectionData::PortType::OUTPUT;
}
inline bool BlockData::isConnectionOutputOrBidirectional(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return connectionId == connection_end_id_t(1);
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return false;
	return iter->second.portType != ConnectionData::PortType::INPUT;
}
inline BlockData::ConnectionData::PortType BlockData::getConnectionPortType(connection_end_id_t connectionId) const noexcept {
	if (defaultData) {
		if (connectionId == connection_end_id_t(0)) return ConnectionData::PortType::INPUT;
		if (connectionId == connection_end_id_t(1)) return ConnectionData::PortType::OUTPUT;
		return ConnectionData::PortType::NONE;
	}
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return ConnectionData::PortType::NONE;
	return iter->second.portType;
}
inline const std::unordered_map<connection_end_id_t, BlockData::ConnectionData>& BlockData::getConnections() const noexcept {
	assert((!defaultData) && "this will be empty if defaultData is true");
	return connections;
}
inline const BlockData::ConnectionData* BlockData::getConnectionData(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return nullptr;
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return nullptr;
	return &iter->second;
}
inline std::optional<FVector> BlockData::getConnectionPortOffset(connection_end_id_t connectionId) const noexcept {
	if (defaultData) {
		if (connectionId == connection_end_id_t(0)) return FVector(0.5f - edgeDistance, 0.5f - sideShift);
		if (connectionId == connection_end_id_t(1)) return FVector(0.5f + edgeDistance, 0.5f + sideShift);
		return std::nullopt;
	}
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return std::nullopt;
	return iter->second.portOffset;
}
inline std::optional<FVector> BlockData::getConnectionPortOffset(connection_end_id_t connectionId, Orientation orientation) const noexcept {
	std::optional<FVector> offset = getConnectionPortOffset(connectionId);
	if (!offset) return std::nullopt;
	return orientation.transformFVectorWithArea(offset.value(), FSize(1));
}
inline unsigned int BlockData::getConnectionBitWidth(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return 1;
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return 0;
	return iter->second.getBitWidth();
}
inline const std::variant<unsigned int, std::vector<unsigned int>>* BlockData::getConnectionBitConfiguration(connection_end_id_t connectionId) const noexcept {
	if (defaultData) return nullptr;
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return nullptr;
	return &iter->second.bitConfiguration;
}
inline const std::vector<connection_end_id_t> BlockData::getPortsWithLaneId(unsigned int laneId) const noexcept {
	assert(isBus() && "Only bus blocks have lane ids");
	std::vector<connection_end_id_t> ports; // TODO: make this not linear search
	for (auto& pair : connections) {
		if (std::holds_alternative<unsigned int>(pair.second.bitConfiguration)) {
			if (laneId < std::get<unsigned int>(pair.second.bitConfiguration)) ports.push_back(pair.first);
		} else {
			for (unsigned int thisLaneId : std::get<std::vector<unsigned int>>(pair.second.bitConfiguration)) {
				if (laneId == thisLaneId) {
					ports.push_back(pair.first);
					break;
				}
			}
		}
	}
	return ports;
}
inline unsigned int BlockData::getLaneCount() const noexcept {
	assert(isBus() && "Only bus blocks have lane counts");
	unsigned int laneCount = 0;
	for (auto& pair : connections) {
		laneCount = std::max(laneCount, pair.second.getMaxLaneId() + 1);
	}
	return laneCount;
}
inline bool BlockData::hasBlockState() const noexcept {
	if (defaultData) return true;
	return (primitive) && !(bus);
}

#endif /* blockData_h */
