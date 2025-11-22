#ifndef blockContainer3d_h
#define blockContainer3d_h

#include "backend/position/sparse3d.h"
#include "block/block3d.h"
#include "difference3d.h"
#include "backend/container/cell.h"

class CircuitManager;

class BlockContainer3d {
public:
	inline BlockContainer3d(CircuitManager& circuitManager, BlockDataManager& blockDataManager) : circuitManager(circuitManager), blockDataManager(blockDataManager) { }

	inline BlockDataManager& getBlockDataManager() const { return blockDataManager; }

	void clear(Difference3d* difference);
	bool isEmpty() const { return blocks.empty(); }

	inline BlockType getBlockType() const { return selfBlockType; }
	inline void setBlockType(BlockType type) { if (getBlockTypeCount(type) == 0) selfBlockType = type; }

	/* ----------- collision ----------- */
	inline bool checkCollision(Position3 position) const { return getCell(position); }
	bool checkCollision(Position3 positionSmall, Position3 positionLarge) const;
	bool checkCollision(Position3 positionSmall, Position3 positionLarge, block_id_t idToIgnore) const;
	bool checkCollision(Position3 position, Orientation3d orientation, BlockType blockType) const;
	bool checkCollision(Position3 position, Orientation3d orientation, BlockType blockType, block_id_t idToIgnore) const;

	/* ----------- blocktype ---------- */
	bool canInsertBlocktype(BlockType blockType) const;

	/* ----------- blocks ----------- */
	// -- getters --
	// Gets the cell at that position. Returns nullptr the cell is empty
	inline const Cell* getCell(Position3 position) const { return grid.get(position); }
	// Gets the number of cells in the BlockContainer
	inline unsigned int getCellCount() const { return grid.size(); }
	// Gets the block that has a cell at that position. Returns nullptr the cell is empty
	inline const Block3d* getBlock(Position3 position) const;
	// Gets the block that has a id. Returns nullptr if no block has the id
	inline const Block3d* getBlock(block_id_t blockId) const;
	// Gets the number of blocks in the BlockContainer
	inline unsigned int getBlockCount() const { return blocks.size(); }
	// gets the number of times a block with a certain type appears
	inline unsigned int getBlockTypeCount(BlockType blockType) const { if (blockTypeCounts.size() <= blockType) return 0; return blockTypeCounts[blockType]; }
	// // gets the number of times a block with a certain type appears in this and child block containers // because it will not have this
	// inline unsigned int getBlockTypeCountRecursive(BlockType blockType) const;

	// -- setters --
	// Trys to insert a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryInsertBlock(Position3 position, Orientation3d orientation, BlockType blockType, Difference3d* difference);
	// Trys to remove a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryRemoveBlock(Position3 position, Difference3d* difference);
	// Trys to move a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryMoveBlock(Position3 positionOfBlock, Position3 position, Orientation3d transformAmount, Difference3d* difference, MoveType moveType = MoveType::SINGLE);
	// Trys to set the type of a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool trySetType(Position3 positionOfBlock, BlockType type, Difference3d* difference);
	// moves blocks until they
	void resizeBlockType(BlockType blockType, Size3 size, Difference3d* difference);

	/* ----------- connections ----------- */
	// -- getters --
	bool connectionExists(Position3 outputPosition3, Position3 inputPosition3) const;
	bool connectionExists(ConnectionEnd connectionEndA, ConnectionEnd connectionEndB) const;
	const std::unordered_set<ConnectionEnd>* getInputConnections(Position3 position) const;
	const std::unordered_set<ConnectionEnd>* getOutputConnections(Position3 position) const;
	const std::unordered_set<ConnectionEnd>* getBidirectionalConnections(Position3 position) const;
	const std::optional<ConnectionEnd> getInputConnectionEnd(Position3 position) const;
	const std::optional<ConnectionEnd> getOutputConnectionEnd(Position3 position) const;
	const std::optional<ConnectionEnd> getBidirectionalConnectionEnd(Position3 position) const;
	const std::optional<ConnectionEnd> getInputOrBidirectionalConnectionEnd(Position3 position) const;
	const std::optional<ConnectionEnd> getOutputOrBidirectionalConnectionEnd(Position3 position) const;

