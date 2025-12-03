#include "fuzzTestcase.h"
#include "computerAPI/directoryManager.h"
#include "environment/environment.h"

std::string FuzzTestcase::serialize() const {
	nlohmann::json j;
	j["type"] = "FuzzTestcase Eval v1";
	j["runRealistic"] = runRealistic;
	j["editActions"] = nlohmann::json::array();
	j["testActions"] = nlohmann::json::array();
	j["blockTypesUsed"] = nlohmann::json::array();
	for (const auto& action : editActions) {
		j["editActions"].push_back(action.toJson());
	}
	for (const auto& action : testActions) {
		j["testActions"].push_back(action.toJson());
	}
	for (const auto& blockType : blockTypesUsed) {
		j["blockTypesUsed"].push_back(blockType.toJson());
	}
	return j.dump();
}

FuzzTestcase FuzzTestcase::deserialize(const std::string& data) {
	nlohmann::json j = nlohmann::json::parse(data);
	std::vector<FuzzBlockType> blockTypesUsed;
	for (const auto& blockTypeJson : j["blockTypesUsed"]) {
		blockTypesUsed.push_back(FuzzBlockType::fromJson(blockTypeJson));
	}
	FuzzTestcase testcase(blockTypesUsed);
	testcase.runRealistic = j.value("runRealistic", false);
	for (const auto& actionJson : j["editActions"]) {
		testcase.addEditAction(FuzzEditAction::fromJson(actionJson));
	}
	for (const auto& actionJson : j["testActions"]) {
		testcase.addTestAction(FuzzTestAction::fromJson(actionJson));
	}
	return testcase;
}

BlockType getBlockTypeFromFuzzBlockType(const FuzzBlockType& fuzzBlockType, Environment& environment) {
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	auto& type = fuzzBlockType.type;
	if (std::holds_alternative<FuzzPrimitiveType>(type)) {
		return std::get<FuzzPrimitiveType>(type).blockType;
	} else if (std::holds_alternative<FuzzBusType>(type)) {
		return blockDataManager.getBusBlock(
			std::get<FuzzBusType>(type).numInputs,
			std::get<FuzzBusType>(type).numOutputs,
			std::get<FuzzBusType>(type).inputLaneWidth,
			std::get<FuzzBusType>(type).outputLaneWidth
		);
	} else if (std::holds_alternative<FuzzCustomCircuitType>(type)) {
		CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
		circuit_id_t circuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / std::get<FuzzCustomCircuitType>(type).path).string()).at(0);
		SharedCircuit circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
		return circuit->getBlockType();
	}
	return BlockType::NONE;
}

std::vector<BlockType> makeBlockTypesUsableVector(const std::vector<FuzzBlockType>& fuzzBlockTypes, Environment& environment) {
	std::vector<BlockType> blockTypes;
	for (const auto& fuzzBlockType : fuzzBlockTypes) {
		blockTypes.push_back(getBlockTypeFromFuzzBlockType(fuzzBlockType, environment));
	}
	return blockTypes;
}

void FuzzTestcase::tryRemoveBlockTypesNotUsed() {
//	std::unordered_map<int, int> mapping;
//	std::unordered_set<int> usedIndices;
//	for (const auto& action : editActions) {
//		if (std::holds_alternative<PlaceBlockAction>(action)) {
//			const PlaceBlockAction& placeAction = std::get<PlaceBlockAction>(action);
//			usedIndices.insert(placeAction.fuzzBlockTypeIndex);
//		}
//	}
//	std::vector<FuzzBlockType> newBlockTypesUsed;
//	int newIndex = 0;
//	for (size_t i = 0; i < blockTypesUsed.size(); ++i) {
//		if (usedIndices.find(i) != usedIndices.end()) {
//			mapping[i] = newIndex;
//			newBlockTypesUsed.push_back(blockTypesUsed[i]);
//			newIndex++;
//		}
//	}
//	blockTypesUsed = std::move(newBlockTypesUsed);
//	for (FuzzEditAction& action : editActions) {
//		if (std::holds_alternative<PlaceBlockAction>(action)) {
//			PlaceBlockAction& placeAction = std::get<PlaceBlockAction>(action);
//			placeAction.fuzzBlockTypeIndex = mapping[placeAction.fuzzBlockTypeIndex];
//		}
//	}
}
