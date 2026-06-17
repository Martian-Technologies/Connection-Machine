#ifndef blockData_h
#define blockData_h

#include "backend/container/block/connectionEnd.h"
#include "backend/dataUpdateEventManager.h"
#include "backend/position/position.h"
#include "util/bidirectionalMultiSecondKeyMap.h"
#include "util/vec2.h"

DECLARE_ID_TYPE(virtual_connection_id_t, unsigned int);

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
		// TODO: improve the performance of the below functions by improving the data structure used to store bitConfiguration (its prob fine)
		inline unsigned int getLaneId(unsigned int index) const noexcept;
		inline unsigned int getMaxLaneId() const noexcept;
		unsigned int getMaxLaneIndex() const noexcept { return getBitWidth() - 1; }
		inline bool containsLaneId(unsigned int laneId) const noexcept;
		inline unsigned int getIndexOfLaneId(unsigned int laneId) const noexcept;
		nlohmann::json dumpState() const;
	};

	struct VirtualConnectionData {
		VirtualConnectionData(unsigned int bitWidth = 1) : bitWidth(bitWidth) { }
		unsigned int bitWidth;
		nlohmann::json dumpState() const;
	};

	struct BlockTextureData {
		std::string path;
		bool useFullTexture = true;
		Vec2Int topLeft = { 0, 0 };
		Vec2Int size = { 0, 0 };
		bool renderState = false;
		virtual_connection_id_t virtualConnectionId = 0;
		Vec2Int stateOffset = { 0, 256 };
	};

	typedef std::variant<BlockTextureData> RenderDataType;

	struct BlockDataCopy {
		BlockType blockType;
		bool primitive;
		bool placeable;
		bool bus;
		std::string name;
		std::string path;
		Size blockSize;
		unsigned int inputConnectionCount;
		unsigned int outputConnectionCount;
		std::unordered_map<connection_end_id_t, ConnectionData> connections;
		std::unordered_map<virtual_connection_id_t, VirtualConnectionData> virtualConnections;
		BidirectionalMultiSecondKeyMap<connection_end_id_t, std::string> connectionIdNames;
		std::vector<RenderDataType>	renderData;
	};

	BlockData(BlockType blockType, DataUpdateEventManager& dataUpdateEventManager);
	BlockDataCopy getBlockDataCopy() const;

	void sendBlockDataUpdate();

	void setPrimitive(bool primitive);
	inline bool isPrimitive() const noexcept { return primitive; }

	void setIsBus(bool bus);
	inline bool isBus() const noexcept { return bus; }

	void setSize(Size size);
	inline Size getSize() const noexcept { return blockSize; }
	inline Size getSize(Orientation orientation) const noexcept { return orientation * blockSize; }

	inline BlockType getBlockType() const { return blockType; }

	void setIsPlaceable(bool placeable);
	inline bool isPlaceable() const noexcept { return placeable; }

	void setName(const std::string& name);
	inline const std::string& getName() const noexcept { return name; }
	void setPath(const std::string& path);
	inline const std::string& getPath() const noexcept { return path; }

	// just gives what a id a new connection port should use
	connection_end_id_t getNewConnectionId() const;
	void removeConnection(connection_end_id_t connectionId);
	// trys to set a input connection in the block.
	void setConnectionInput(Vector positionOnBlock, connection_end_id_t connectionId);
	// trys to set a output connection in the block.
	void setConnectionOutput(Vector positionOnBlock, connection_end_id_t connectionId);
	// trys to set a bidirectional connection in the block.
	void setConnectionBidirectional(Vector positionOnBlock, connection_end_id_t connectionId);

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
	inline const std::unordered_map<connection_end_id_t, ConnectionData> getConnectionsSafe() const noexcept;
	inline const ConnectionData* getConnectionData(connection_end_id_t connectionId) const noexcept;
	void setConnectionIdName(connection_end_id_t connectionId, const std::string& name);
	std::optional<std::string> getConnectionIdToName(connection_end_id_t connectionId) const;
	// connection_end_id_t getConnectionNameToId(const std::string& connectionName) const;
	void setConnectionPortOffset(connection_end_id_t connectionId, FVector offset);
	inline std::optional<FVector> getConnectionPortOffset(connection_end_id_t connectionId) const noexcept;
	inline std::optional<FVector> getConnectionPortOffset(connection_end_id_t connectionId, Orientation orientation) const noexcept;
	inline unsigned int getConnectionBitWidth(connection_end_id_t connectionId) const noexcept;
	void setConnectionBitConfiguration(connection_end_id_t connectionId, std::variant<unsigned int, std::vector<unsigned int>> bitConfiguration);
	inline const std::variant<unsigned int, std::vector<unsigned int>>* getConnectionBitConfiguration(connection_end_id_t connectionId) const noexcept;
	inline const std::vector<connection_end_id_t> getPortsWithLaneId(unsigned int laneId) const noexcept;
	inline unsigned int getLaneCount() const noexcept;
	inline bool hasBlockState() const noexcept;
	nlohmann::json dumpState() const;

	inline virtual_connection_id_t getVirtualConnectionCount() const noexcept;
	inline const std::unordered_map<virtual_connection_id_t, VirtualConnectionData>& getVirtualConnections() const noexcept;
	inline const std::unordered_map<virtual_connection_id_t, VirtualConnectionData> getVirtualConnectionsSafe() const noexcept;
	void setVirtualConnection(virtual_connection_id_t virtualConnectionId, unsigned int bitWidth);
	void removeVirtualConnection(virtual_connection_id_t virtualConnectionId);
	inline const VirtualConnectionData* getVirtualConnectionData(virtual_connection_id_t virtualConnectionId) const noexcept;
	inline unsigned int getVirtualConnectionBitWidth(virtual_connection_id_t virtualConnectionId) const noexcept;

	// render data
	template <class T>
	void newRenderData(unsigned int index = std::numeric_limits<unsigned int>::max());
	void moveRenderData(unsigned int before, unsigned int after);
	void removeRenderData(unsigned int index);
	unsigned int getRenderDataSize() const noexcept { return renderData.size(); }
	const std::vector<RenderDataType>& getRenderData() const noexcept { return renderData; }
	const RenderDataType* getRenderData(unsigned int index) const noexcept;
	template<class T>
	const bool isRenderDataOfType(unsigned int index) const noexcept;

	// BlockTextureData
	void setBlockTexturePath(unsigned int index, const std::string& texturePath);
	void setBlockUseFullTexture(unsigned int index, bool useFullTexture);
	void setBlockTextureTopLeft(unsigned int index, Vec2Int topLeft);
	void setBlockTextureSize(unsigned int index, Vec2Int size);
	void setBlockRenderState(unsigned int index, bool renderState);
	void setBlockTextureVirtualConnection(unsigned int index, virtual_connection_id_t virtualConnectionId);
	void setBlockTextureStateOffset(unsigned int index, Vec2Int stateOffset);

