#include "blockRenderDataFeeder.h"

#include "backend/backend.h"
#include "gpu/freetype/freetype.h"
#include "gpu/mainRenderer.h"

BlockRenderDataFeeder::BlockRenderDataFeeder(Backend& backend) : backend(backend), dataUpdateEventReceiver(backend.getDataUpdateEventManager()) {
	dataUpdateEventReceiver.linkFunction("newBlockType", std::bind(&BlockRenderDataFeeder::newBlockTypeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("postBlockSizeChange", std::bind(&BlockRenderDataFeeder::postBlockSizeChangeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockNameChange", std::bind(&BlockRenderDataFeeder::blockNameChangeUpdate, this, std::placeholders::_1));

	dataUpdateEventReceiver.linkFunction("blockDataSetConnection", std::bind(&BlockRenderDataFeeder::blockDataSetConnectionUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataRemoveConnection", std::bind(&BlockRenderDataFeeder::blockDataRemoveConnectionUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataConnectionNameSet", std::bind(&BlockRenderDataFeeder::blockDataConnectionNameSetUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureChange", std::bind(&BlockRenderDataFeeder::blockDataTextureChangeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataUsesTileMapTextureChange", std::bind(&BlockRenderDataFeeder::blockDataUsesTileMapTextureChangeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureTileSizeChange", std::bind(&BlockRenderDataFeeder::blockDataTextureTileChangeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureSmallestCordTileChange", std::bind(&BlockRenderDataFeeder::blockDataTextureTileChangeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureBlockTileSizeChange", std::bind(&BlockRenderDataFeeder::blockDataTextureTileChangeUpdate, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureBlockStateOffsetChange", std::bind(&BlockRenderDataFeeder::blockDataTextureTileChangeUpdate, this, std::placeholders::_1));

	font = Freetype::get().loadFont(*Settings::get<SettingType::FILE_PATH>("Appearance/Font"), 64);
	blockTextureGenerator = std::make_unique<BlockTextureGenerator>(font);
}

BlockRenderDataId BlockRenderDataFeeder::getBlockRenderDataId(BlockType blockType) const {
	auto iter = blockTypeToRenderData.find(blockType);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to get BlockRenderDataId for BlockType {}", "BlockRenderDataFeeder", blockType);
		return 0;
	}
	return iter->second.blockRenderDataId;
}

void BlockRenderDataFeeder::newBlockTypeUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<BlockType>();
	if (!data) return;

	BlockRenderDataId blockRenderDataId = MainRenderer::get().registerBlockRenderData();
	blockTypeToRenderData.emplace(data->get(), blockRenderDataId);

	blockTexturesToUpdate.insert(data->get());
}

void BlockRenderDataFeeder::postBlockSizeChangeUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, Size>>();
	if (!data) return;
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	MainRenderer::get().setBlockSize(iter->second.blockRenderDataId, data->get().second);
	blockTexturesToUpdate.insert(data->get().first);
}

void BlockRenderDataFeeder::blockNameChangeUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, std::string>>();
	if (!data) return;
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	MainRenderer::get().setBlockName(iter->second.blockRenderDataId, data->get().second);
	blockTexturesToUpdate.insert(data->get().first);
}

void BlockRenderDataFeeder::blockDataSetConnectionUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, connection_end_id_t>>();
	if (!data) return;
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	auto portIter = iter->second.blockPortRenderDataIds.find(data->get().second);
	const BlockData* blockData = backend.getBlockDataManager().getBlockData(data->get().first);
	bool isInput = blockData->isConnectionInput(data->get().second);
	if (portIter == iter->second.blockPortRenderDataIds.end()) {
		BlockPortRenderDataId blockPortRenderDataId = MainRenderer::get().addBlockPort(
			iter->second.blockRenderDataId,
			isInput,
			blockData->getConnectionVector(data->get().second)->free() +
				(isInput ? blockData->getConnectionPortOffset(data->get().second).value() : blockData->getConnectionPortOffset(data->get().second).value())
		);
		iter->second.blockPortRenderDataIds.try_emplace(data->get().second, blockPortRenderDataId);
		return;
	}
	MainRenderer::get().moveBlockPort(
		iter->second.blockRenderDataId,
		portIter->second,
		blockData->getConnectionVector(data->get().second)->free() +
			(isInput ? blockData->getConnectionPortOffset(data->get().second).value() : blockData->getConnectionPortOffset(data->get().second).value())
	);
}

void BlockRenderDataFeeder::blockDataRemoveConnectionUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, connection_end_id_t>>();
	if (!data) return;
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	auto portIter = iter->second.blockPortRenderDataIds.find(data->get().second);
	if (portIter == iter->second.blockPortRenderDataIds.end()) {
		logError("Failed to remove BlockPortRenderData for BlockType {}, connection_end_id {}", "BlockRenderDataFeeder", data->get().first, data->get().second);
		return;
	}
	MainRenderer::get().removeBlockPort(iter->second.blockRenderDataId, portIter->second);
	iter->second.blockPortRenderDataIds.erase(portIter);
}

void BlockRenderDataFeeder::blockDataConnectionNameSetUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, connection_end_id_t>>();
	if (!data) return;
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	auto portIter = iter->second.blockPortRenderDataIds.find(data->get().second);
	if (portIter == iter->second.blockPortRenderDataIds.end()) {
		logError("Failed to find BlockPortRenderDataId for BlockType {}, connection_end_id {}", "BlockRenderDataFeeder", data->get().first, data->get().second);
		return;
	}
	const BlockData* blockData = backend.getBlockDataManager().getBlockData(data->get().first);
	MainRenderer::get().setBlockPortName(iter->second.blockRenderDataId, portIter->second, *blockData->getConnectionIdToName(data->get().second));
}

void BlockRenderDataFeeder::blockDataTextureChangeUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, std::string>>();
	if (!data) return;
	blockTexturesToUpdate.insert(data->get().first);
}

