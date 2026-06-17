#include "blockRenderDataFeeder.h"

#include "gpu/blockRenderDataManager.h"
#include "backend/backend.h"
#include "gpu/freetype/freetype.h"
#include "gpu/mainRenderer.h"
#include "computerAPI/directoryManager.h"

BlockRenderDataFeeder::BlockRenderDataFeeder(Backend& backend) : backend(backend), dataUpdateEventReceiver(backend.getDataUpdateEventManager()) {
	dataUpdateEventReceiver.linkFunction("newBlockType", std::bind(&BlockRenderDataFeeder::newBlockType_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("postBlockSizeChange", std::bind(&BlockRenderDataFeeder::postBlockSizeChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockNameChange", std::bind(&BlockRenderDataFeeder::blockNameChange_event, this, std::placeholders::_1));

	dataUpdateEventReceiver.linkFunction("blockDataSetConnection", std::bind(&BlockRenderDataFeeder::blockDataSetConnection_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataPortBitConfigurationSet", std::bind(&BlockRenderDataFeeder::blockDataPortBitConfigurationSet_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataRemoveConnection", std::bind(&BlockRenderDataFeeder::blockDataRemoveConnection_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataConnectionNameSet", std::bind(&BlockRenderDataFeeder::blockDataConnectionNameSet_event, this, std::placeholders::_1));

	dataUpdateEventReceiver.linkFunction("blockDataTexturePathChange", std::bind(&BlockRenderDataFeeder::blockDataTexturePathChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureUseFullTextureChange", std::bind(&BlockRenderDataFeeder::blockDataTextureUseFullTextureChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureTopLeftChange", std::bind(&BlockRenderDataFeeder::blockDataTextureTopLeftChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureSizeChange", std::bind(&BlockRenderDataFeeder::blockDataTextureSizeChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureRenderStateChange", std::bind(&BlockRenderDataFeeder::blockDataTextureRenderStateChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureVirtualConnectionChange", std::bind(&BlockRenderDataFeeder::blockDataTextureVirtualConnectionChange_event, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("blockDataTextureStateOffsetChange", std::bind(&BlockRenderDataFeeder::blockDataTextureStateOffsetChange_event, this, std::placeholders::_1));

	const std::string* fontPath = Settings::get<SettingType::FILE_PATH>("Appearance/Font");
	std::pair<char*, unsigned long long> fontData;
	if (fontPath) {
		std::string realFontPath = DirectoryManager::extendPath(*fontPath).generic_string();
		font = Freetype::get().loadFont(realFontPath, 64);
		if (font == nullptr) {
			std::filesystem::path fallBackPath = (DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf");
			logError("Failed to read font from \"{}\". Falling back to \"{}\"", "BlockRenderDataFeeder::BlockRenderDataFeeder", realFontPath, fallBackPath.generic_string());
			font = Freetype::get().loadFont(fallBackPath.generic_string(), 64);
		} else {
			logInfo("Loaded, {}", "BlockRenderDataFeeder::BlockRenderDataFeeder", realFontPath);
		}
	} else {
		std::filesystem::path fallBackPath = (DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf");
		logError("Failed to load file path from setting. Falling back to \"{}\"", "BlockRenderDataFeeder::BlockRenderDataFeeder", fallBackPath.generic_string());
		font = Freetype::get().loadFont(fallBackPath.generic_string(), 64);
	}
	blockTextureGenerator = std::make_unique<BlockTextureGenerator>(font);

	Settings::registerListener<SettingType::FILE_PATH>("Appearance/Font", [this](const std::string& fontFilePath) {
		std::pair<char*, unsigned long long> fontData;
		std::string realFontPath = DirectoryManager::extendPath(fontFilePath).generic_string();
		font = Freetype::get().loadFont(realFontPath, 64);
		if (font == nullptr) {
			std::filesystem::path fallBackPath = (DirectoryManager::getResourceDirectory() / "gui/fonts/Consolas.ttf");
			logError("Failed to read font from \"{}\". Falling back to \"{}\"", "BlockRenderDataFeeder::BlockRenderDataFeeder", realFontPath, fallBackPath.generic_string());
			font = Freetype::get().loadFont(fallBackPath.generic_string(), 64);
		} else {
			logInfo("Loaded, {}", "BlockRenderDataFeeder::BlockRenderDataFeeder", realFontPath);
		}
		blockTextureGenerator = std::make_unique<BlockTextureGenerator>(font);
	});
}

BlockRenderDataId BlockRenderDataFeeder::getBlockRenderDataId(BlockType blockType) const {
	auto iter = blockTypeToRenderData.find(blockType);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to get BlockRenderDataId for BlockType {}", "BlockRenderDataFeeder", blockType);
		return 0;
	}
	return iter->second.blockRenderDataId;
}

void BlockRenderDataFeeder::newBlockType_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<BlockType>();
	assert(data);
	BlockRenderDataId blockRenderDataId = MainRenderer::get().registerBlockRenderData();
	blockTypeToRenderData.emplace(data->get(), blockRenderDataId);
	refreshBlockTexture(data->get());
	const BlockData* blockData = backend.getBlockDataManager().getBlockData(data->get());
	MainRenderer::get().setTextureVirtualConnection(blockRenderDataId, 0);
}

void BlockRenderDataFeeder::postBlockSizeChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::pair<BlockType, Size>>();
	assert(data);
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	MainRenderer::get().setBlockSize(iter->second.blockRenderDataId, data->get().second);
	refreshBlockTexture(data->get().first);
}

void BlockRenderDataFeeder::blockNameChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::pair<BlockType, std::string>>();
	assert(data);
	auto iter = blockTypeToRenderData.find(data->get().first);
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", data->get().first);
		return;
	}
	MainRenderer::get().setBlockName(iter->second.blockRenderDataId, data->get().second);
	refreshBlockTexture(data->get().first);
}

void BlockRenderDataFeeder::blockDataSetConnection_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::pair<BlockType, connection_end_id_t>>();
	assert(data);
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
		refreshBlockTexture(data->get().first);
		return;
	}
	MainRenderer::get().moveBlockPort(
		iter->second.blockRenderDataId,
		portIter->second,
		blockData->getConnectionVector(data->get().second)->free() +
			(isInput ? blockData->getConnectionPortOffset(data->get().second).value() : blockData->getConnectionPortOffset(data->get().second).value())
	);
	refreshBlockTexture(data->get().first);
}

void BlockRenderDataFeeder::blockDataPortBitConfigurationSet_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, connection_end_id_t, unsigned int>>();
	assert(data);

	// auto iter = blockTypeToRenderData.find(std::get<0>(data->get()));
	// if (iter == blockTypeToRenderData.end()) {
	// 	logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", std::get<0>(data->get()));
	// 	return;
	// }
	// auto portIter = iter->second.blockPortRenderDataIds.find(std::get<1>(data->get()));
	// const BlockData* blockData = backend.getBlockDataManager().getBlockData(std::get<0>(data->get()));
	// bool isInput = blockData->isConnectionInput(std::get<1>(data->get()));
	// if (portIter == iter->second.blockPortRenderDataIds.end()) {
	// 	logError("Failed to find BlockPortRenderDataId for BlockType {}, connection_end_id {}", "BlockRenderDataFeeder", std::get<0>(data->get()), std::get<1>(data->get()));
	// 	return;
	// }
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::blockDataRemoveConnection_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::pair<BlockType, connection_end_id_t>>();
	assert(data);
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
	refreshBlockTexture(data->get().first);
}

void BlockRenderDataFeeder::blockDataConnectionNameSet_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::pair<BlockType, connection_end_id_t>>();
	assert(data);
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
	refreshBlockTexture(data->get().first);
}

void BlockRenderDataFeeder::blockDataTexturePathChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, std::string>>();
	assert(data);
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::blockDataTextureUseFullTextureChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, bool>>();
	assert(data);
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::blockDataTextureTopLeftChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, Vec2Int>>();
	assert(data);
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::blockDataTextureSizeChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, Vec2Int>>();
	assert(data);
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::blockDataTextureRenderStateChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, bool>>();
	assert(data);
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::blockDataTextureVirtualConnectionChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, virtual_connection_id_t>>();
	assert(data);
	auto iter = blockTypeToRenderData.find(std::get<0>(data->get()));
	if (iter == blockTypeToRenderData.end()) {
		logError("Failed to find RenderData for BlockType {}", "BlockRenderDataFeeder", std::get<0>(data->get()));
		return;
	}

	MainRenderer::get().setTextureVirtualConnection(iter->second.blockRenderDataId, std::get<2>(data->get()));
}

void BlockRenderDataFeeder::blockDataTextureStateOffsetChange_event(const DataUpdateEventManager::EventData* event) {
	const auto* data = event->cast<std::tuple<BlockType, unsigned int, Vec2Int>>();
	assert(data);
	refreshBlockTexture(std::get<0>(data->get()));
}

void BlockRenderDataFeeder::refreshBlockTexture(BlockType blockType) {
	blockTexturesToUpdate.insert(blockType);
}

BlockTextureId BlockRenderDataFeeder::getBlockTextureId(const BlockData& blockData, RenderData& renderData) {
	BlockTextureId blockTextureId = 0;
	if (blockData.getRenderDataSize() == 0) {
		// create new block texture
		int scale = 256;
		int width = blockData.getSize().w * scale;
		int height = blockData.getSize().h * scale;
		auto [xPadding, yPadding] = calculatePadding(width, height);
		int paddedWidth = width + xPadding * 2;
		int paddedHeight = height + yPadding * 2;
		if (paddedWidth > BLOCK_TEXTURE_SIZE || paddedHeight > BLOCK_TEXTURE_SIZE) {
			if (paddedWidth > paddedHeight) {
				scale = (BLOCK_TEXTURE_SIZE - xPadding * 2) / blockData.getSize().w;
			} else {
				scale = (BLOCK_TEXTURE_SIZE - yPadding * 2) / blockData.getSize().h;
			}
		}
		CpuImage img({ blockData.getSize().w * scale, blockData.getSize().h * scale }, { 0, 0, 0, 255 });
		assert(blockTextureGenerator && "BlockTextureGenerator is not initialized");
		if (blockData.isBus()) {
			blockTextureGenerator->createBusBlockTexture(blockData, img, scale);
		} else {
			blockTextureGenerator->createCustomBlockTexture(blockData, img, scale);
		}
		CpuImage paddedImg = padTexture(img);
		blockTextureId = MainRenderer::get().addBlockTexture(paddedImg.getData(), paddedImg.getSize().x, paddedImg.getSize().y);
	} else {
		const BlockData::RenderDataType* renderData = blockData.getRenderData(0);
		if (renderData != nullptr && !std::get<BlockData::BlockTextureData>(*renderData).path.empty()) {
			blockTextureId = MainRenderer::get().addBlockTexture(std::get<BlockData::BlockTextureData>(*renderData).path);
		}
	}
	if (renderData.blockTextureId == blockTextureId) return blockTextureId;
	if (renderData.blockTextureId != 0) {
		auto iter = blockTextureIdUsage.find(renderData.blockTextureId);
		if (iter->second <= 1) {
			blockTextureIdUsage.erase(iter);
			MainRenderer::get().removeBlockTexture(renderData.blockTextureId);
		} else {
			iter->second--;
		}
	}
	renderData.blockTextureId = blockTextureId;
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
			continue;
		}

		auto iter = blockTypeToRenderData.find(blockType);
		if (iter == blockTypeToRenderData.end()) {
			logError("Failed to find RenderData for BlockType {} \"{}\"", "BlockRenderDataFeeder", blockType, blockData->getName());
			continue;
		}

		if (!blockData->getRenderDataSize()) {
			BlockTextureId blockTextureId = getBlockTextureId(*blockData, iter->second);
			if (blockTextureId == 0) continue;
			MainRenderer::get().setBlockTexture(iter->second.blockRenderDataId, blockTextureId);
		} else {
			BlockTextureId blockTextureId = getBlockTextureId(*blockData, iter->second);
			if (blockTextureId == 0) continue;
			const BlockData::RenderDataType* renderData = blockData->getRenderData(0);
			assert(renderData != nullptr && std::holds_alternative<BlockData::BlockTextureData>(*renderData));
			const BlockData::BlockTextureData& textureData = std::get<BlockData::BlockTextureData>(*renderData);
			if (textureData.useFullTexture) {
				MainRenderer::get().setBlockTexture(
					iter->second.blockRenderDataId,
					blockTextureId
				);
			} else {
				if (textureData.renderState) {
					MainRenderer::get().setBlockTexture(
						iter->second.blockRenderDataId,
						blockTextureId,
						textureData.topLeft,
						textureData.size,
						textureData.stateOffset
					);
				} else {
					MainRenderer::get().setBlockTexture(
						iter->second.blockRenderDataId,
						blockTextureId,
						textureData.topLeft,
						textureData.size
					);
				}
			}
		}
	}
	blockTexturesToUpdate.clear();
}
