#include "connectionMachineParser.h"
#include "backend/position/position.h"
#include "util/uuid.h"

BlockType stringToBlockType(const std::string& str) {
	if (str == "NONE") return BlockType::NONE;
	if (str == "AND") return BlockType::AND;
	if (str == "OR") return BlockType::OR;
	if (str == "XOR") return BlockType::XOR;
	if (str == "NAND") return BlockType::NAND;
	if (str == "NOR") return BlockType::NOR;
	if (str == "XNOR") return BlockType::XNOR;
	if (str == "JUNCTION") return BlockType::JUNCTION;
	if (str == "TRISTATE_BUFFER") return BlockType::TRISTATE_BUFFER;
	if (str == "BUTTON") return BlockType::BUTTON;
	if (str == "TICK_BUTTON") return BlockType::TICK_BUTTON;
	if (str == "SWITCH") return BlockType::SWITCH;
	if (str == "CONSTANT") return BlockType::CONSTANT;
	if (str == "LIGHT") return BlockType::LIGHT;
	return BlockType::CUSTOM;
}

Orientation stringToOrientation(const std::string& str) {
	if (str == "ZERO") return Orientation(Rotation::ZERO, false);
	if (str == "NINETY") return Orientation(Rotation::NINETY, false);
	if (str == "ONE_EIGHTY") return Orientation(Rotation::ONE_EIGHTY, false);
	if (str == "TWO_SEVENTY") return Orientation(Rotation::TWO_SEVENTY, false);
	if (str == "ZERO_FLIPPED") return Orientation(Rotation::ZERO, true);
	if (str == "NINETY_FLIPPED") return Orientation(Rotation::NINETY, true);
	if (str == "ONE_EIGHTY_FLIPPED") return Orientation(Rotation::ONE_EIGHTY, true);
	if (str == "TWO_SEVENTY_FLIPPED") return Orientation(Rotation::TWO_SEVENTY, true);
	return Orientation();
}

std::string blockTypeToString(BlockType type) {
	switch (type) {
	case BlockType::NONE: return "NONE";
	case BlockType::AND: return "AND";
	case BlockType::OR: return "OR";
	case BlockType::XOR: return "XOR";
	case BlockType::NAND: return "NAND";
	case BlockType::NOR: return "NOR";
	case BlockType::XNOR: return "XNOR";
	case BlockType::JUNCTION: return "JUNCTION";
	case BlockType::TRISTATE_BUFFER: return "TRISTATE_BUFFER";
	case BlockType::BUTTON: return "BUTTON";
	case BlockType::TICK_BUTTON: return "TICK_BUTTON";
	case BlockType::SWITCH: return "SWITCH";
	case BlockType::CONSTANT: return "CONSTANT";
	case BlockType::LIGHT: return "LIGHT";
	case BlockType::CUSTOM: return "CUSTOM";
	default: return "NONE";
	}
}

std::string orientationToString(Orientation orientation) {
	if (orientation == Orientation(Rotation::ZERO, false)) return "ZERO";
	if (orientation == Orientation(Rotation::NINETY, false)) return "NINETY";
	if (orientation == Orientation(Rotation::ONE_EIGHTY, false)) return "ONE_EIGHTY";
	if (orientation == Orientation(Rotation::TWO_SEVENTY, false)) return "TWO_SEVENTY";
	if (orientation == Orientation(Rotation::ZERO, true)) return "ZERO_FLIPPED";
	if (orientation == Orientation(Rotation::NINETY, true)) return "NINETY_FLIPPED";
	if (orientation == Orientation(Rotation::ONE_EIGHTY, true)) return "ONE_EIGHTY_FLIPPED";
	if (orientation == Orientation(Rotation::TWO_SEVENTY, true)) return "TWO_SEVENTY_FLIPPED";
	return "ZERO";
}

