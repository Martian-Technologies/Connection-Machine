#include "BLIFParser.h"

#include "util/algorithm.h"

std::vector<circuit_id_t> BLIFParser::load(const std::string& path) {
	// Check for cyclic import
	if (importedFiles.find(path) != importedFiles.end()) {
		logError("Cyclic import detected: " + path, "BLIFParser");
		return {};
	}
	importedFiles.insert(path);
	logInfo("Parsing BLIF Circuit File (.cir)", "BLIFParser");

	std::ifstream inputFile(path, std::ios::in | std::ios::binary);
	if (!inputFile.is_open()) {
		logError("Couldn't open file at path: " + path, "BLIFParser");
		return {};
	}

	logInfo("Inserted current file as a dependency: " + path, "BLIFParser");

	std::string token;
	char cToken;

	std::map<std::string, std::set<std::string>> dependencies;
	std::set<std::string>* curDependencies = nullptr;
	BLIFParsedCircuitData current;

	while (inputFile >> token) {
		if (token == ".search") {
			std::string importFileName;
			inputFile >> std::quoted(importFileName);
			std::filesystem::path fullPath = std::filesystem::absolute(std::filesystem::path(path)).parent_path() / importFileName;
			const std::string& fPath = fullPath.generic_string();
			load(fPath);
		} else if (token == ".end") {
			if (current.parsedCircuit) {
				BLIFParsedCircuits.emplace(current.parsedCircuit->getName(), std::move(current));
				current.parsedCircuit = nullptr;
			}
		} else if (token == ".model") {
			if (current.parsedCircuit) {
				BLIFParsedCircuits.emplace(current.parsedCircuit->getName(), std::move(current));
				current.parsedCircuit = nullptr;
			}
			current.nameToConnectionEnd.clear();
			current.connectionsToMake.clear();
			current.endId = 0;
			current.inPortY = 0;
			current.outPortY = 0;
			current.blockIdCounter = 0;
			current.parsedCircuit = std::make_shared<ParsedCircuit>();
			current.parsedCircuit->markAsCustom();
			current.parsedCircuit->setAbsoluteFilePath(path);
			std::string circuitName;
			inputFile >> circuitName;
			current.parsedCircuit->setName(circuitName);
			curDependencies = &(dependencies[circuitName]);
				logInfo("\tFound circuit: {}", "BLIFParser", circuitName);
		} else if (token == ".inputs") {
			std::getline(inputFile, token);
			std::istringstream ss(token);
			while (ss >> token) {
				if (token.front() == '#') break;
				current.parsedCircuit->addBlock(++current.blockIdCounter, BlockType::SWITCH);
				current.nameToConnectionEnd.try_emplace(token, current.blockIdCounter, 0);
				current.parsedCircuit->addConnectionPort(true, current.endId++, Vector(0, current.inPortY++), current.blockIdCounter, 0, token);
			}
		} else if (token == ".outputs") {
			std::getline(inputFile, token);
			std::istringstream ss(token);
			while (ss >> token) {
				if (token.front() == '#') break;
				current.parsedCircuit->addBlock(++current.blockIdCounter, BlockType::LIGHT);
				current.parsedCircuit->addConnectionPort(false, current.endId++, Vector(1, current.outPortY++), current.blockIdCounter, 0, token);
			}
		} else if (token == ".names") {
			std::getline(inputFile, token);
			std::istringstream ss(token);
			std::vector<std::string> ports;
			while (ss >> token) {
				if (token.front() == '#') break;
				ports.push_back(token);
			}
			std::string output = ports.back();
			ports.pop_back();
			inputFile >> std::ws;
			std::vector<block_id_t> gates;
			while (!inputFile.eof() && inputFile.peek() != '.') {
				inputFile >> token;
				inputFile >> cToken;
				logInfo("Found \"{}: {}\"", "BLIFParser", token, cToken);
				if (ports.size() != token.size()) {
					logError("Bad input plane \"{}\" of size {}. Should be {} bits wide.", "BLIFParser", token, token.size(), ports.size());
				}
				current.parsedCircuit->addBlock(++current.blockIdCounter, BlockType::AND);

				block_id_t blockId = current.blockIdCounter;
				gates.push_back(blockId);
				unsigned int index = 0;
				for (char c : token) {
					if (c == '1') {
						current.connectionsToMake.emplace_back(ports[index], ConnectionEnd(blockId, 0));
					} else if (c == '0') {
						current.parsedCircuit->addBlock(++current.blockIdCounter, BlockType::NOR);
						current.connectionsToMake.emplace_back(ports[index], ConnectionEnd(current.blockIdCounter, 0));
						current.parsedCircuit->addConnection(current.blockIdCounter, 1, blockId, 0);
					}
					++index;
				}
				inputFile >> std::ws;
			}
			current.parsedCircuit->addBlock(++current.blockIdCounter, BlockType::OR);
			current.nameToConnectionEnd.emplace(output, ConnectionEnd(current.blockIdCounter, 1));
			for (block_id_t gate : gates) {
				current.parsedCircuit->addConnection(gate, 1, current.blockIdCounter, 0);
			}
		} else if (token == ".subckt") {
			std::string blockName;
			inputFile >> blockName;
			curDependencies->emplace(blockName);
			std::getline(inputFile, token);
			std::istringstream ss(token);
			std::unordered_map<std::string, std::string> pairings;
			while (ss >> token) {
				if (token.front() == '#') break;
				std::vector<std::string> pair;
				stringSplitInto(token, '=', pair);
				if (pair.size() != 2) {
					logError("Failed to make pairing for IC \"{}\", \"{}\" should have exactly 1 '='.", "BLIFParser", blockName, token);
				}
				pairings.emplace(std::move(pair[0]), std::move(pair[1]));
			}
			current.customBlocksToAdd.emplace_back(std::move(blockName), std::move(pairings));
		}
	}
	if (current.parsedCircuit) {
		BLIFParsedCircuits.emplace(current.parsedCircuit->getName(), std::move(current));
	}

	std::vector<std::string> sortedDependencies;

	while (dependencies.size() > 0) {
		bool failed = true;
		for (auto depIter = dependencies.begin(); depIter != dependencies.end(); ++depIter) {
			bool doDep = true;
			for (const std::string& str : depIter->second) {
				auto iter = dependencies.find(str);
				if (iter != dependencies.end()) {
					doDep = false;
					continue;
				}
			}
			if (doDep) {
				sortedDependencies.push_back(depIter->first);
					dependencies.erase(depIter);
				failed = false;
				break;
			}
		}
		if (failed) break;
	}

	std::vector<circuit_id_t> circuitIds;

	for (const std::string& cirName : sortedDependencies) {
		BLIFParsedCircuitData& cirData = BLIFParsedCircuits.at(cirName);
		for (const auto& customBlock : cirData.customBlocksToAdd) {
			auto iter = BLIFParsedCircuits.find(customBlock.first);
			if (iter == BLIFParsedCircuits.end()) {
				logInfo("Failed to find BLIFParsedCircuitData for custom block {}", "BLIFParser", customBlock.first);
				continue;
			}
			cirData.parsedCircuit->addBlock(++cirData.blockIdCounter, iter->second.type);
			for (const ParsedCircuit::ConnectionPort& port : iter->second.parsedCircuit->getConnectionPorts()) {
				auto iter2 = customBlock.second.find(port.portName);
				if (iter2 != customBlock.second.end()) {
					if (port.isInput) {
						cirData.connectionsToMake.emplace_back(iter2->second, ConnectionEnd(cirData.blockIdCounter, port.connectionEndId));
					} else {
						cirData.nameToConnectionEnd.emplace(iter2->second, ConnectionEnd(cirData.blockIdCounter, port.connectionEndId));
					}
				}
			}
		}
		for (const auto& connection : cirData.connectionsToMake) {
			auto iter = cirData.nameToConnectionEnd.find(connection.first);
			if (iter == cirData.nameToConnectionEnd.end()) {
				logError("Failed to make connection. Port \"{}\" was never defined.", "BLIFParser", connection.first);
				continue;
			}
			cirData.parsedCircuit->addConnection(
				iter->second.getBlockId(),
				iter->second.getConnectionId(),
				connection.second.getBlockId(),
				connection.second.getConnectionId()
			);
		}
		for (const auto& port : cirData.parsedCircuit->getConnectionPorts()) {
			if (port.isInput) continue;
			auto iter = cirData.nameToConnectionEnd.find(port.portName);
			if (iter == cirData.nameToConnectionEnd.end()) {
				logError("Failed to make connection. Port \"{}\" was never defined.", "BLIFParser", port.portName);
				continue;
			}
			cirData.parsedCircuit->addConnection(
				iter->second.getBlockId(),
				iter->second.getConnectionId(),
				port.internalBlockId,
				port.internalBlockConnectionEndId
			);
		}
		circuit_id_t id = loadParsedCircuit(*cirData.parsedCircuit);
		circuitIds.push_back(id);
		cirData.type = circuitManager->getCircuit(id)->getBlockType();
	}
	inputFile.close();
	importedFiles.erase(path);
	return circuitIds;
}
