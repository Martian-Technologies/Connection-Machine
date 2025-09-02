#include "circuitFileManager.h"

#include "BLIFParser.h"
#include "connectionMachineParser.h"

CircuitFileManager::CircuitFileManager(CircuitManager* circuitManager) : circuitManager(circuitManager) { }

std::vector<circuit_id_t> CircuitFileManager::loadFromFile(const std::string& path) {
	auto iter = filePathToFile.find(path);
	if (iter != filePathToFile.end()) {
		logInfo("Duplicate import detected. skipping file: " + path, "CircuitFileManager");
		std::vector<circuit_id_t> circuitIds;
		for (const std::string& uuid : iter->second.UUIDs) {
			SharedCircuit circuit = circuitManager->getCircuit(uuid);
			if (circuit) {
				circuitIds.push_back(circuit->getCircuitId());
			}
		}
		return circuitIds;
	}

	if (path.size() >= 4 && path.substr(path.size() - 4) == ".cir") {
		// our Connection Machine file parser function
		ConnectionMachineParser parser(this, circuitManager);

		std::vector<circuit_id_t> circuits = parser.load(path);
		if (circuits.empty()) {
			logWarning("No circuits loaded from {}. This may be a error", "CircuitFileManager", path);
		}
		return circuits;
	} else if (path.size() >= 5 && path.substr(path.size() - 5) == ".blif") {
		SharedParsedCircuit parsedCircuit = std::make_shared<ParsedCircuit>();
		// open circuit file parser function
		BLIFParser parser(this, circuitManager);
		std::vector<circuit_id_t> circuits = parser.load(path);
		if (circuits.empty()) {
			logWarning("No circuits loaded from {}. This may be a error", "CircuitFileManager", path);
		}
		return circuits;
	} else if ((path.size() >= 4 && path.substr(path.size() - 4) == ".wat") || (path.size() >= 5 && path.substr(path.size() - 5) == ".wasm")) {
		std::optional<wasmtime::Module> module = Wasm::loadModule(path);
		if (module) {
			const std::string* UUID = circuitManager->getProceduralCircuitManager()->createWasmProceduralCircuit(module.value());
			if (UUID) {
				setSaveFilePath(*UUID, std::filesystem::absolute(std::filesystem::path(path)).generic_string());
			}
		} else {
			logError("Failed to load wasm module", "CircuitFileManager");
		}
	} else {
		logError("Unsupported file extension \"{}\". Expected .cir, .blif, .wat, or .wasm", "CircuitFileManager", std::filesystem::path(path).extension().generic_string());
	}
	return {};
}

bool CircuitFileManager::saveToFile(const std::string& path, const std::string& UUID) {
	// Doesn't check if the file is saved, we are just saving as
	setSaveFilePath(UUID, path);
	ConnectionMachineParser saver(this, circuitManager);
	if (saver.save(filePathToFile.at(path), true)) {
		logInfo("Successfully saved to: {}", "CircuitFileManager", path);
		return true;
	}
	return false;
}

bool CircuitFileManager::save(const std::string& UUID) {
	std::map<std::string, std::string>::iterator iter = UUIDToFilePath.find(UUID);
	if (iter == UUIDToFilePath.end()) {
		logInfo("Can not save: {}. No file set", "CircuitFileManager", UUID);
		return false;
	}

	FileData& fileData = filePathToFile.at(iter->second);
	SharedCircuit circuit = circuitManager->getCircuit(UUID);
	if (circuit) {
		unsigned long long currentEditCount = circuit->getEditCount();
		unsigned long long lastSaved = fileData.lastSavedEdit.at(UUID);
		if (lastSaved >= currentEditCount) {
			logInfo("No changes to save ({})", "CircuitFileManager", iter->second);
			return true;
		}
		// fileData.lastSavedEdit[UUID] = currentEditCount; // Should this be here? Move into ConnectionMachineParser?
	} else {
		SharedProceduralCircuit proceduralCircuit = circuitManager->getProceduralCircuitManager()->getProceduralCircuit(UUID);
		if (!proceduralCircuit) {
			logError("Save failed! No Circuit or ProceduralCircuit with UUID {}", "CircuitFileManager", UUID); // this should not happen
			return false;
		}
	}

	ConnectionMachineParser saver(this, circuitManager);
	if (saver.save(fileData, false)) {
		for (auto& pair : fileData.lastSavedEdit) {
			SharedCircuit savedCircuit = circuitManager->getCircuit(pair.first);
			if (savedCircuit) pair.second = savedCircuit->getEditCount();
		}
		logInfo("Successfully saved to: {}", "CircuitFileManager", iter->second);
		return true;
	}
	return false;
}