	unsigned int getBitwidthOfJunction(Position3 position) const { return getBitwidthOfJunction(getBlock(position)); }
	unsigned int getBitwidthOfJunction(block_id_t blockId) const { return getBitwidthOfJunction(getBlock(blockId)); }
	unsigned int getBitwidthOfJunctionIgnorePort(block_id_t blockId, BlockType blockType, connection_end_id_t endId) const { return getBitwidthOfJunctionIgnorePort(getBlock(blockId), blockType, endId); }

	// -- setters --
	// Trys to creates a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryCreateConnection(ConnectionEnd connectionEndA, ConnectionEnd connectionEndB, Difference3d* difference);
	// Trys to creates a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryCreateConnection(Position3 outputPosition3, Position3 inputPosition3, Difference3d* difference);
	// Trys to remove a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryRemoveConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd, Difference3d* difference);
	// Trys to remove a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryRemoveConnection(Position3 outputPosition3, Position3 inputPosition3, Difference3d* difference);
	// Sets up connection containers to have the new end id
	void addConnectionPort(BlockType blockType, connection_end_id_t endId, Difference3d* difference);
	// Removes all connects made to port that are a invalide bitwidth
	void setConnectionPortBitwidth(BlockType blockType, connection_end_id_t endId, unsigned int bitwidth, Difference3d* difference);
	// Removes all connects and set connection containers to not have the end id
	void removeConnectionPort(BlockType blockType, connection_end_id_t endId, Difference3d* difference);

	/* ----------- iterators ----------- */
	// not safe if the container gets modifided (dont worry about it for now)
	typedef std::unordered_map<block_id_t, Block3d>::iterator iterator;
	typedef std::unordered_map<block_id_t, Block3d>::const_iterator const_iterator;
	iterator begin() { return blocks.begin(); }
	iterator end() { return blocks.end(); }
	const_iterator begin() const { return blocks.begin(); }
	const_iterator end() const { return blocks.end(); }

	/* Difference Getter */
	Difference3d getCreationDifference() const;
	Difference3dSharedPtr getCreationDifferenceShared() const;

	nlohmann::json dumpState() const;

private:
	unsigned int getBitwidthOfJunction(const Block3d* block) const;
	unsigned int getBitwidthOfJunction(block_id_t blockId, std::unordered_set<block_id_t>& visited) const;
	unsigned int getBitwidthOfJunctionIgnorePort(const Block3d* block, BlockType blockType, connection_end_id_t endId) const;
	unsigned int getBitwidthOfJunctionIgnorePort(const Block3d* block, BlockType blockType, connection_end_id_t endId, std::unordered_set<block_id_t>& visited) const;

	inline Block3d* getBlock_(Position3 position);
	inline Block3d* getBlock_(block_id_t blockId);
	inline Cell* getCell(Position3 position) { return grid.get(position); }
	inline void insertCell(Position3 position, Cell cell) { grid.insert(position, cell); }
	inline void removeCell(Position3 position) { grid.remove(position); }
	void placeBlockCells(block_id_t id, Position3 position, Size3 size);
	void placeBlockCells(Position3 position, Orientation3d orientation, BlockType type, block_id_t blockId);
	void placeBlockCells(const Block3d* block);
	void removeBlockCells(const Block3d* block);
	block_id_t getNewId() { return ++lastId; }

	BlockType selfBlockType = BlockType::NONE;
	CircuitManager& circuitManager;
	BlockDataManager& blockDataManager;
	block_id_t lastId = 0;
	Sparse3d<Cell> grid;
	std::unordered_map<block_id_t, Block3d> blocks;
	std::vector<unsigned int> blockTypeCounts;
};

inline Block3d* BlockContainer3d::getBlock_(Position3 position) {
	const Cell* cell = grid.get(position);
	return cell == nullptr ? nullptr : &(blocks.find(cell->getBlockId())->second);
}

inline const Block3d* BlockContainer3d::getBlock(Position3 position) const {
	const Cell* cell = grid.get(position);
	return cell == nullptr ? nullptr : &(blocks.find(cell->getBlockId())->second);
}

inline Block3d* BlockContainer3d::getBlock_(block_id_t blockId) {
	auto iter = blocks.find(blockId);
	return (iter == blocks.end()) ? nullptr : &(iter->second);
}

inline const Block3d* BlockContainer3d::getBlock(block_id_t blockId) const {
	auto iter = blocks.find(blockId);
	return (iter == blocks.end()) ? nullptr : &(iter->second);
}

#endif /* blockContainer3d_h */
