#include "blockDataManager.h"

#include "computerAPI/directoryManager.h"

void BlockDataManager::initializeDefaults() {
	assert(blockData.size() == 0); // should call this before doing anything
	logInfo("Initializing default BlockData", "BlockDataManager");
	// load default data
	for (unsigned int i = 0; i < 22; i++) addBlock();

	std::string mainTexturePath = (DirectoryManager::getResourceDirectory() / "logicTiles.png").string();

	// AND
	BlockData* andBlockData = getBlockData(BlockType::AND);
	andBlockData->setName("And");
	andBlockData->setTexturePath(mainTexturePath);
	andBlockData->setUsesTileMapTexture(true);
	andBlockData->setTextureTileSize({ 256, 256 });
	andBlockData->setTextureBlockTileSize({ 1, 1 });
	andBlockData->setTextureSmallestCordTile({ 1, 0 });
	// OR
	BlockData* orBlockData = getBlockData(BlockType::OR);
	orBlockData->setName("Or");
	orBlockData->setTexturePath(mainTexturePath);
	orBlockData->setUsesTileMapTexture(true);
	orBlockData->setTextureTileSize({ 256, 256 });
	orBlockData->setTextureBlockTileSize({ 1, 1 });
	orBlockData->setTextureSmallestCordTile({ 2, 0 });
	// XOR
	BlockData* xorBlockData = getBlockData(BlockType::XOR);
	xorBlockData->setName("Xor");
	xorBlockData->setTexturePath(mainTexturePath);
	xorBlockData->setUsesTileMapTexture(true);
	xorBlockData->setTextureTileSize({ 256, 256 });
	xorBlockData->setTextureBlockTileSize({ 1, 1 });
	xorBlockData->setTextureSmallestCordTile({ 3, 0 });
	// NAND
	BlockData* nandBlockData = getBlockData(BlockType::NAND);
	nandBlockData->setName("Nand");
	nandBlockData->setTexturePath(mainTexturePath);
	nandBlockData->setUsesTileMapTexture(true);
	nandBlockData->setTextureTileSize({ 256, 256 });
	nandBlockData->setTextureBlockTileSize({ 1, 1 });
	nandBlockData->setTextureSmallestCordTile({ 4, 0 });
	// NOR
	BlockData* norBlockData = getBlockData(BlockType::NOR);
	norBlockData->setName("Nor");
	norBlockData->setTexturePath(mainTexturePath);
	norBlockData->setUsesTileMapTexture(true);
	norBlockData->setTextureTileSize({ 256, 256 });
	norBlockData->setTextureBlockTileSize({ 1, 1 });
	norBlockData->setTextureSmallestCordTile({ 5, 0 });
	// XNOR
	BlockData* xnorBlockData = getBlockData(BlockType::XNOR);
	xnorBlockData->setName("Xnor");
	xnorBlockData->setTexturePath(mainTexturePath);
	xnorBlockData->setUsesTileMapTexture(true);
	xnorBlockData->setTextureTileSize({ 256, 256 });
	xnorBlockData->setTextureBlockTileSize({ 1, 1 });
	xnorBlockData->setTextureSmallestCordTile({ 6, 0 });
	// BUFFER
	BlockData* bufferBlockData = getBlockData(BlockType::BUFFER);
	bufferBlockData->setName("Buffer");
	bufferBlockData->setTexturePath(mainTexturePath);
	bufferBlockData->setUsesTileMapTexture(true);
	bufferBlockData->setTextureTileSize({ 256, 256 });
	bufferBlockData->setTextureBlockTileSize({ 1, 1 });
	bufferBlockData->setTextureSmallestCordTile({ 7, 0 });
	// NOT
	BlockData* notBlockData = getBlockData(BlockType::NOT);
	notBlockData->setName("Not");
	notBlockData->setTexturePath(mainTexturePath);
	notBlockData->setUsesTileMapTexture(true);
	notBlockData->setTextureTileSize({ 256, 256 });
	notBlockData->setTextureBlockTileSize({ 1, 1 });
	notBlockData->setTextureSmallestCordTile({ 8, 0 });
	// BUTTON
	BlockData* buttonBlockData = getBlockData(BlockType::BUTTON);
	buttonBlockData->setName("Button");
	buttonBlockData->setDefaultData(false);
	buttonBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	buttonBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	buttonBlockData->setTexturePath(mainTexturePath);
	buttonBlockData->setUsesTileMapTexture(true);
	buttonBlockData->setTextureTileSize({ 256, 256 });
	buttonBlockData->setTextureBlockTileSize({ 1, 1 });
	buttonBlockData->setTextureSmallestCordTile({ 9, 0 });
	// TICK_BUTTON
	BlockData* tickButtonBlockData = getBlockData(BlockType::TICK_BUTTON);
	tickButtonBlockData->setName("Tick Button");
	tickButtonBlockData->setDefaultData(false);
	tickButtonBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	tickButtonBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	tickButtonBlockData->setTexturePath(mainTexturePath);
	tickButtonBlockData->setUsesTileMapTexture(true);
	tickButtonBlockData->setTextureTileSize({ 256, 256 });
	tickButtonBlockData->setTextureBlockTileSize({ 1, 1 });
	tickButtonBlockData->setTextureSmallestCordTile({ 10, 0 });
	// SWITCH
	BlockData* switchBlockData = getBlockData(BlockType::SWITCH);
	switchBlockData->setName("Switch");
	switchBlockData->setDefaultData(false);
	switchBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	switchBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	switchBlockData->setTexturePath(mainTexturePath);
	switchBlockData->setUsesTileMapTexture(true);
	switchBlockData->setTextureTileSize({ 256, 256 });
	switchBlockData->setTextureBlockTileSize({ 1, 1 });
	switchBlockData->setTextureSmallestCordTile({ 11, 0 });
	// CONSTANT OFF
	BlockData* constantOffBlockData = getBlockData(BlockType::CONSTANT_OFF);
	constantOffBlockData->setName("Constant");
	constantOffBlockData->setDefaultData(false);
	constantOffBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	constantOffBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	constantOffBlockData->setTexturePath(mainTexturePath);
	constantOffBlockData->setUsesTileMapTexture(true);
	constantOffBlockData->setTextureTileSize({ 256, 256 });
	constantOffBlockData->setTextureBlockTileSize({ 1, 1 });
	constantOffBlockData->setTextureSmallestCordTile({ 12, 0 });
	// CONSTANT ON
	BlockData* constantOnBlockData = getBlockData(BlockType::CONSTANT_ON);
	constantOnBlockData->setName("Constant");
	constantOnBlockData->setDefaultData(false);
	constantOnBlockData->setIsPlaceable(false);
	constantOnBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	constantOnBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	constantOnBlockData->setTexturePath(mainTexturePath);
	constantOnBlockData->setUsesTileMapTexture(true);
	constantOnBlockData->setTextureTileSize({ 256, 256 });
	constantOnBlockData->setTextureBlockTileSize({ 1, 1 });
	constantOnBlockData->setTextureSmallestCordTile({ 12, 0 });
	// CONSTANT Z
	BlockData* constantZBlockData = getBlockData(BlockType::CONSTANT_Z);
	constantZBlockData->setName("Constant");
	constantZBlockData->setDefaultData(false);
	constantZBlockData->setIsPlaceable(false);
	constantZBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	constantZBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	constantZBlockData->setTexturePath(mainTexturePath);
	constantZBlockData->setUsesTileMapTexture(true);
	constantZBlockData->setTextureTileSize({ 256, 256 });
	constantZBlockData->setTextureBlockTileSize({ 1, 1 });
	constantZBlockData->setTextureSmallestCordTile({ 12, 0 });
	// CONSTANT X
	BlockData* constantXBlockData = getBlockData(BlockType::CONSTANT_X);
	constantXBlockData->setName("Constant");
	constantXBlockData->setDefaultData(false);
	constantXBlockData->setIsPlaceable(false);
	constantXBlockData->setConnectionOutput(Vector(0), connection_end_id_t(0));
	constantXBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	constantXBlockData->setTexturePath(mainTexturePath);
	constantXBlockData->setUsesTileMapTexture(true);
	constantXBlockData->setTextureTileSize({ 256, 256 });
	constantXBlockData->setTextureBlockTileSize({ 1, 1 });
	constantXBlockData->setTextureSmallestCordTile({ 12, 0 });
	// LIGHT
	BlockData* lightBlockData = getBlockData(BlockType::LIGHT);
	lightBlockData->setName("Light");
	lightBlockData->setDefaultData(false);
	lightBlockData->setConnectionInput(Vector(0), connection_end_id_t(0));
	lightBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f - edgeDistance, 0.5f - sideShift));
	lightBlockData->setTexturePath(mainTexturePath);
	lightBlockData->setUsesTileMapTexture(true);
	lightBlockData->setTextureTileSize({ 256, 256 });
	lightBlockData->setTextureBlockTileSize({ 1, 1 });
	lightBlockData->setTextureSmallestCordTile({ 13, 0 });
	// JUNCTION
	BlockData* junctionBlockData = getBlockData(BlockType::JUNCTION);
	junctionBlockData->setName("Junction");
	junctionBlockData->setDefaultData(false);
	junctionBlockData->setConnectionBidirectional(Vector(0), connection_end_id_t(0));
	junctionBlockData->setTexturePath(mainTexturePath);
	junctionBlockData->setUsesTileMapTexture(true);
	junctionBlockData->setTextureTileSize({ 256, 256 });
	junctionBlockData->setTextureBlockTileSize({ 1, 1 });
	junctionBlockData->setTextureSmallestCordTile({ 8, 6 });
	junctionBlockData->setTextureBlockStateOffset({ 256, 0 });
	// JUNCTION_L
	BlockData* junctionLBlockData = getBlockData(BlockType::JUNCTION_L);
	junctionLBlockData->setName("Pull-down Resistor");
	junctionLBlockData->setPath("Hi-Z");
	junctionLBlockData->setDefaultData(false);
	junctionLBlockData->setSize(Size(1, 3));
	junctionLBlockData->setConnectionBidirectional(Vector(0, 2), connection_end_id_t(0));
	junctionLBlockData->setTexturePath(mainTexturePath);
	junctionLBlockData->setUsesTileMapTexture(true);
	junctionLBlockData->setTextureTileSize({ 256, 256 });
	junctionLBlockData->setTextureBlockTileSize({ 1, 3 });
	junctionLBlockData->setTextureSmallestCordTile({ 0, 4 });
	junctionLBlockData->setTextureBlockStateOffset({ 256, 0 });
	// JUNCTION_H
	BlockData* junctionHBlockData = getBlockData(BlockType::JUNCTION_H);
	junctionHBlockData->setName("Pull-up Resistor");
	junctionHBlockData->setPath("Hi-Z");
	junctionHBlockData->setDefaultData(false);
	junctionHBlockData->setSize(Size(1, 3));
	junctionHBlockData->setConnectionBidirectional(Vector(0, 2), connection_end_id_t(0));
	junctionHBlockData->setTexturePath(mainTexturePath);
	junctionHBlockData->setUsesTileMapTexture(true);
	junctionHBlockData->setTextureTileSize({ 256, 256 });
	junctionHBlockData->setTextureBlockTileSize({ 1, 3 });
	junctionHBlockData->setTextureSmallestCordTile({ 4, 4 });
	junctionHBlockData->setTextureBlockStateOffset({ 256, 0 });
	// JUNCTION_X
	BlockData* junctionXBlockData = getBlockData(BlockType::JUNCTION_X);
	junctionXBlockData->setName("Pull-X Resistor");
	junctionXBlockData->setPath("Hi-Z");
	junctionXBlockData->setDefaultData(false);
	junctionXBlockData->setSize(Size(1, 3));
	junctionXBlockData->setConnectionBidirectional(Vector(0, 2), connection_end_id_t(0));
	junctionXBlockData->setTexturePath(mainTexturePath);
	junctionXBlockData->setUsesTileMapTexture(true);
	junctionXBlockData->setTextureTileSize({ 256, 256 });
	junctionXBlockData->setTextureBlockTileSize({ 1, 3 });
	junctionXBlockData->setTextureSmallestCordTile({ 0, 3 });
	junctionXBlockData->setTextureBlockStateOffset({ 256, 0 });
	junctionXBlockData->setIsPlaceable(false);
	// TRISTATE_BUFFER
	BlockData* tristateBufferBlockData = getBlockData(BlockType::TRISTATE_BUFFER);
	tristateBufferBlockData->setName("Tristate Buffer");
	tristateBufferBlockData->setPath("Hi-Z");
	tristateBufferBlockData->setDefaultData(false);
	tristateBufferBlockData->setConnectionInput(Vector(0, 1), connection_end_id_t(0));
	tristateBufferBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f - edgeDistance, 0.5f - sideShift));
	tristateBufferBlockData->setConnectionIdName(connection_end_id_t(0), "Enable");
	tristateBufferBlockData->setConnectionInput(Vector(0, 0), connection_end_id_t(1));
	tristateBufferBlockData->setConnnectionPortOffset(connection_end_id_t(1), FVector(0.5f - edgeDistance, 0.5f - sideShift));
	tristateBufferBlockData->setConnectionIdName(connection_end_id_t(1), "Input");
	tristateBufferBlockData->setConnectionOutput(Vector(0, 1), connection_end_id_t(2));
	tristateBufferBlockData->setConnnectionPortOffset(connection_end_id_t(2), FVector(0.5f + edgeDistance, 0.5f + sideShift));
	tristateBufferBlockData->setConnectionIdName(connection_end_id_t(2), "Output");
	tristateBufferBlockData->setSize(Size(1, 2));
	tristateBufferBlockData->setTexturePath(mainTexturePath);
	tristateBufferBlockData->setUsesTileMapTexture(true);
	tristateBufferBlockData->setTextureTileSize({ 256, 256 });
	tristateBufferBlockData->setTextureBlockTileSize({ 1, 2 });
	tristateBufferBlockData->setTextureSmallestCordTile({ 8, 4 });
	tristateBufferBlockData->setTextureBlockStateOffset({ 256, 0 });
	// COLOR_LIGHT
	BlockData* colorLightBlockData = getBlockData(BlockType::COLOR_LIGHT);
	colorLightBlockData->setName("Color Light (6 bits)");
	colorLightBlockData->setPath("Other");
	colorLightBlockData->setDefaultData(false);
	colorLightBlockData->setConnectionInput(Vector(0), connection_end_id_t(0));
	colorLightBlockData->setConnnectionPortOffset(connection_end_id_t(0), FVector(0.5f - edgeDistance, 0.5f - sideShift));
	colorLightBlockData->setConnectionBitConfiguration(connection_end_id_t(0), std::vector<unsigned int>{ 0, 1, 2, 3, 4, 5 });
	colorLightBlockData->setUsesTileMapTexture(true);
	colorLightBlockData->setTexturePath((DirectoryManager::getResourceDirectory() / "colorLight.png").string());
	colorLightBlockData->setTextureTileSize({ 256, 256 });
	colorLightBlockData->setTextureBlockTileSize({ 1, 1 });
	colorLightBlockData->setTextureSmallestCordTile({ 0, 0 });
	colorLightBlockData->setTextureBlockStateOffset({ 8, 256 });
	logInfo("Default BlockData initialized", "BlockDataManager");
}

