#ifndef copiedBlocks_h
#define copiedBlocks_h

#include "block/blockDefs.h"
#include "backend/selection.h"
class BlockContainer;

class CopiedBlocks {
public:
	CopiedBlocks(const BlockContainer& blockContainer, SharedSelection selection);

	struct CopiedBlockData {
		BlockType blockType;
		Position position;
		Orientation orientation;
	    CopiedBlockData(BlockType type, Position pos, Orientation rot)
			: blockType(type), position(pos), orientation(rot) {}

		nlohmann::json dumpState() const {
			nlohmann::json blockJson;
			blockJson["blockType"] = blocktype_to_string(blockType);
			blockJson["position"] = position.toString();
			blockJson["orientation"] = orientation.toString();
			return blockJson;
		}
	};

	const std::vector<CopiedBlockData> getCopiedBlocks() const { return blocks; }
	const std::vector<std::pair<Position, Position>> getCopiedConnections() const { return connections; }
	Position getMinPosition() { return minPosition; }
	Position getMaxPosition() { return maxPosition; }

	nlohmann::json dumpState() const;

private:
	
	Position minPosition;
	Position maxPosition;
	std::vector<CopiedBlockData> blocks;
	std::vector<std::pair<Position, Position>> connections;
};

typedef std::shared_ptr<CopiedBlocks> SharedCopiedBlocks;

#endif /* copiedBlocks_h */