private:
	// Block Data
	BlockType blockType;
	bool primitive = true; // true if defined by default (And, Or, Xor...)
	bool placeable = true;
	bool bus = false;
	std::string name = "Unnamed Block";
	std::string path = "Basic";
	Size blockSize = Size(1);
	unsigned int inputConnectionCount = 0;
	unsigned int outputConnectionCount = 0;
	std::unordered_map<connection_end_id_t, ConnectionData> connections;
	std::unordered_map<virtual_connection_id_t, VirtualConnectionData> virtualConnections;
	BidirectionalMultiSecondKeyMap<connection_end_id_t, std::string> connectionIdNames;
	std::vector<RenderDataType>	renderData; // first renders on the bottom, last on the top.
	DataUpdateEventManager& dataUpdateEventManager;
};

// defaults for good connection pos
constexpr float edgeDistance = 0.46f;
constexpr float sideShift = 0.0f;

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
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == positionOnBlock && pair.second.portType == ConnectionData::PortType::INPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getOutputConnectionId(Vector positionOnBlock) const noexcept {
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == positionOnBlock && pair.second.portType == ConnectionData::PortType::OUTPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getInputConnectionId(Vector vector, Orientation orientation) const noexcept {
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && pair.second.portType == ConnectionData::PortType::INPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getOutputConnectionId(Vector vector, Orientation orientation) const noexcept {
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && pair.second.portType == ConnectionData::PortType::OUTPUT) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept {
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && pair.second.portType == ConnectionData::PortType::BIDIRECTIONAL) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getInputOrBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept {
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (
			pair.second.positionOnBlock == noOrientationVec &&
			(pair.second.portType == ConnectionData::PortType::INPUT || pair.second.portType == ConnectionData::PortType::BIDIRECTIONAL)
		) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<connection_end_id_t> BlockData::getOutputOrBidirectionalConnectionId(Vector vector, Orientation orientation) const noexcept {
	Vector noOrientationVec = orientation.inverseTransformVectorWithArea(vector, orientation * blockSize);
	for (auto& pair : connections) {
		if (pair.second.positionOnBlock == noOrientationVec && (pair.second.portType == ConnectionData::PortType::OUTPUT || pair.second.portType == ConnectionData::PortType::BIDIRECTIONAL)) return pair.first;
	}
	return std::nullopt;
}
inline std::optional<Vector> BlockData::getConnectionVector(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return std::nullopt;
	return iter->second.positionOnBlock;
}
inline std::optional<Vector> BlockData::getConnectionVector(connection_end_id_t connectionId, Orientation orientation) const noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return std::nullopt;
	return orientation.transformVectorWithArea(iter->second.positionOnBlock, blockSize);
}
inline connection_end_id_t BlockData::getConnectionCount() const noexcept {
	return connections.size();
}
inline connection_end_id_t BlockData::getInputConnectionCount() const noexcept {
	return inputConnectionCount;
}
inline connection_end_id_t BlockData::getOutputConnectionCount() const noexcept {
	return outputConnectionCount;
}
inline connection_end_id_t BlockData::getBidirectionalConnectionCount() const noexcept {
	return connections.size() - inputConnectionCount - outputConnectionCount;
}
inline bool BlockData::connectionExists(connection_end_id_t connectionId) const noexcept {
	return connections.contains(connectionId);
}
inline bool BlockData::isConnectionInput(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	return iter != connections.end() && iter->second.portType == ConnectionData::PortType::INPUT;
}
inline bool BlockData::isConnectionOutput(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	return iter != connections.end() && iter->second.portType == ConnectionData::PortType::OUTPUT;
}
inline bool BlockData::isConnectionBidirectional(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	return iter != connections.end() && iter->second.portType == ConnectionData::PortType::BIDIRECTIONAL;
}
inline bool BlockData::isConnectionInputOrBidirectional(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return false;
	return iter->second.portType != ConnectionData::PortType::OUTPUT;
}
inline bool BlockData::isConnectionOutputOrBidirectional(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return false;
	return iter->second.portType != ConnectionData::PortType::INPUT;
}
inline BlockData::ConnectionData::PortType BlockData::getConnectionPortType(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return ConnectionData::PortType::NONE;
	return iter->second.portType;
}
inline const std::unordered_map<connection_end_id_t, BlockData::ConnectionData>& BlockData::getConnections() const noexcept {
	return connections;
}
inline const std::unordered_map<connection_end_id_t, BlockData::ConnectionData> BlockData::getConnectionsSafe() const noexcept {
	return connections;
}
inline const BlockData::ConnectionData* BlockData::getConnectionData(connection_end_id_t connectionId) const noexcept {
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return nullptr;
	return &iter->second;
}
inline std::optional<FVector> BlockData::getConnectionPortOffset(connection_end_id_t connectionId) const noexcept {
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
	auto iter = connections.find(connectionId);
	if (iter == connections.end()) return 0;
	return iter->second.getBitWidth();
}
inline const std::variant<unsigned int, std::vector<unsigned int>>* BlockData::getConnectionBitConfiguration(connection_end_id_t connectionId) const noexcept {
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
	return (primitive) && !(bus);
}

inline virtual_connection_id_t BlockData::getVirtualConnectionCount() const noexcept {
	return virtualConnections.size();
}
inline const std::unordered_map<virtual_connection_id_t, BlockData::VirtualConnectionData>& BlockData::getVirtualConnections() const noexcept {
	return virtualConnections;
}
inline const std::unordered_map<virtual_connection_id_t, BlockData::VirtualConnectionData> BlockData::getVirtualConnectionsSafe() const noexcept {
	return virtualConnections;
}
inline const BlockData::VirtualConnectionData* BlockData::getVirtualConnectionData(virtual_connection_id_t virtualConnectionId) const noexcept {
	auto iter = virtualConnections.find(virtualConnectionId);
	if (iter == virtualConnections.end()) return nullptr;
	return &iter->second;
}
inline unsigned int BlockData::getVirtualConnectionBitWidth(virtual_connection_id_t virtualConnectionId) const noexcept {
	const BlockData::VirtualConnectionData* virtualConnectionData = getVirtualConnectionData(virtualConnectionId);
	if (virtualConnectionData == nullptr) return 0;
	return virtualConnectionData->bitWidth;
}

template <class T>
void BlockData::newRenderData(unsigned int index) {
	if (index == std::numeric_limits<unsigned int>::max()) {
		renderData.emplace_back(std::in_place_type<T>);
	} else {
		assert(index < renderData.size());
		renderData.emplace(renderData.begin() + index, std::in_place_type<T>);
	}
	dataUpdateEventManager.sendEvent<std::pair<BlockType, unsigned int>>("blockDataNewRenderData", { blockType, index });
	sendBlockDataUpdate();
}

inline const BlockData::RenderDataType* BlockData::getRenderData(unsigned int index) const noexcept {
	if (index >= renderData.size()) return nullptr;
	return &renderData[index];
}

template<class T>
inline const bool BlockData::isRenderDataOfType(unsigned int index) const noexcept {
	assert(index < renderData.size());
	return std::holds_alternative<T>(renderData[index]);
}

#endif /* blockData_h */