BlockType BlockDataManager::getBusBlock(unsigned int bitWidth) {
	return getBusBlock(bitWidth, 1);
}

BlockType BlockDataManager::getBusBlock(unsigned int numInputs, unsigned int inputBitwidth) {
	std::vector<BusConnectionData> busConnections;
	for (unsigned int i = 0; i < numInputs; i++) {
		std::vector<unsigned int> bitConfig;
		for (unsigned int j = 0; j < inputBitwidth; j++) {
			bitConfig.push_back(i * inputBitwidth + j);
		}
		busConnections.emplace_back(Vector(0, i), bitConfig);
	}
	busConnections.emplace_back(Vector(1, 0), numInputs * inputBitwidth);
	return getBusBlock(busConnections);
}

BlockType BlockDataManager::getBusBlock(unsigned int numInputs, unsigned int numOutputs, unsigned int inputLaneWidth, unsigned int outputLaneWidth) {
	std::vector<BusConnectionData> busConnections;
	for (unsigned int i = 0; i < numInputs; i++) {
		std::vector<unsigned int> bitConfig;
		for (unsigned int j = i * inputLaneWidth; j < (i + 1) * inputLaneWidth; j++) {
			bitConfig.push_back(j);
		}
		busConnections.emplace_back(Vector(0, i), bitConfig);
	}
	for (unsigned int i = 0; i < numOutputs; i++) {
		std::vector<unsigned int> bitConfig;
		for (unsigned int j = i * outputLaneWidth; j < (i + 1) * outputLaneWidth; j++) {
			bitConfig.push_back(j);
		}
		busConnections.emplace_back(Vector(1, i), bitConfig);
	}
	return getBusBlock(busConnections);
}