void BlockRenderDataFeeder::blockDataUsesTileMapTextureChangeUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, bool>>();
	if (!data) return;
	blockTexturesToUpdate.insert(data->get().first);
}

void BlockRenderDataFeeder::blockDataTextureTileChangeUpdate(const DataUpdateEventManager::EventData* dataEvent) {
	const auto* data = dataEvent->cast<std::pair<BlockType, Vec2Int>>();
	if (!data) return;
	blockTexturesToUpdate.insert(data->get().first);
}

void BlockRenderDataFeeder::refreshBlockTexture(BlockType blockType) {
	blockTexturesToUpdate.insert(blockType);
}

BlockTextureId BlockRenderDataFeeder::getBlockTextureId(const BlockData* blockData, RenderData* renderData) {
	if (!blockData || !renderData) return 0;

	BlockTextureId blockTextureId = 0;
	if (blockData->getTexturePath().empty()) {
		// create new block texture
		int scale = 256;
		int width = blockData->getSize().w * scale;
		int height = blockData->getSize().h * scale;
		auto [xPadding, yPadding] = calculatePadding(width, height);
		int paddedWidth = width + xPadding * 2;
		int paddedHeight = height + yPadding * 2;
		if (paddedWidth > BLOCK_TEXTURE_SIZE || paddedHeight > BLOCK_TEXTURE_SIZE) {
			if (paddedWidth > paddedHeight) {
				scale = (BLOCK_TEXTURE_SIZE - xPadding * 2) / blockData->getSize().w;
			} else {
				scale = (BLOCK_TEXTURE_SIZE - yPadding * 2) / blockData->getSize().h;
			}
		}
		CpuImage img({ blockData->getSize().w * scale, blockData->getSize().h * scale }, { 0, 0, 0, 255 });
		assert(blockTextureGenerator && "BlockTextureGenerator is not initialized");
		if (blockData->isBus()) {
			blockTextureGenerator->createBusBlockTexture(blockData, img, scale);
		} else {
			blockTextureGenerator->createCustomBlockTexture(blockData, img, scale);
		}
		CpuImage paddedImg = padTexture(img);
		blockTextureId = MainRenderer::get().addBlockTexture(paddedImg.getData(), paddedImg.getSize().x, paddedImg.getSize().y);
	} else {
		blockTextureId = MainRenderer::get().addBlockTexture(blockData->getTexturePath());
	}
	if (renderData->blockTextureId == blockTextureId) return blockTextureId;
	if (renderData->blockTextureId != 0) {
		auto iter = blockTextureIdUsage.find(renderData->blockTextureId);
		if (iter->second <= 1) {
			blockTextureIdUsage.erase(iter);
			MainRenderer::get().removeBlockTexture(renderData->blockTextureId);
		} else {
			iter->second--;
		}
	}
	renderData->blockTextureId = blockTextureId;
	blockTextureIdUsage[blockTextureId]++;
	return blockTextureId;
}

