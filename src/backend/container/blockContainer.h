#ifndef blockContainer_h
#define blockContainer_h

#include "backend/position/sparse2d.h"
#include "block/block.h"
#include "difference.h"
#include "cell.h"

class CircuitManager;

class BlockContainer {
public:
	inline BlockContainer(CircuitManager* circuitManager, BlockDataManager* blockDataManager) : circuitManager(circuitManager), blockDataManager(blockDataManager) { }

	inline BlockDataManager* getBlockDataManager() const { return blockDataManager; }

	void clear(Difference* difference);

	inline BlockType getBlockType() const { return selfBlockType; }
	inline void setBlockType(BlockType type) { if (getBlockTypeCount(type) == 0) selfBlockType = type; }

	/* ----------- collision ----------- */
	inline bool checkCollision(Position position) const { return getCell(position); }
	bool checkCollision(Position positionSmall, Position positionLarge) const;
	bool checkCollision(Position positionSmall, Position positionLarge, block_id_t idToIgnore) const;
	bool checkCollision(Position position, Orientation orientation, BlockType blockType) const;
	bool checkCollision(Position position, Orientation orientation, BlockType blockType, block_id_t idToIgnore) const;

	/* ----------- blocktype ---------- */
	bool canInsertBlocktype(BlockType blockType) const;

	/* ----------- blocks ----------- */
	// -- getters --
	// Gets the cell at that position. Returns nullptr the cell is empty
	inline const Cell* getCell(Position position) const { return grid.get(position); }
	// Gets the number of cells in the BlockContainer
	inline unsigned int getCellCount() const { return grid.size(); }
	// Gets the block that has a cell at that position. Returns nullptr the cell is empty
	inline const Block* getBlock(Position position) const;
	// Gets the block that has a id. Returns nullptr if no block has the id
	inline const Block* getBlock(block_id_t blockId) const;
	// Gets the number of blocks in the BlockContainer
	inline unsigned int getBlockCount() const { return blocks.size(); }
	// gets the number of times a block with a certain type appears
	inline unsigned int getBlockTypeCount(BlockType blockType) const { if (blockTypeCounts.size() <= blockType) return 0; return blockTypeCounts[blockType]; }
	// gets the number of times a block with a certain type appears in this and child block containers
	inline unsigned int getBlockTypeCountRecursive(BlockType blockType) const;

	// -- setters --
	// Trys to insert a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryInsertBlock(Position position, Orientation orientation, BlockType blockType, Difference* difference);
	// Trys to remove a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryRemoveBlock(Position position, Difference* difference);
	// Trys to move a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryMoveBlock(Position positionOfBlock, Position position, Orientation transformAmount, Difference* difference, MoveType moveType = MoveType::SINGLE);
	// Trys to set the type of a block. Returns if successful. Pass a Difference* to read the what changes were made.
	bool trySetType(Position positionOfBlock, BlockType type, Difference* difference);
	// moves blocks until they
	void resizeBlockType(BlockType blockType, Size size, Difference* difference);

	/* ----------- connections ----------- */
	// -- getters --
	bool connectionExists(Position outputPosition, Position inputPosition) const;
	const phmap::flat_hash_set<ConnectionEnd>* getInputConnections(Position position) const;
	const phmap::flat_hash_set<ConnectionEnd>* getOutputConnections(Position position) const;
	const std::optional<ConnectionEnd> getInputConnectionEnd(Position position) const;
	const std::optional<ConnectionEnd> getOutputConnectionEnd(Position position) const;

	// -- setters --
	// Trys to creates a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryCreateConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd, Difference* difference);
	// Trys to creates a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryCreateConnection(Position outputPosition, Position inputPosition, Difference* difference);
	// Trys to remove a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryRemoveConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd, Difference* difference);
	// Trys to remove a connection. Returns if successful. Pass a Difference* to read the what changes were made.
	bool tryRemoveConnection(Position outputPosition, Position inputPosition, Difference* difference);
	// Sets up connection containers to have the new end id
	void addConnectionPort(BlockType blockType, connection_end_id_t endId, Difference* difference);
	// Remove all connects and set connection containers to not have the end id
	void removeConnectionPort(BlockType blockType, connection_end_id_t endId, Difference* difference);

	/* ----------- iterators ----------- */
	// not safe if the container gets modifided (dont worry about it for now)
	typedef std::unordered_map<block_id_t, Block>::iterator iterator;
	typedef std::unordered_map<block_id_t, Block>::const_iterator const_iterator;
	iterator begin() { return blocks.begin(); }
	iterator end() { return blocks.end(); }
	const_iterator begin() const { return blocks.begin(); }
	const_iterator end() const { return blocks.end(); }

	/* Difference Getter */
	Difference getCreationDifference() const;
	DifferenceSharedPtr getCreationDifferenceShared() const;

private:
	inline Block* getBlock_(Position position);
	inline Block* getBlock_(block_id_t blockId);
	inline Cell* getCell(Position position) { return grid.get(position); }
	inline void insertCell(Position position, Cell cell) { grid.insert(position, cell); }
	inline void removeCell(Position position) { grid.remove(position); }
	void placeBlockCells(block_id_t id, Position position, Size size);
	void placeBlockCells(Position position, Orientation orientation, BlockType type, block_id_t blockId);
	void placeBlockCells(const Block* block);
	void removeBlockCells(const Block* block);
	block_id_t getNewId() { return ++lastId; }

	BlockType selfBlockType = BlockType::NONE;
	CircuitManager* circuitManager;
	BlockDataManager* blockDataManager;
	block_id_t lastId = 0;
	Sparse2d<Cell> grid;
	std::unordered_map<block_id_t, Block> blocks;
	std::vector<unsigned int> blockTypeCounts;
};

inline Block* BlockContainer::getBlock_(Position position) {
	const Cell* cell = grid.get(position);
	return cell == nullptr ? nullptr : &(blocks.find(cell->getBlockId())->second);
}

inline const Block* BlockContainer::getBlock(Position position) const {
	const Cell* cell = grid.get(position);
	return cell == nullptr ? nullptr : &(blocks.find(cell->getBlockId())->second);
}

inline Block* BlockContainer::getBlock_(block_id_t blockId) {
	auto iter = blocks.find(blockId);
	return (iter == blocks.end()) ? nullptr : &(iter->second);
}

inline const Block* BlockContainer::getBlock(block_id_t blockId) const {
	auto iter = blocks.find(blockId);
	return (iter == blocks.end()) ? nullptr : &(iter->second);
}

#endif /* blockContainer_h */
