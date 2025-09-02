#include "circuitManager.h"

#include "backend/evaluator/evaluatorManager.h"
#include "backend/proceduralCircuits/generatedCircuit.h"
#include "parsedCircuit.h"

circuit_id_t CircuitManager::createNewCircuit(const std::string& name, const std::string& uuid, bool createEval) {
	circuit_id_t id = getNewCircuitId();
	const SharedCircuit circuit = std::make_shared<Circuit>(id, this, &blockDataManager, dataUpdateEventManager, name, uuid);
	circuits.emplace(id, circuit);
	UUIDToCircuits.emplace(uuid, circuit);
	for (auto& [object, funcData] : listenerFunctions) {
		circuit->connectListener(object, funcData.second, funcData.first);
	}

	setupBlockData(id);

	if (createEval) {
		auto evaluatorId = evaluatorManager->createNewEvaluator(*this, id);
		SharedEvaluator eval = evaluatorManager->getEvaluator(evaluatorId);
		eval->setPause(false);
		eval->setUseTickrate(true);
		eval->setTickrate(40);
	}

	return id;
}

CircuitManager::CircuitManager(DataUpdateEventManager* dataUpdateEventManager, EvaluatorManager* evaluatorManager, CircuitFileManager* fileManager) :
	blockDataManager(dataUpdateEventManager), circuitBlockDataManager(dataUpdateEventManager), proceduralCircuitManager(this, dataUpdateEventManager, fileManager),
	dataUpdateEventManager(dataUpdateEventManager), dataUpdateEventReceiver(dataUpdateEventManager), evaluatorManager(evaluatorManager) {
	dataUpdateEventReceiver.linkFunction("postBlockSizeChange", [this](const DataUpdateEventManager::EventData* eventData) { linkedFunctionForUpdates<Vector>(eventData); });
	dataUpdateEventReceiver.linkFunction("blockDataRemoveConnection", [this](const DataUpdateEventManager::EventData* eventData) { linkedFunctionForUpdates<connection_end_id_t>(eventData); });
	dataUpdateEventReceiver.linkFunction("blockDataSetConnection", [this](const DataUpdateEventManager::EventData* eventData) { linkedFunctionForUpdates<connection_end_id_t>(eventData); });
	dataUpdateEventReceiver.linkFunction("blockDataConnectionNameSet", [this](const DataUpdateEventManager::EventData* eventData) { linkedFunctionForUpdates<connection_end_id_t>(eventData); });
}

circuit_id_t CircuitManager::createNewCircuit(const ParsedCircuit& parsedCircuit, bool createEval) {
	if (!parsedCircuit.isValid()) {
		logError("parsedCircuit is not validated", "CircuitManager");
		return 0;
	}

	std::string uuid = parsedCircuit.getUUID();
	if (uuid.empty()) {
		logInfo("Setting a uuid for parsed circuit", "CircuitManager");
		uuid = generate_uuid_v4();
	} else {
		SharedCircuit possibleExistingCircuit = getCircuit(uuid);
		if (possibleExistingCircuit) {
			// this duplicates check won't really work with open circuits ics because we have no way of knowing
			// unless we save which paths we have loaded. Though this would require then linking the IC blocktype to
			// the parsed circuit which seems annoying
			logWarning("Dependency Circuit with UUID {} already exists; not creating custom block.", "CircuitManager", uuid);
			return possibleExistingCircuit->getCircuitId();
		} else {
			if (getProceduralCircuitManager()->getProceduralCircuit(uuid)) {
				logWarning("Dependency Circuit with UUID {} already exists as ProceduralCircuit. Can't create block.", "CircuitManager", uuid);
				return 0;
			}
		}
	}

	circuit_id_t id = createNewCircuit(parsedCircuit.getName(), uuid, createEval);
	SharedCircuit circuit = getCircuit(id);
	circuit->tryInsertParsedCircuit(parsedCircuit, Position());

	// if is custom
	if (!parsedCircuit.isCustom()) {
		return id;
	}

	// Block Data
	BlockType blockType = circuit->getBlockType();
	if (blockType == BlockType::NONE) {
		blockType = blockDataManager.addBlock();
	}
	BlockData* blockData = blockDataManager.getBlockData(blockType);
	if (!blockData) {
		logError("Did not find newly created block data with block type: {}", "CircuitManager", std::to_string(blockType));
		return id;
	}
	blockData->setDefaultData(false);
	blockData->setPrimitive(false);
	blockData->setPath("Custom");
	blockData->setSize(parsedCircuit.getSize());

	// Circuit Block Data
	circuitBlockDataManager.newCircuitBlockData(id, blockType);
	circuit->setBlockType(blockType);

	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(id);
	if (!circuitBlockData) {
		logError("Did not find newly created circuit block data with circuit id: {}", "CircuitManager", (unsigned int)id);
		return id;
	}

	// Connections
	const std::vector<ParsedCircuit::ConnectionPort>& ports = parsedCircuit.getConnectionPorts();

	for (const ParsedCircuit::ConnectionPort& port : ports) {
		if (port.isInput) blockData->setConnectionInput(port.positionOnBlock, port.connectionEndId);
		else blockData->setConnectionOutput(port.positionOnBlock, port.connectionEndId);
		if (!port.portName.empty()) {
			blockData->setConnectionIdName(port.connectionEndId, port.portName);
		}
		if (port.internalBlockId != 0) {
			const ParsedCircuit::BlockData* parsedBlock = parsedCircuit.getBlock(port.internalBlockId);
			circuitBlockData->setConnectionIdPosition(
				port.connectionEndId,
				parsedBlock->position.snap() + blockDataManager.getConnectionVector(parsedBlock->type, port.internalBlockConnectionEndId).value()
			);
		}
	}

	dataUpdateEventManager->sendEvent("blockDataUpdate");

	return id;
}