std::pair<int, int> BlockRenderDataFeeder::calculatePadding(int width, int height) {
	float xPadding = 1.0f;
	float heightToWidthRatio = (float)height / (float)width;
	float yPadding = xPadding * heightToWidthRatio;
	if (yPadding < 1.0f) {
		yPadding = 1.0f;
		xPadding = yPadding / heightToWidthRatio;
	}
	int xPixelPadding = static_cast<int>(std::round(xPadding));
	int yPixelPadding = static_cast<int>(std::round(yPadding));
	return { xPixelPadding, yPixelPadding };
}

CpuImage BlockRenderDataFeeder::padTexture(const CpuImage& img) {
	int width = img.getSize().x;
	int height = img.getSize().y;
	auto [xPixelPadding, yPixelPadding] = calculatePadding(width, height);
	Vec2Int newSize = Vec2Int(
		width + 2 * xPixelPadding,
		height + 2 * yPixelPadding
	);
	CpuImage paddedImg(newSize, { 0, 0, 0, 0 });
	for (int y = 0; y < height + yPixelPadding * 2; y++) {
		for (int x = 0; x < width + xPixelPadding * 2; x++) {
			int srcX = std::clamp(x - xPixelPadding, 0, width - 1);
			int srcY = std::clamp(y - yPixelPadding, 0, height - 1);
			paddedImg.setPixel({ x, y }, img.getPixel({ srcX, srcY }));
		}
	}
	return paddedImg;
}

void BlockRenderDataFeeder::doBlockTextureUpdates() {
	for (BlockType blockType : blockTexturesToUpdate) {
		const BlockData* blockData = backend.getBlockDataManager().getBlockData(blockType);
		if (!blockData) {
			logError("Failed to find BlockData for BlockType {}", "BlockRenderDataFeeder", blockType);
			return;
		}

		auto iter = blockTypeToRenderData.find(blockType);
		if (iter == blockTypeToRenderData.end()) {
			logError("Failed to find RenderData for BlockType {} \"{}\"", "BlockRenderDataFeeder", blockType, blockData->getName());
			return;
		}

		if (!blockData->getUsesTileMapTexture()) {
			BlockTextureId blockTextureId = getBlockTextureId(blockData, &iter->second);
			if (blockTextureId == 0) return;
			MainRenderer::get().setBlockTexture(iter->second.blockRenderDataId, blockTextureId);
		} else {
			BlockTextureId blockTextureId = getBlockTextureId(blockData, &iter->second);
			if (blockTextureId == 0) return;
			MainRenderer::get().setBlockTexture(
				iter->second.blockRenderDataId,
				blockTextureId,
				blockData->getTextureTileSize(),
				blockData->getTextureSmallestCordTile(),
				blockData->getTextureBlockTileSize(),
				blockData->getTextureBlockStateOffset()
			);
		}
	}
	blockTexturesToUpdate.clear();
}