std::vector<circuit_id_t> ConnectionMachineParser::load(const std::string& path) {
	logInfo("Parsing Connection Machine Circuit File (.cir)", "ConnectionMachineParser");

	std::ifstream inputFile(path, std::ios::in | std::ios::binary);
	if (!inputFile.is_open()) {
		logError("Couldn't open file at path: " + path, "ConnectionMachineParser");
		return {};
	}

	logInfo("Inserted current file as a dependency: " + path, "ConnectionMachineParser");

	std::string token;
	char cToken;
	inputFile >> token;

	unsigned int version;
	if (token == "version_7") {
		version = 7;
	} else if (token == "version_6") {
		version = 6;
	} else if (token == "version_5" || token == "version_4" || token == "version_3" || token == "version_2") {
		version = 5;
	} else if (token == "version_1") {
		version = 1;
	} else {
		logError("Invalid circuit file version: " + token, "ConnectionMachineParser");
		return {};
	}

	std::vector<circuit_id_t> circuitIds;
	SharedParsedCircuit currentParsedCircuit = nullptr;
	if (version == 1) {
		currentParsedCircuit = std::make_shared<ParsedCircuit>();
		currentParsedCircuit->setAbsoluteFilePath(path);
		currentParsedCircuit->setName(std::filesystem::path(path).stem().string());
	}

	while (inputFile >> token) {
		if (token == "import") {
			std::string importFileName;
			inputFile >> std::quoted(importFileName);
			// std::filesystem::path(importFileName).
			std::filesystem::path fullPath = std::filesystem::absolute(std::filesystem::path(path)).parent_path() / importFileName;
			const std::string& fPath = std::filesystem::weakly_canonical(fullPath).generic_string();
			circuitFileManager->loadFromFile(fPath);
		} else if (token == "Circuit:") {
			if (currentParsedCircuit) {
				circuit_id_t circuitId = loadParsedCircuit(*currentParsedCircuit);
				if (circuitId != 0) circuitIds.push_back(circuitId);
				currentParsedCircuit = nullptr;
			}
			currentParsedCircuit = std::make_shared<ParsedCircuit>();
			currentParsedCircuit->setAbsoluteFilePath(path);
			std::string circuitName;
			inputFile >> std::quoted(circuitName);
			currentParsedCircuit->setName(circuitName);
			logInfo("\tFound circuit: {}", "ConnectionMachineParser", circuitName);
		} else if (token == "size:") {
			currentParsedCircuit->markAsCustom();
			unsigned int width, height;
			if (version < 7)
				inputFile >> cToken >> width >> cToken >> height >> cToken;
			else
				inputFile >> width >> cToken >> height;
			currentParsedCircuit->setSize(Size(width, height));
		} else if (token == "ports" || token == "ports:") {
			currentParsedCircuit->markAsCustom();
			if (version <= 5) getline(inputFile, token);
			while (true) {
				inputFile >> std::ws;
				if (inputFile.peek() != '(') break;
				inputFile >> cToken;
				connection_end_id_t endId;
				int blockId;
				coordinate_t vecX, vecY;
				std::string portName = "";
				inputFile >> token >> endId >> cToken >> blockId >> cToken >> cToken >> vecX >> cToken >> vecY >> cToken >> cToken >> std::quoted(portName) >> cToken;
				currentParsedCircuit->addConnectionPort(token == "IN,", endId, Vector(vecX, vecY), blockId, 0, portName);
			}
		} else if (token == "UUID:") {
			std::string uuid;
			inputFile >> uuid;
			currentParsedCircuit->setUUID(uuid == "null" ? generate_uuid_v4() : uuid);
		} else if (token == "blockId") {
			// block id
			int blockId;
			std::string blockTypeStr;
			float posX, posY;
			inputFile >> blockId >> std::quoted(blockTypeStr);
			BlockType blockType = stringToBlockType(blockTypeStr);

			if (blockType == BlockType::CUSTOM) {
				SharedCircuit circuit = circuitManager->getCircuit(blockTypeStr);
				if (circuit) {
					blockType = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(circuit->getCircuitId())->getBlockType();
				} else {
					SharedProceduralCircuit proceduralCircuit = circuitManager->getProceduralCircuitManager()->getProceduralCircuit(blockTypeStr);
					if (proceduralCircuit) {
						blockType = proceduralCircuit->getBlockType(ProceduralCircuitParameters(inputFile));
					} else {
						logError("Count not find Circuit or ProceduralCircuit with UUID: {}", "ConnectionMachineParser", blockTypeStr);
						return circuitIds;
					}
				}
			}

			inputFile >> posX >> posY >> token;
			Orientation orientation = stringToOrientation(token);

			currentParsedCircuit->addBlock(blockId, FPosition(posX, posY), orientation, blockType);

			if (version <= 5) getline(inputFile, token);
			while (true) {
				inputFile >> std::ws;
				if (inputFile.peek() != '(') break;
				inputFile >> cToken;
				int connId;
				inputFile >> cToken >> cToken >> cToken >> cToken >> cToken >> cToken >> cToken >> connId >> cToken; // (connId:x)
				std::string line;
				std::getline(inputFile, line);
				std::istringstream lineStream(line);
				while (lineStream >> cToken) { // open paren
					int otherConnId, otherBlockId;
					if (!(lineStream >> otherBlockId >> otherConnId >> cToken)) {
						logError("Failed to parse (blockid, connection_id) token", "ConnectionMachineParser");
						break;
					}
					currentParsedCircuit->addConnection(blockId, connId, otherBlockId, otherConnId);
				}
			}
		}
	}
	if (currentParsedCircuit) {
		circuit_id_t circuitId = loadParsedCircuit(*currentParsedCircuit);
		if (circuitId != 0) circuitIds.push_back(circuitId);
	}
	inputFile.close();
	importedFiles.erase(path);
	return circuitIds;
}

