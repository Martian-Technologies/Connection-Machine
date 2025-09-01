#ifndef blockRenderDataManager_h
#define blockRenderDataManager_h

#include "backend/position/position.h"

typedef uint32_t BlockRenderDataId;
typedef uint32_t BlockPortRenderDataId;

class BlockRenderDataManager {
public:
	struct BlockRenderData {
		struct BlockPortRenderData {
			BlockPortRenderData(bool isInput, FVector positionOnBlock) : isInput(isInput), positionOnBlock(positionOnBlock) {}
			std::string portName;
			bool isInput;
			FVector positionOnBlock;
		};
		std::string blockName = "";
		Size size = Size(1);
		unsigned int textureIndex = 0;
		std::map<BlockPortRenderDataId, BlockPortRenderData> blockPortRenderData;
	};

	const BlockRenderData* getBlockRenderData(BlockRenderDataId blockRenderDataId);

	BlockRenderDataId addBlockRenderData();
	void removeBlockRenderData(BlockRenderDataId blockRenderDataId);
	void setBlockName(BlockRenderDataId blockRenderDataId, const std::string& blockName);
	void setBlockSize(BlockRenderDataId blockRenderDataId, Size size);
	void setBlockTextureIndex(BlockRenderDataId blockRenderDataId, unsigned int textureIndex);
	BlockPortRenderDataId addBlockPort(BlockRenderDataId blockRenderDataId, bool isInput, FVector positionOnBlock);
	void removeBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId);
	void moveBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, FVector newPositionOnBlock);
	void setBlockPortName(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, const std::string newPortName);

private:
	std::map<BlockRenderDataId, BlockRenderData> blockRenderData;
};

#endif /* blockRenderDataManager_h */