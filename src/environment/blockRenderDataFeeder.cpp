#include "blockRenderDataFeeder.h"

#include "backend/backend.h"
#include "gpu/cpuImageEditor/cpuImage.h"
#include "gpu/freetype/freetype.h"
#include "gpu/mainRenderer.h"

BlockRenderDataFeeder::BlockRenderDataFeeder(Backend* backend) : backend(backend), dataUpdateEventReceiver(backend->getDataUpdateEventManager()) {
	logInfo("Initializing BlockRenderDataFeeder", "BlockRenderDataFeeder");
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
	logInfo("BlockRenderDataFeeder initialized", "BlockRenderDataFeeder");
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
	const BlockData* blockData = backend->getBlockDataManager()->getBlockData(data->get().first);
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
	const BlockData* blockData = backend->getBlockDataManager()->getBlockData(data->get().first);
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
		CpuImage img({ blockData->getSize().w * 256, blockData->getSize().h * 256 }, { 0, 0, 0, 255 });
		img.addRect({ 5, 5 }, img.getSize() - Vec2Int(10, 10), { 76, 76, 76, 255 });
		for (const std::pair<connection_end_id_t, BlockData::ConnectionData>& connection : blockData->getConnections()) {
			Vec2Int portTexturePos = (
				Vec2Int(connection.second.positionOnBlock.dx * 256, connection.second.positionOnBlock.dy * 256) +
				Vec2Int(connection.second.portOffset.dx * 256.f, connection.second.portOffset.dy * 256.f)
			);
			img.addRect(portTexturePos - Vec2Int(4,4), Vec2Int(8, 8), { 0, 0, 0, 255 });
		}
		img.writeStringInArea(font, blockData->getName(), { 75, 75 }, img.getSize() - Vec2Int(150, 150), { 255, 255, 255, 255 });
		blockTextureId = MainRenderer::get().addBlockTexture(img.getData(), img.getSize().x, img.getSize().y);
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

void BlockRenderDataFeeder::doBlockTextureUpdates() {
	for (BlockType blockType : blockTexturesToUpdate) {
		const BlockData* blockData = backend->getBlockDataManager()->getBlockData(blockType);
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
