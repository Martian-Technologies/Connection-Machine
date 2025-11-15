#ifndef cell_h
#define cell_h

#include "block/blockDefs.h"

class Cell {
public:
	Cell(block_id_t blockId) : blockId(blockId) { }

	inline block_id_t getBlockId() const { return blockId; }

	inline void setBlockId(block_id_t blockId) { this->blockId = blockId; }

	nlohmann::json dumpState() const {
		return blockId;
	}

private:
	block_id_t blockId;
};

#endif /* cell_h */