BlockType BlockDataManager::getBusBlock(std::vector<BusConnectionData> busConnections) {
	std::sort(busConnections.begin(), busConnections.end(), [](const BusConnectionData& a, const BusConnectionData& b) -> bool {
		return (a.positionOnBlock.dx < b.positionOnBlock.dx) || (a.positionOnBlock.dx == b.positionOnBlock.dx && a.positionOnBlock.dy < b.positionOnBlock.dy);
	});
	for (unsigned int i = 0; i < busConnections.size() - 1; i++) {
		if (busConnections[i].positionOnBlock == busConnections[i + 1].positionOnBlock) {
			logError("Can't have 2 bus ports at the same position {} on a block.", "", busConnections[i].positionOnBlock);
			return BlockType::NONE;
		}
	}
	if (createdBuses.contains(busConnections)) {
		return createdBuses.at(busConnections);
	}

	Size blockSize(1);
	for (const BusConnectionData& busConnection : busConnections) {
		blockSize.extentToFitTartgetCell(busConnection.positionOnBlock);
	}

	BlockType blockType = addBlock();
	createdBuses[busConnections] = blockType;
	BlockData* busInterfaceBlockData = getBlockData(blockType);
	busInterfaceBlockData->setDefaultData(false);
	busInterfaceBlockData->setIsBus(true);
	busInterfaceBlockData->setSize(blockSize);
	busInterfaceBlockData->setIsPlaceable(false);
	std::string name = "Bus ";
	unsigned int range = 0;
	for (unsigned int i = 0; i < busConnections.size(); i++) {
		busInterfaceBlockData->setConnectionBidirectional(busConnections[i].positionOnBlock, connection_end_id_t(i));
		if (std::holds_alternative<unsigned int>(busConnections[i].bitConfiguration)) {
			if (range != 0) {
				name += std::to_string(range) + "x1, ";
				range = 0;
			}
			name += "1x" + std::to_string(std::get<unsigned int>(busConnections[i].bitConfiguration));
			busInterfaceBlockData->setConnectionBitConfiguration(connection_end_id_t(i), std::get<unsigned int>(busConnections[i].bitConfiguration));
			if (i + 1 < busConnections.size()) name += ", ";
		} else {
			auto& vec = std::get<std::vector<unsigned int>>(busConnections[i].bitConfiguration);
			if (vec.size() == 1) {
				if (range == vec.back()) {
					range++;
				} else {
					if (range != 0) name += std::to_string(range) + "x1, ";
					range = 1;
				}
			} else {
				if (range != 0) {
					name += std::to_string(range) + "x1, ";
					range = 0;
				}
				name += "{";
				for (unsigned int j = 0; j < vec.size(); j++) {
					name += std::to_string(vec[j]);
					if (j + 1 < vec.size()) {
						name += ",";
					}
				}
				name += "}";
				if (i + 1 < busConnections.size()) name += ", ";
			}
			busInterfaceBlockData->setConnectionBitConfiguration(connection_end_id_t(i), vec);
		}
	}
	if (range != 0) name += std::to_string(range) + "x1, ";
	busInterfaceBlockData->setName(name);
	busInterfaceBlockData->setPath("Other");
	return blockType;
}

