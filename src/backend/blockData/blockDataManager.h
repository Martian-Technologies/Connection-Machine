#ifndef blockDataManager_h
#define blockDataManager_h

#include "backend/dataUpdateEventManager.h"
#include "blockData.h"

class BlockDataManager {
public:
	BlockDataManager(DataUpdateEventManager* dataUpdateEventManager) : dataUpdateEventManager(dataUpdateEventManager) { }

	void initializeDefaults() {
		assert(blockData.size() == 0); // should call this before doing anything
		// load default data
		for (unsigned int i = 0; i < 14; i++) addBlock();
		getBlockData(BlockType::AND)->setName("And");
		getBlockData(BlockType::OR)->setName("Or");
		getBlockData(BlockType::XOR)->setName("Xor");
		getBlockData(BlockType::NAND)->setName("Nand");
		getBlockData(BlockType::NOR)->setName("Nor");
		getBlockData(BlockType::XNOR)->setName("Xnor");
		getBlockData(BlockType::JUNCTION)->setName("Junction");
		// TRISTATE_BUFFER
		getBlockData(BlockType::TRISTATE_BUFFER)->setName("Tristate Buffer");
		getBlockData(BlockType::TRISTATE_BUFFER)->setDefaultData(false);
		getBlockData(BlockType::TRISTATE_BUFFER)->setConnectionInput(Vector(0, 1), 0);
		getBlockData(BlockType::TRISTATE_BUFFER)->setConnectionInput(Vector(0, 0), 1);
		getBlockData(BlockType::TRISTATE_BUFFER)->setConnectionOutput(Vector(0, 1), 2);
		getBlockData(BlockType::TRISTATE_BUFFER)->setSize(Size(1, 2));
		// BUTTON
		getBlockData(BlockType::BUTTON)->setName("Button");
		getBlockData(BlockType::BUTTON)->setDefaultData(false);
		getBlockData(BlockType::BUTTON)->setConnectionOutput(Vector(0), 0);
		// TICK_BUTTON
		getBlockData(BlockType::TICK_BUTTON)->setName("Tick Button");
		getBlockData(BlockType::TICK_BUTTON)->setDefaultData(false);
		getBlockData(BlockType::TICK_BUTTON)->setConnectionOutput(Vector(0), 0);
		// SWITCH
		getBlockData(BlockType::SWITCH)->setName("Switch");
		getBlockData(BlockType::SWITCH)->setDefaultData(false);
		getBlockData(BlockType::SWITCH)->setConnectionOutput(Vector(0), 0);
		// CONSTANT
		getBlockData(BlockType::CONSTANT)->setName("Constant");
		getBlockData(BlockType::CONSTANT)->setDefaultData(false);
		getBlockData(BlockType::CONSTANT)->setIsPlaceable(false);
		getBlockData(BlockType::CONSTANT)->setConnectionOutput(Vector(0), 0);
		// LIGHT
		getBlockData(BlockType::LIGHT)->setName("Light");
		getBlockData(BlockType::LIGHT)->setDefaultData(false);
		getBlockData(BlockType::LIGHT)->setConnectionInput(Vector(0), 0);
		// BUS
		getBlockData(BlockType::BUS)->setName("Bus");
		getBlockData(BlockType::BUS)->setDefaultData(false);
		getBlockData(BlockType::BUS)->setConnectionOutput(Vector(1, 0), 0);
		for (int i = 0; i < 8; ++i) {
			getBlockData(BlockType::BUS)->setConnectionInput(Vector(0, i), i + 1);
		}
		getBlockData(BlockType::BUS)->setSize(Size(2, 8));
	}

	inline BlockType addBlock() noexcept {
		blockData.emplace_back((BlockType)(blockData.size() + 1), dataUpdateEventManager);
		BlockType blockType = (BlockType)blockData.size();
		// sending data events for default data
		// pre
		dataUpdateEventManager->sendEvent<std::pair<BlockType, Size>>("preBlockSizeChange", { blockType, Size(1) });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, 0 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("preBlockDataSetConnection", { blockType, 1 });
		// post
		dataUpdateEventManager->sendEvent<std::pair<BlockType, Size>>("postBlockSizeChange", { blockType, Size(1) });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, 0 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataSetConnection", { blockType, 1 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataConnectionNameSet", { blockType, 0 });
		dataUpdateEventManager->sendEvent<std::pair<BlockType, connection_end_id_t>>("blockDataConnectionNameSet", { blockType, 1 });
		sendBlockDataUpdate();
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

	inline void sendBlockDataUpdate() { dataUpdateEventManager->sendEvent("blockDataUpdate"); }

	inline const BlockData* getBlockData(BlockType type) const noexcept { if (!blockExists(type)) return nullptr; return &blockData[type - 1]; }
	inline BlockData* getBlockData(BlockType type) noexcept { if (!blockExists(type)) return nullptr; return &blockData[type - 1]; }

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
	inline  std::optional<connection_end_id_t> getInputConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getInputConnectionId(vector, orientation);
	}
	inline  std::optional<connection_end_id_t> getOutputConnectionId(BlockType type, Orientation orientation, Vector vector) const noexcept {
		if (!blockExists(type)) return std::nullopt;
		return blockData[type - 1].getOutputConnectionId(vector, orientation);
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

private:
	std::vector<BlockData> blockData;
	DataUpdateEventManager* dataUpdateEventManager;
};

#endif /* blockDataManager_h */