circuit_id_t CircuitManager::createNewCircuit(const GeneratedCircuit& generatedCircuit, bool createEval) {
	if (!generatedCircuit.isValid()) {
		logError("GeneratedCircuit is not validated", "CircuitManager");
		return 0;
	}

	std::string uuid = generate_uuid_v4();
	circuit_id_t id = createNewCircuit(generatedCircuit.getName(), uuid, createEval);
	SharedCircuit circuit = getCircuit(id);
	circuit->tryInsertGeneratedCircuit(generatedCircuit, Position());

	if (!generatedCircuit.isCustom()) {
		return id;
	}

	// Block Data
	BlockType blockType = circuit->getBlockType();
	if (blockType == BlockType::NONE) {
		blockType = blockDataManager.addBlock();
	}
	BlockData* blockData = blockDataManager.getBlockData(blockType);
	if (!blockData) {
		logError("Did not find newly created block data with block type: {}", "CircuitManager", std::to_string(blockType));
		return id;
	}
	blockData->setDefaultData(false);
	blockData->setPrimitive(false);
	blockData->setPath("Custom");
	blockData->setSize(generatedCircuit.getSize());

	// Circuit Block Data
	circuitBlockDataManager.newCircuitBlockData(id, blockType);
	circuit->setBlockType(blockType);

	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(id);
	if (!circuitBlockData) {
		logError("Did not find newly created circuit block data with circuit id: {}", "CircuitManager", (unsigned int)id);
		return id;
	}

	// Connections
	const std::vector<GeneratedCircuit::ConnectionPort>& ports = generatedCircuit.getConnectionPorts();

	for (const GeneratedCircuit::ConnectionPort& port : ports) {
		if (port.isInput) blockData->setConnectionInput(port.positionOnBlock, port.connectionEndId);
		else blockData->setConnectionOutput(port.positionOnBlock, port.connectionEndId);
		if (!port.portName.empty()) {
			blockData->setConnectionIdName(port.connectionEndId, port.portName);
		}
		if (port.internalBlockId == 0) {
			logError("Can't find port.internalBlockId should not be 0.", "CircuitManager");
		} else {
			const GeneratedCircuit::GeneratedCircuitBlockData* blockData = generatedCircuit.getBlock(port.internalBlockId);
			circuitBlockData->setConnectionIdPosition(
				port.connectionEndId,
				blockData->position + blockDataManager.getConnectionVector(blockData->type, blockData->orientation, port.internalBlockConnectionEndId).value()
			);
		}
	}

	dataUpdateEventManager->sendEvent("blockDataUpdate");

	return id;
}

