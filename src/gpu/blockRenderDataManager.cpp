#include "blockRenderDataManager.h"

#include "util/algorithm.h"

BlockRenderDataId BlockRenderDataManager::addBlockRenderData() {
	BlockRenderDataId newBlockRenderDataId = findUnusedKey<BlockRenderDataId>(blockRenderData, 1);
	blockRenderData.try_emplace(newBlockRenderDataId);
	return newBlockRenderDataId;
}

void BlockRenderDataManager::removeBlockRenderData(BlockRenderDataId blockRenderDataId) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call removeBlockRenderData on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	blockRenderData.erase(iter);
}

void BlockRenderDataManager::setBlockName(BlockRenderDataId blockRenderDataId, const std::string& blockName) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call setBlockName on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	iter->second.blockName = blockName;
}

void BlockRenderDataManager::setBlockSize(BlockRenderDataId blockRenderDataId, Size size) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call setBlockSize on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	iter->second.size = size;
}

void BlockRenderDataManager::setBlockTextureIndex(BlockRenderDataId blockRenderDataId, unsigned int textureIndex) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call setBlockTextureIndex on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	iter->second.textureIndex = textureIndex;
}

BlockPortRenderDataId BlockRenderDataManager::addBlockPort(BlockRenderDataId blockRenderDataId, bool isInput, FVector positionOnBlock) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call addBlockPort on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return 0;
	}
	BlockPortRenderDataId newBlockPortRenderDataId = findUnusedKey<BlockPortRenderDataId>(iter->second.blockPortRenderData, 1);
	iter->second.blockPortRenderData.try_emplace(newBlockPortRenderDataId, isInput, positionOnBlock);
	return newBlockPortRenderDataId;
}

void BlockRenderDataManager::removeBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call removeBlockPort on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	auto portIter = iter->second.blockPortRenderData.find(blockPortRenderDataId);
	if (portIter == iter->second.blockPortRenderData.end()) {
		logError("Failed to call removeBlockPort on non existent BlockPortRenderData {}.", "BlockRenderDataManager", blockPortRenderDataId);
		return;
	}
	iter->second.blockPortRenderData.erase(portIter);
}

void BlockRenderDataManager::moveBlockPort(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, FVector newPositionOnBlock) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call moveBlockPort on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	auto portIter = iter->second.blockPortRenderData.find(blockPortRenderDataId);
	if (portIter == iter->second.blockPortRenderData.end()) {
		logError("Failed to call moveBlockPort on non existent BlockPortRenderData {}.", "BlockRenderDataManager", blockPortRenderDataId);
		return;
	}
	portIter->second.positionOnBlock = newPositionOnBlock;
}

void BlockRenderDataManager::setBlockPortName(BlockRenderDataId blockRenderDataId, BlockPortRenderDataId blockPortRenderDataId, const std::string newPortName) {
	auto iter = blockRenderData.find(blockRenderDataId);
	if (iter == blockRenderData.end()) {
		logError("Failed to call setBlockPortName on non existent BlockRenderData {}.", "BlockRenderDataManager", blockRenderDataId);
		return;
	}
	auto portIter = iter->second.blockPortRenderData.find(blockPortRenderDataId);
	if (portIter == iter->second.blockPortRenderData.end()) {
		logError("Failed to call setBlockPortName on non existent BlockPortRenderData {}.", "BlockRenderDataManager", blockPortRenderDataId);
		return;
	}
	portIter->second.portName = newPortName;
}
