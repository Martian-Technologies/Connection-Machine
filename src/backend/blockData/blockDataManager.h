#ifndef blockDataManager_h
#define blockDataManager_h

#include "backend/dataUpdateEventManager.h"
#include "blockData.h"

class BlockDataManager {
public:
	BlockDataManager(DataUpdateEventManager& dataUpdateEventManager) : dataUpdateEventManager(dataUpdateEventManager) { }
	BlockDataManager(const BlockDataManager&) = delete;
    BlockDataManager& operator=(const BlockDataManager&) = delete;

	void initializeDefaults();

	inline BlockType addBlock() noexcept {
		blockData.emplace_back((BlockType)(blockData.size() + 1), dataUpdateEventManager);
		BlockType blockType = (BlockType)blockData.size();
		dataUpdateEventManager.sendEvent<BlockType>("newBlockType", blockType);
		// pre
		dataUpdateEventManager.sendEvent<std::pair<BlockType, Size>>("preBlockSizeChange", { blockType, Size(1) });
		// post
		dataUpdateEventManager.sendEvent<std::pair<BlockType, Size>>("postBlockSizeChange", { blockType, Size(1) });
		sendBlockDataUpdate(blockType);
		return blockType;
	}

	inline BlockType getBlockType(const std::string& blockPath) const {
		for (unsigned int i = 0; i < blockData.size(); i++) {
			if (blockData[i].getPath() + "/" + blockData[i].getName() == blockPath) {
				return (BlockType)(i + 1);
			}
		}
		return BlockType::NONE;
	}

	inline void sendBlockDataUpdate(BlockType blockType) { dataUpdateEventManager.sendEvent<BlockType>("blockDataUpdate", blockType); }

	inline const BlockData* getBlockData(BlockType type) const noexcept {
		if (!blockExists(type)) return nullptr;
		return &blockData[type - 1];
	}
	inline BlockData* getBlockData(BlockType type) noexcept {
		if (!blockExists(type)) return nullptr;
		return &blockData[type - 1];
	}

	inline unsigned int maxBlockId() const noexcept { return blockData.size(); }
	inline bool blockExists(BlockType type) const noexcept { return type != BlockType::NONE && type <= blockData.size(); }
	inline bool isPlaceable(BlockType type) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].isPlaceable();
	}

	inline std::string getName(BlockType type) const noexcept {
		if (!blockExists(type)) return "None" + std::to_string(type);
		return blockData[type - 1].getName();
	}
	inline std::string getPath(BlockType type) const noexcept {
		if (!blockExists(type)) return "Path To None";
		return blockData[type - 1].getPath();
	}

	inline Size getBlockSize(BlockType type) const noexcept {
		if (!blockExists(type)) return Size();
		return blockData[type - 1].getSize();
	}
	inline Size getBlockSize(BlockType type, Orientation orientation) const noexcept {
		if (!blockExists(type)) return Size();
		return blockData[type - 1].getSize(orientation);
	}

	inline std::optional<connection_end_id_t> getInputConnectionId(BlockType type, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getInputConnectionId(vector);
	}
	inline std::optional<connection_end_id_t> getOutputConnectionId(BlockType type, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getOutputConnectionId(vector);
	}
	inline std::optional<connection_end_id_t> getInputConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getInputConnectionId(vector, orientation);
	}
	inline std::optional<connection_end_id_t> getOutputConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getOutputConnectionId(vector, orientation);
	}
	inline std::optional<connection_end_id_t> getBidirectionalConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getBidirectionalConnectionId(vector, orientation);
	}
	inline std::optional<connection_end_id_t> getInputOrBidirectionalConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getInputOrBidirectionalConnectionId(vector, orientation);
	}
	inline std::optional<connection_end_id_t> getOutputOrBidirectionalConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getOutputOrBidirectionalConnectionId(vector, orientation);
	}
	inline std::optional<Vector> getConnectionVector(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getConnectionVector(connectionId);
	}
	inline std::optional<Vector> getConnectionVector(BlockType type, Orientation orientation, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getConnectionVector(connectionId, orientation);
	}
	inline connection_end_id_t getConnectionCount(BlockType type) const noexcept {
		if (!blockExists(type)) return 0;
		return blockData[type - 1].getConnectionCount();
	}
	inline bool connectionExists(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].connectionExists(connectionId);
	}
	inline bool isConnectionInput(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].isConnectionInput(connectionId);
	}
	inline bool isConnectionOutput(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].isConnectionOutput(connectionId);
	}
	inline bool isConnectionBidirectional(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].isConnectionBidirectional(connectionId);
	}
	inline bool isConnectionInputOrBidirectional(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].isConnectionInputOrBidirectional(connectionId);
	}
	inline bool isConnectionOutputOrBidirectional(BlockType type, connection_end_id_t connectionId) const noexcept {
		if (!blockExists(type)) return false;
		return blockData[type - 1].isConnectionOutputOrBidirectional(connectionId);
	}

	BlockType getBusBlock(unsigned int bitwith);
	BlockType getBusBlock(unsigned int numInputs, unsigned int inputBitwidth);
	BlockType getBusBlock(unsigned int numInputs, unsigned int numOutputs, unsigned int inputLaneWidth, unsigned int outputLaneWidth);
	struct BusConnectionData {
		Vector positionOnBlock;
		std::variant<unsigned int, std::vector<unsigned int>> bitConfiguration = static_cast<unsigned int>(1);

		bool operator==(const BusConnectionData& other) const noexcept { return positionOnBlock == other.positionOnBlock && bitConfiguration == other.bitConfiguration; }

		auto operator<=>(const BusConnectionData& other) const noexcept {
			if (positionOnBlock == other.positionOnBlock) {
				if (auto cmp = bitConfiguration <=> other.bitConfiguration; cmp != 0) return cmp;
				return std::strong_ordering::equal;
			}
			if (positionOnBlock.dy == other.positionOnBlock.dy) return positionOnBlock.dx <=> other.positionOnBlock.dx;
			return positionOnBlock.dy <=> other.positionOnBlock.dy;
		}

		nlohmann::json dumpState() const;
	};
	BlockType getBusBlock(std::vector<BusConnectionData> busConnections);

	nlohmann::json dumpState() const;

private:
	std::vector<BlockData> blockData;
	DataUpdateEventManager& dataUpdateEventManager;
	std::map<std::vector<BusConnectionData>, BlockType> createdBuses;

	static nlohmann::json dumpBusConnectionDataVector(const std::vector<BusConnectionData>& busConnections);
};

#endif /* blockDataManager_h */