nlohmann::json BlockDataManager::dumpState() const {
	nlohmann::json stateJson;
	stateJson["blockData"] = nlohmann::json::array();
	for (const BlockData& bd : blockData) {
		stateJson["blockData"].push_back(bd.dumpState());
	}
	stateJson["createdBuses"] = nlohmann::json::array();
	for (const auto& [busConnections, blockType] : createdBuses) {
		nlohmann::json busJson;
		busJson["busConnections"] = dumpBusConnectionDataVector(busConnections);
		busJson["blockType"] = blocktype_to_string(blockType);
		stateJson["createdBuses"].push_back(busJson);
	}
	return stateJson;
}

nlohmann::json BlockDataManager::dumpBusConnectionDataVector(const std::vector<BlockDataManager::BusConnectionData>& busConnections) {
	nlohmann::json busConnectionsJson = nlohmann::json::array();
	for (const BusConnectionData& busConnection : busConnections) {
		busConnectionsJson.push_back(busConnection.dumpState());
	}
	return busConnectionsJson;
}

nlohmann::json BlockDataManager::BusConnectionData::dumpState() const {
	nlohmann::json busConnectionJson;
	busConnectionJson["positionOnBlock"] = positionOnBlock.toString();
	if (std::holds_alternative<unsigned int>(bitConfiguration)) {
		busConnectionJson["bitConfiguration"] = std::get<unsigned int>(bitConfiguration);
	} else {
		busConnectionJson["bitConfiguration"] = nlohmann::json::array();
		busConnectionJson["bitConfiguration"] = std::get<std::vector<unsigned int>>(bitConfiguration);
	}
	return busConnectionJson;
}
