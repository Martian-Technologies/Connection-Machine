#include "connectionMachineParser.h"
#include "backend/position/position.h"
#include "util/uuid.h"
#include "textParser.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

std::vector<circuit_id_t> ConnectionMachineParser::load(const std::string& path) {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
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
	const unsigned int latestVersion = 8;
	if (token == "version_8") {
		version = 8;
	} else if (token == "version_7") {
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
			circuitFileManager.loadFromFile(fPath);
		} else if (token == "Circuit:") {
			if (currentParsedCircuit) {
				if (version != latestVersion) {
					currentParsedCircuit->setOldFileVersion(true);
				}
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
			if (version < 7) inputFile >> cToken >> width >> cToken >> height >> cToken;
			else inputFile >> width >> cToken >> height;
			currentParsedCircuit->setSize(Size(width, height));
		} else if (token == "ports" || token == "ports:") {
			currentParsedCircuit->markAsCustom();
			if (version <= 5) getline(inputFile, token);
			if (version <= 7) {
				while (true) {
					inputFile >> std::ws;
					if (inputFile.peek() != '(') break;
					inputFile >> cToken;
					unsigned int endId;
					int blockId;
					coordinate_t vecX, vecY;
					std::string portName = "";
					inputFile >> token >> endId >> cToken >> blockId >> cToken >> cToken >> vecX >> cToken >> vecY >> cToken >> cToken >> std::quoted(portName) >> cToken;
					currentParsedCircuit->addConnectionPort(token == "IN,", endId, Vector(vecX, vecY), blockId, 0, portName);
				}
			} else {
				while (true) {
					inputFile >> std::ws;
					if (inputFile.peek() != '(') break;
					inputFile >> cToken;
					unsigned int endId;
					std::optional<Position> positionOfBlock;
					coordinate_t vecX, vecY;
					f_coordinate_t offsetX, offsetY;
					std::string portName = "";
					inputFile >> token >> endId >> cToken >> cToken;
					if (cToken == 'N') {
						inputFile >> cToken >> cToken >> cToken; // read "NONE"
					} else {
						coordinate_t posX, posY;
						inputFile >> posX >> cToken >> posY >> cToken;
						positionOfBlock = Position(posX, posY);
					}
					inputFile >> cToken >> cToken >> vecX >> cToken >> vecY >> cToken >> cToken >> cToken >> offsetX >> cToken >> offsetY >> cToken >> cToken >> std::quoted(portName) >> cToken >> cToken;
					unsigned int bitWidth = 1;
					if (cToken == '[') {
						logError("IC cant have array of bits as input bitWidth");
						std::vector<unsigned int> bits;
						while (true) {
							int bit = 0;
							inputFile >> cToken;
							if (cToken == ']') break;
							inputFile.unget();
							inputFile >> bit;
							if (bit >= 0) {
								bits.push_back(bit);
							} else {
								logError("Invalid bit {} loaded from file.", "ConnectionMachineParser", bit);
							}
						}

					} else {
						inputFile.unget();
						inputFile >> bitWidth;
						if (bitWidth == 0) {
							logError("Invalid bit width {} loaded from file.", "ConnectionMachineParser", bitWidth);
							bitWidth = 1;
						}
					}
					inputFile >> cToken;
					currentParsedCircuit->addConnectionPort(token == "IN,", endId, *positionOfBlock, Vector(vecX, vecY), FVector(offsetX, offsetY), portName, bitWidth);
				}
			}
		} else if (token == "UUID:") {
			std::string uuid;
			inputFile >> uuid;
			currentParsedCircuit->setUUID(uuid == "null" ? generate_uuid_v4() : uuid);
		} else if (token == "renderData:") {
			while (true) {
				inputFile >> std::ws;
				if (inputFile.peek() != '(') break;
				inputFile >> cToken;
				inputFile >> token;
				if (token == "texture:") {
					std::string texturePath;
					inputFile >> std::quoted(texturePath);
					currentParsedCircuit->setTexturePath(texturePath);
					inputFile >> std::ws;
					if (inputFile.peek() != ')') {
						inputFile >> token;
						if (token == "textureTileData:") {
							int smallestTextureCordX, smallestTextureCordY, textureSizeX, textureSizeY;
							inputFile >> cToken >> smallestTextureCordX >> cToken >> smallestTextureCordY >> cToken >>
								cToken >> cToken >> textureSizeX >> cToken >> textureSizeY >> cToken;
							currentParsedCircuit->setUseFullTexture(false);
							currentParsedCircuit->setSmallestTextureCord({ smallestTextureCordX, smallestTextureCordY });
							currentParsedCircuit->setTextureSize({ textureSizeX, textureSizeY });
							inputFile >> std::ws;
							if (inputFile.peek() != ')') {
								inputFile >> token;
							}
						}
						if (token == "stateData:") {
							int virtualConnectionId, stateOffsetX, stateOffsetY;
							inputFile >> virtualConnectionId >> cToken >> cToken >> cToken >> stateOffsetX >> cToken >> stateOffsetY >> cToken;
							currentParsedCircuit->setRenderState(true);
							// currentParsedCircuit->({ smallestTextureCordX, smallestTextureCordY });
							// currentParsedCircuit->setTextureSize({ textureSizeX, textureSizeY });
							logError("StateData not implemented yet!", "ConnectionMachineParser");
						}
					}
				} else {
					logError("Invalid renderData type \"{}\"", "ConnectionMachineParser", token);
				}
				inputFile >> cToken;
			}
		} else if (token == "blockId") {
			// block id
			int blockId;
			std::string blockTypeStr;
			float posX, posY;
			inputFile >> blockId >> std::quoted(blockTypeStr);
			BlockType blockType = stringToBlockType(blockTypeStr);

			if (blockType == BlockType::CUSTOM) {
				Circuit* circuit = circuitManager.getCircuit(blockTypeStr);
				if (circuit) {
					blockType = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuit->getCircuitId())->getBlockType();
				} else {
					SharedProceduralCircuit proceduralCircuit = circuitManager.getProceduralCircuitManager().getProceduralCircuit(blockTypeStr);
					if (proceduralCircuit) {
						blockType = proceduralCircuit->getBlockType(ProceduralCircuitParameters(inputFile));
					} else if (blockTypeStr == "Bus") {
						std::vector<BlockDataManager::BusConnectionData> busConnections;
						char cToken;
						inputFile >> cToken;
						if (cToken != '(') {
							// inputFile.unget(); // Makes the stream be unchanged from when it was passed in
							return circuitIds;
						}
						while (true) {
							inputFile >> cToken;
							if (cToken != ',') {
								if (cToken == ')') break;
								inputFile.unget();
							}
							int portPosX = 0;
							int portPosY = 0;
							inputFile >> portPosX >> portPosY >> cToken;
							if (cToken == '[') {
								std::vector<unsigned int> bits;
								while (true) {
									int bit = 0;
									inputFile >> cToken;
									if (cToken == ']') break;
									inputFile.unget();
									inputFile >> bit;
									if (bit >= 0) {
										bits.push_back(bit);
									} else {
										logError("Invalid bit {} loaded from file.", "ConnectionMachineParser", bit);
									}
								}
								busConnections.push_back(BlockDataManager::BusConnectionData(Vector(portPosX, portPosY), bits));
							} else {
								inputFile.unget();
								unsigned int bitWidth = 0;
								inputFile >> bitWidth;
								if (bitWidth != 0) {
									busConnections.emplace_back(Vector(portPosX, portPosY), bitWidth);
									blockType = circuitManager.getBlockDataManager().getBusBlock(bitWidth);
								} else {
									logError("Invalid bit width {} loaded from file.", "ConnectionMachineParser", bitWidth);
									return circuitIds;
								}
							}
						}
						blockType = circuitManager.getBlockDataManager().getBusBlock(busConnections);
					} else {
						logError("Could not find Circuit or ProceduralCircuit or Bus with UUID: {}", "ConnectionMachineParser", blockTypeStr);
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
		if (version != latestVersion) {
			currentParsedCircuit->setOldFileVersion(true);
		}
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

	outputFile << "version_8\n";

	// find all required imports
	// not ideal but if we loop through from maxBlockId down then we will find all dependencies across every circuit, not just this one
	std::unordered_set<BlockType> imports;
	std::set<std::string> pathImports;
	std::map<std::string, std::set<std::string>> inFileDependencies;
	for (const std::string& UUID : fileData.UUIDs) {
		Circuit* circuit = circuitManager.getCircuit(UUID);
		if (!circuit) continue;
		const BlockContainer& blockContainer = circuit->getBlockContainer();
		for (auto itr = blockContainer.begin(); itr != blockContainer.end(); ++itr) {
			BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(itr->second.type());
			if (!blockData) {
				logError("Could not find block data for block {}", "ConnectionMachineParser", std::to_string(itr->second.type()));
				continue;
			}
			if (blockData->isPrimitive() || !imports.insert(blockData->getBlockType()).second) continue;
			circuit_id_t subCircuitId = circuitManager.getCircuitBlockDataManager().getCircuitId(blockData->getBlockType());
			const CircuitBlockData* subCircuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(subCircuitId);
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
				Circuit* circuit = circuitManager.getCircuit(subCircuitId);
				subUUID = &(circuit->getUUID());
			}
			subSavePath = circuitFileManager.getSavePath(*subUUID);
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
		Circuit* circuit = circuitManager.getCircuit(UUID);
		if (!circuit) continue;
		;
		const BlockContainer& blockContainer = circuit->getBlockContainer();
		const CircuitBlockData* circuitBlockData = circuitManager.getCircuitBlockDataManager().getCircuitBlockData(circuit->getCircuitId());
		outputFile << "Circuit: \"" << circuit->getCircuitName() << "\"\n";
		outputFile << "UUID: " << circuit->getUUID() << "\n";
		if (circuitBlockData) {
			BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(circuitBlockData->getBlockType());
			if (blockData->getRenderDataSize() != 0) {
				outputFile << "renderData:\n";
				for (const BlockData::RenderDataType& renderData : blockData->getRenderData()) {
					outputFile << "\t(\n";
					if (std::holds_alternative<BlockData::BlockTextureData>(renderData)) {
						const BlockData::BlockTextureData& blockTextureData = std::get<BlockData::BlockTextureData>(renderData);
						outputFile << "\ttexture: " << std::quoted(blockTextureData.path) << "\n";
						if (blockTextureData.useFullTexture) {
							outputFile << "\ttextureTileData: " << blockTextureData.topLeft.toString() << ", " << blockTextureData.size.toString() << "\n";
						}
						if (blockTextureData.renderState) {
							outputFile << "\tstateData: " << blockTextureData.virtualConnectionId.get() << ", " << blockTextureData.stateOffset.toString() << "\n";
						}
					}
					outputFile << "\t)\n";
				}
			}

			outputFile << "size: " << blockData->getSize().toString() << "\n";
			outputFile << "ports:\n";
			for (auto pair : blockData->getConnections()) {
				const Position* position = circuitBlockData->getConnectionIdToPosition(pair.first);
				std::optional<std::string> name = blockData->getConnectionIdToName(pair.first);
				if (!name) name = "";
				outputFile << "\t(" << (pair.second.portType == BlockData::ConnectionData::PortType::INPUT ? "IN, " : "OUT, ") << std::to_string(pair.first) << ", ";
				if (position) outputFile << fmt::to_string(*position);
				else outputFile<< "NONE";
				outputFile << ", " << pair.second.positionOnBlock.toString() << ", " << pair.second.portOffset.toString() << ", \"" << *name << "\", " << std::to_string(pair.second.getBitWidth()) << ")\n";
			}
		}

		for (auto itr = blockContainer.begin(); itr != blockContainer.end(); ++itr) {
			const Block& block = itr->second;
			Position pos = block.getPosition();

			const BlockData* blockData = circuitManager.getBlockDataManager().getBlockData(block.type());
			std::string blockTypeStr;
			if (blockData->isBus()) {
				blockTypeStr = "Bus (";
				bool first = true;
				for (const std::pair<connection_end_id_t, BlockData::ConnectionData>& connectionData : blockData->getConnections()) {
					if (first) first = false;
					else {
						blockTypeStr += ",";
					}
					blockTypeStr += " ";
					blockTypeStr += std::to_string(connectionData.second.positionOnBlock.dx) + " " + std::to_string(connectionData.second.positionOnBlock.dy) + " ";
					if (std::holds_alternative<unsigned int>(connectionData.second.bitConfiguration)) {
						blockTypeStr += std::to_string(std::get<unsigned int>(connectionData.second.bitConfiguration));
					} else {
						blockTypeStr += "[";
						for (unsigned int bit : std::get<std::vector<unsigned int>>(connectionData.second.bitConfiguration)) {
							blockTypeStr += " " + std::to_string(bit);
						}
						blockTypeStr += " ]";
					}
				}
				blockTypeStr += ")";
			} else if (!blockData->isPrimitive()) {
				circuit_id_t subCircuitId = circuitManager.getCircuitBlockDataManager().getCircuitId(block.type());
				const std::optional<std::string>& proceduralCircuitUUID =
					circuitManager.getCircuitBlockDataManager().getCircuitBlockData(subCircuitId)->getProceduralCircuitUUID();
				if (proceduralCircuitUUID.has_value()) {
					const ProceduralCircuitParameters* proceduralCircuitParameters =
						circuitManager.getProceduralCircuitManager().getProceduralCircuit(proceduralCircuitUUID.value())->getProceduralCircuitParameters(subCircuitId);

					blockTypeStr = '"' + proceduralCircuitUUID.value() + "\" " + (proceduralCircuitParameters->toString());
				} else {
					const Circuit* circuit = circuitManager.getCircuit(subCircuitId);
					blockTypeStr = '"' + circuit->getUUID() + '"';
				}
			} else {
				blockTypeStr = blockTypeToString(block.type());
			}

			outputFile << "blockId " << itr->first << ' ' << blockTypeStr << ' ' << pos.x << ' ' << pos.y << ' ' << orientationToString(block.getOrientation()) << '\n';
			const ConnectionContainer& connectionContainer = block.getConnectionContainer();

			std::vector<connection_end_id_t> connectionIds;
			for (auto& connectionIter : connectionContainer.getConnections()) {
				connectionIds.push_back(connectionIter.first);
			}
			std::sort(connectionIds.begin(), connectionIds.end());
			for (connection_end_id_t connectionId : connectionIds) {
				outputFile << '\t' << "(connId:" << std::to_string(connectionId) << ')';
				std::vector<ConnectionEnd> connections;
				for (ConnectionEnd conn : *connectionContainer.getConnections(connectionId)) {
					connections.push_back(conn);
				}
				std::sort(connections.begin(), connections.end());
				for (ConnectionEnd conn : connections) {
					outputFile << " (" << conn.getBlockId() << ' ' << std::to_string(conn.getConnectionId()) << ')';
				}
				outputFile << '\n';
			}
		}
	}
	outputFile.close();
	return true;
}