bool ConnectionMachineParser::save(const CircuitFileManager::FileData& fileData, bool compressed) {
	const std::string& path = fileData.fileLocation;

	std::ofstream outputFile(path);
	if (!outputFile.is_open()) {
		logError("Couldn't open file at path: {}", "ConnectionMachineParser", path);
		return false;
	}

	outputFile << "version_7\n";

	// find all required imports
	// not ideal but if we loop through from maxBlockId down then we will find all dependencies across every circuit, not just this one
	std::unordered_set<BlockType> imports;
	std::set<std::string> pathImports;
	std::map<std::string, std::set<std::string>> inFileDependencies;
	for (const std::string& UUID : fileData.UUIDs) {
		SharedCircuit circuit = circuitManager->getCircuit(UUID);
		if (!circuit) continue;
		const BlockContainer* blockContainer = circuit->getBlockContainer();
		for (auto itr = blockContainer->begin(); itr != blockContainer->end(); ++itr) {
			BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(itr->second.type());
			if (!blockData) {
				logError("Could not find block data for block {}", "ConnectionMachineParser", std::to_string(itr->second.type()));
				continue;
			}
			if (blockData->isPrimitive() || !imports.insert(blockData->getBlockType()).second) continue;
			circuit_id_t subCircuitId = circuitManager->getCircuitBlockDataManager()->getCircuitId(blockData->getBlockType());
			const CircuitBlockData* subCircuitBlockData = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(subCircuitId);
			if (!subCircuitBlockData) {
				logError("Could not find save path for depedecy {}", "ConnectionMachineParser", subCircuitId);
				continue;
			}
			const std::optional<std::string>& subProceduralCircuitUUID = subCircuitBlockData->getProceduralCircuitUUID();
			const std::string* subSavePath = nullptr;
			const std::string* subUUID;
			if (subProceduralCircuitUUID) {
				subUUID = &(subProceduralCircuitUUID.value());
			} else {
				SharedCircuit circuit = circuitManager->getCircuit(subCircuitId);
				subUUID = &(circuit->getUUID());
			}
			subSavePath = circuitFileManager->getSavePath(*subUUID);
			if (!subSavePath) {
				logError("Could not find save path for depedecy {}", "ConnectionMachineParser", *subUUID);
				continue;
			}

			if (std::filesystem::equivalent(std::filesystem::path(*subSavePath), std::filesystem::path(path))) {
				inFileDependencies[UUID].emplace(*subUUID);
			} else {
				if (!pathImports.insert(*subSavePath).second) continue;
				try {
					std::string relPath = std::filesystem::relative(std::filesystem::path(*subSavePath), std::filesystem::path(path) / "..").generic_string();
					outputFile << "import \"" << relPath << "\"\n";
				} catch (...) {
					logError("Could not find relPath between, \"{}\" and \"{}\".", "ConnectionMachineParser", *subSavePath, path);
				}
			}
		}
	}
	std::list<std::string> UUIDsToPutInFile(fileData.UUIDs.begin(), fileData.UUIDs.end());
	std::set<std::string> UUIDsAlreadyInFile;
	for (const std::string& UUID : UUIDsToPutInFile) {
		bool dontAddYet = false;
		for (const std::string& otherUUID : inFileDependencies[UUID]) {
			if (!(UUIDsAlreadyInFile.contains(otherUUID))) {
				dontAddYet = true;
				break;
			}
		}
		if (dontAddYet) {
			UUIDsToPutInFile.push_back(std::move(UUID));
			continue;
		}
		UUIDsAlreadyInFile.emplace(UUID);
		SharedCircuit circuit = circuitManager->getCircuit(UUID);
		if (!circuit) continue;;
		const BlockContainer* blockContainer = circuit->getBlockContainer();
		const CircuitBlockData* circuitBlockData = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(circuit->getCircuitId());
		outputFile << "Circuit: \"" << circuit->getCircuitName() << "\"\n";
		outputFile << "UUID: " << circuit->getUUID() << "\n";
		if (circuitBlockData) {
			BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(circuitBlockData->getBlockType());
			outputFile << "size: " << blockData->getSize().toString() << "\n";
			outputFile << "ports:\n";
			for (auto pair : blockData->getConnections()) {
				const Position* position = circuitBlockData->getConnectionIdToPosition(pair.first);
				block_id_t id;
				if (position) {
					const Block* block = blockContainer->getBlock(*position);
					if (!block) {
						logError("Could not find block for connection: {}", "ConnectionMachineParser", pair.first);
						continue;
					}
					id = block->id();
				} else {
					logError("Could not find position for connection: {}", "ConnectionMachineParser", pair.first);
					id = 0;
				}
				std::optional<std::string> name = blockData->getConnectionIdToName(pair.first);
				if (!name) name = "";

				outputFile << "\t(" << (pair.second.second ? "IN, " : "OUT, ") << pair.first << ", " << id << ", " << pair.second.first.toString() << ", \"" << *name << "\")\n";
			}
		}

		for (auto itr = blockContainer->begin(); itr != blockContainer->end(); ++itr) {
			const Block& block = itr->second;
			Position pos = block.getPosition();


			const BlockData* blockData = circuitManager->getBlockDataManager()->getBlockData(block.type());
			connection_end_id_t connectionNum = blockData->getConnectionCount();
			std::string blockTypeStr;
			if (!blockData->isPrimitive()) {
				circuit_id_t subCircuitId = circuitManager->getCircuitBlockDataManager()->getCircuitId(block.type());
				const std::optional<std::string>& proceduralCircuitUUID = circuitManager->getCircuitBlockDataManager()->getCircuitBlockData(subCircuitId)->getProceduralCircuitUUID();
				if (proceduralCircuitUUID.has_value()) {
					const ProceduralCircuitParameters* proceduralCircuitParameters =
						circuitManager->getProceduralCircuitManager()->getProceduralCircuit(proceduralCircuitUUID.value())->getProceduralCircuitParameters(subCircuitId);

					blockTypeStr = '"' + proceduralCircuitUUID.value() + "\" " + (proceduralCircuitParameters->toString());
				} else {
					const SharedCircuit circuit = circuitManager->getCircuit(subCircuitId);
					blockTypeStr = '"' + circuit->getUUID() + '"';
				}
			} else {
				blockTypeStr = blockTypeToString(block.type());
			}

			outputFile << "blockId " << itr->first << ' '
				<< blockTypeStr << ' ' << pos.x << ' '
				<< pos.y << ' ' << orientationToString(block.getOrientation()) << '\n';
			const ConnectionContainer& connectionContainer = block.getConnectionContainer();

			for (auto& connectionIter : connectionContainer.getConnections()) {
				outputFile << '\t' << "(connId:" << connectionIter.first << ')';
				for (ConnectionEnd conn : connectionIter.second) {
					outputFile << " (" << conn.getBlockId() << ' ' << conn.getConnectionId() << ')';
				}
				outputFile << '\n';
			}
		}
	}
	outputFile.close();
	return true;
}