void CircuitManager::updateExistingCircuit(circuit_id_t id, const GeneratedCircuit* generatedCircuit) {
	if (!generatedCircuit->isValid()) {
		logError("GeneratedCircuit is not validated", "CircuitManager");
		return;
	}

	SharedCircuit circuit = getCircuit(id);
	std::string uuid = circuit->getUUID();

	circuit->clear(true);

	circuit->tryInsertGeneratedCircuit(*generatedCircuit, Position());

	if (!generatedCircuit->isCustom()) {
		BlockType blockType = circuit->getBlockType();
		if (blockType != BlockType::NONE) {
			logError("Trying to update circuit to no longer be a custom block. This can not be done yet. UUID: {}", "CircuitManager", uuid);
		}
		return;
	}

	// Block Data
	BlockType blockType = circuit->getBlockType();
	if (blockType == BlockType::NONE) {
		blockType = blockDataManager.addBlock();
	}
	BlockData* blockData = blockDataManager.getBlockData(blockType);
	if (!blockData) {
		logError("Did not find newly created block data with block type: {}", "CircuitManager", std::to_string(blockType));
		return;
	}
	blockData->setDefaultData(false);
	blockData->setPrimitive(false);
	blockData->setPath("Custom");
	blockData->setSize(generatedCircuit->getSize());

	// Circuit Block Data
	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(id);
	if (!circuitBlockData) {
		circuitBlockDataManager.newCircuitBlockData(id, blockType);
		circuit->setBlockType(blockType);
	}

	if (!circuitBlockData) {
		logError("Did not find newly created circuit block data with circuit id: {}", "CircuitManager", (unsigned int)id);
		return;
	}

	// Connections
	const std::vector<GeneratedCircuit::ConnectionPort>& ports = generatedCircuit->getConnectionPorts();

	for (const GeneratedCircuit::ConnectionPort& port : ports) {
		if (port.isInput) blockData->setConnectionInput(port.positionOnBlock, port.connectionEndId);
		else blockData->setConnectionOutput(port.positionOnBlock, port.connectionEndId);
		if (!port.portName.empty()) blockData->setConnectionIdName(port.connectionEndId, port.portName);

		if (port.internalBlockId == 0) {
			logError("Can't find port.internalBlockId should not be 0.", "CircuitManager");
		} else {
			const GeneratedCircuit::GeneratedCircuitBlockData* generatedBlockData = generatedCircuit->getBlock(port.internalBlockId);
			circuitBlockData->setConnectionIdPosition(
				port.connectionEndId,
				generatedBlockData->position + blockDataManager.getConnectionVector(generatedBlockData->type, generatedBlockData->orientation, port.internalBlockConnectionEndId).value()
			);
		}
	}

	std::vector<connection_end_id_t> endIdsToRemove;
	for (auto iter : blockData->getConnections()) {
		bool found = false;
		for (const GeneratedCircuit::ConnectionPort& port : ports) {
			if (port.connectionEndId == iter.first) {
				found = true;
				break;
			}
		}
		if (!found) {
			endIdsToRemove.push_back(iter.first);
		}
	}
	for (connection_end_id_t endId : endIdsToRemove) {
		blockData->removeConnection(endId);
	}

	for (const GeneratedCircuit::ConnectionPort& port : ports) {
		if (blockData->connectionExists(port.connectionEndId)) {
			if (blockData->isConnectionInput(port.connectionEndId) != port.isInput) blockData->removeConnection(port.connectionEndId);
		}

		if (port.isInput) blockData->setConnectionInput(port.positionOnBlock, port.connectionEndId);
		else blockData->setConnectionOutput(port.positionOnBlock, port.connectionEndId);
		blockData->setConnectionIdName(port.connectionEndId, port.portName);

		if (port.internalBlockId == 0) {
			logError("Can't find port.internalBlockId should not be 0.", "CircuitManager");
		} else {
			const GeneratedCircuit::GeneratedCircuitBlockData* generatedBlockData = generatedCircuit->getBlock(port.internalBlockId);
			circuitBlockData->setConnectionIdPosition(
				port.connectionEndId,
				generatedBlockData->position + blockDataManager.getConnectionVector(generatedBlockData->type, generatedBlockData->orientation, port.internalBlockConnectionEndId).value()
			);
		}
	}

	dataUpdateEventManager->sendEvent("blockDataUpdate");
}