// bool CircuitFileManager::saveAllDependencies(const std::string& UUID) {
// 	const BlockContainer* blockContainer = circuitManager->getCircuit(circuitId)->getBlockContainer();
// 	const CircuitBlockDataManager* circuitBlockDataManager = circuitManager->getCircuitBlockDataManager();
// 	for (const std::pair<block_id_t, Block>& iter : *blockContainer) {
// 		// could check if it primitive first but shouldn't need to
// 		circuit_id_t id = circuitBlockDataManager->getCircuitId(iter.second.type());
// 		if (id != 0) {
// 			if (!saveCircuit(id)) {
// 				logWarning("Failed to save subcircuit: {}", "CircuitFileManager", id);
// 				return false;
// 			}
// 		}
// 	}
// 	return true;
// }

// bool CircuitFileManager::saveAsMultiFile(const std::unordered_set<std::string>& UUIDs, const std::string& fileLocation) {
//     ConnectionMachineParser saver(this, circuitManager);
//     FileData fileData(fileLocation);
//     fileData.circuitIds = circuits; // put all circuits in here, and the saver will save as a single mulit-circuit file
//     if (saver.save(fileData, true)) {
// 		logInfo("Successfully saved Circuit to: {}", "CircuitFileManager", fileLocation);
// 		return true;
//     }
//     return false;
// }

// bool CircuitFileManager::saveAsNewProject(const std::unordered_set<std::string>& UUIDs, const std::string& fileLocationPrefix) {
// 	ConnectionMachineParser saver(this, circuitManager);
//
// 	FileData fileData(fileLocationPrefix);
// 	int untitled = 1;
//
// 	std::filesystem::path prefixPath(fileLocationPrefix);
// 	if (!fileLocationPrefix.empty() && fileLocationPrefix.back() != std::filesystem::path::preferred_separator) {
// 		prefixPath += std::filesystem::path::preferred_separator;
// 	}
//
// 	for (circuit_id_t id : circuits) {
// 		fileData.circuitIds = { id }; // isolate each circuit id and save them individually
// 		std::map<circuit_id_t, std::string>::iterator itr = circuitIdToFilePath.find(id);
// 		if (itr == circuitIdToFilePath.end()) {
// 			// give default name
// 			fileData.fileLocation = (prefixPath / ("Untitled_" + std::to_string(untitled++))).generic_string();
// 		} else {
// 			// get the name that we loaded the circuit in as
// 			fileData.fileLocation = (prefixPath / std::filesystem::path(itr->second).filename()).generic_string();
// 		}
// 		if (saver.save(fileData, true)) {
// 			logInfo("Successfully saved Circuit to: {}", "CircuitFileManager", fileLocationPrefix);
// 		} else {
// 			return false;
// 		}
// 	}
// 	return true;
// }

void CircuitFileManager::setSaveFilePath(const std::string& UUID, const std::string& fileLocation) {
	std::map<std::string, FileData>::iterator iter = filePathToFile.find(fileLocation);
	if (iter == filePathToFile.end()) {
		iter = filePathToFile.emplace(fileLocation, fileLocation).first;
	} else {
		if (iter->second.UUIDs.contains(UUID)) return;
	}
	iter->second.UUIDs.emplace(UUID);
	SharedCircuit circuit = circuitManager->getCircuit(UUID);
	if (circuit) {
		iter->second.lastSavedEdit[UUID] = 0;
	}

	std::map<std::string, std::string>::iterator iter2 = UUIDToFilePath.find(UUID);
	if (iter2 != UUIDToFilePath.end()) {
		filePathToFile.at(iter2->second).UUIDs.erase(UUID);
		if (circuit) filePathToFile.at(iter2->second).lastSavedEdit.erase(UUID);
		iter2->second = fileLocation;
	} else {
		UUIDToFilePath[UUID] = fileLocation;
	}
}

const std::string* CircuitFileManager::getSavePath(const std::string& UUID) const {
	auto iter = UUIDToFilePath.find(UUID);
	if (iter == UUIDToFilePath.end()) return nullptr;
	return &(iter->second);
}

circuit_id_t CircuitFileManager::loadParsedCircuit(ParsedCircuit& parsedCircuit) {
	CircuitValidator validator(parsedCircuit, circuitManager->getBlockDataManager());
	if (!parsedCircuit.isValid()) {
		return 0;
	}
	circuit_id_t id = circuitManager->createNewCircuit(parsedCircuit);
	if (parsedCircuit.getAbsoluteFilePath() != "") {
		setSaveFilePath(circuitManager->getCircuit(id)->getUUID(), parsedCircuit.getAbsoluteFilePath());
	}

	return id; // 0 if circuit creation failed
}
