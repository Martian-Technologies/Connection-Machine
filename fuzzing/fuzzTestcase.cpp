#include "fuzzTestcase.h"
#include "computerAPI/directoryManager.h"

namespace {
	std::atomic<unsigned long long> g_setStateFailCount { 0 };
}

void setupFuzzLogCapture() {
	setLogStdErrEnabled(false); // we re-emit below so we can drop one specific line
	setLogOutputCallback([](const std::string& line) {
		if (line.find("Failed to set sim id") != std::string::npos && line.find("EvalLogicSimulator::setState") != std::string::npos) return;
		std::cerr << line << "\n";
	});
	setLogErrorCallback([](const std::string& message, const std::string& subcategory) {
		if (message == "Failed to set sim id" && subcategory == "EvalLogicSimulator::setState") {
			g_setStateFailCount.fetch_add(1, std::memory_order_relaxed);
		}
	});
}

unsigned long long fuzzSetStateFailCount() {
	return g_setStateFailCount.load(std::memory_order_relaxed);
}

void resetFuzzSetStateFailCount() {
	g_setStateFailCount.store(0, std::memory_order_relaxed);
}

std::string FuzzTestcase::serialize() const {
	nlohmann::json j;
	j["type"] = "FuzzTestcase Eval v1";
	j["runRealistic"] = runRealistic;
	j["editActions"] = nlohmann::json::array();
	j["testActions"] = nlohmann::json::array();
	j["blockTypesUsed"] = nlohmann::json::array();
	for (const auto& action : editActions) {
		std::visit([&j](auto&& arg) {
			j["editActions"].push_back(arg.toJson());
		}, action);
	}
	for (const auto& action : testActions) {
		std::visit([&j](auto&& arg) {
			j["testActions"].push_back(arg.toJson());
		}, action);
	}
	for (const auto& blockType : blockTypesUsed) {
		std::visit([&j](auto&& arg) {
			j["blockTypesUsed"].push_back(arg.toJson());
		}, blockType);
	}
	return j.dump();
}

FuzzTestcase FuzzTestcase::deserialize(const std::string& data) {
	nlohmann::json j = nlohmann::json::parse(data);
	std::vector<FuzzBlockType> blockTypesUsed;
	for (const auto& blockTypeJson : j["blockTypesUsed"]) {
		std::string type = blockTypeJson.value("type", "");
		if (type == "FuzzPrimitiveType") {
			FuzzPrimitiveType blockType;
			blockType.blockType = stringToBlockType(blockTypeJson["name"].get<std::string>());
			blockTypesUsed.push_back(blockType);
		} else if (type == "FuzzBusType") {
			FuzzBusType blockType;
			blockType.numInputs = blockTypeJson["numInputs"].get<unsigned int>();
			blockType.numOutputs = blockTypeJson["numOutputs"].get<unsigned int>();
			blockType.inputLaneWidth = blockTypeJson["inputLaneWidth"].get<unsigned int>();
			blockType.outputLaneWidth = blockTypeJson["outputLaneWidth"].get<unsigned int>();
			blockTypesUsed.push_back(blockType);
		} else if (type == "FuzzCustomCircuitType") {
			FuzzCustomCircuitType blockType;
			blockType.path = blockTypeJson["path"].get<std::string>();
			blockTypesUsed.push_back(blockType);
		}
	}
	FuzzTestcase testcase(blockTypesUsed);
	testcase.runRealistic = j.value("runRealistic", false);
	for (const auto& actionJson : j["editActions"]) {
		std::string type = actionJson.value("type", "");
		if (type == "PlaceBlockAction") {
			PlaceBlockAction action;
			action.position = Position(actionJson["position"]["x"], actionJson["position"]["y"]);
			action.fuzzBlockTypeIndex = actionJson["fuzzBlockTypeIndex"].get<int>();
			action.orientation = Orientation(
				Rotation(actionJson["orientation"]["rotation"].get<uint8_t>()),
				actionJson["orientation"]["flipped"].get<bool>()
			);
			testcase.addEditAction(action);
		} else if (type == "RemoveBlockAction") {
			RemoveBlockAction action;
			action.position = Position(actionJson["position"]["x"], actionJson["position"]["y"]);
			testcase.addEditAction(action);
		} else if (type == "CreateConnectionAction") {
			CreateConnectionAction action;
			action.source = Position(actionJson["source"]["x"], actionJson["source"]["y"]);
			action.destination = Position(actionJson["destination"]["x"], actionJson["destination"]["y"]);
			testcase.addEditAction(action);
		} else if (type == "RemoveConnectionAction") {
			RemoveConnectionAction action;
			action.source = Position(actionJson["source"]["x"], actionJson["source"]["y"]);
			action.destination = Position(actionJson["destination"]["x"], actionJson["destination"]["y"]);
			testcase.addEditAction(action);
		}
	}
	for (const auto& actionJson : j["testActions"]) {
		std::string type = actionJson.value("type", "");
		if (type == "SetBlockStateAction") {
			SetBlockStateAction action;
			action.position = Position(actionJson["position"]["x"], actionJson["position"]["y"]);
			action.state = static_cast<logic_state_t>(actionJson["state"].get<uint8_t>());
			testcase.addTestAction(action);
		} else if (type == "TickEvalAction") {
			TickEvalAction action;
			action.numTicks = actionJson["numTicks"].get<unsigned int>();
			testcase.addTestAction(action);
		}
	}
	return testcase;
}

BlockType getBlockTypeFromFuzzBlockType(const FuzzBlockType& fuzzBlockType, Environment& environment) {
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	if (std::holds_alternative<FuzzPrimitiveType>(fuzzBlockType)) {
		return std::get<FuzzPrimitiveType>(fuzzBlockType).blockType;
	} else if (std::holds_alternative<FuzzBusType>(fuzzBlockType)) {
		return blockDataManager.getBusBlock(
			std::get<FuzzBusType>(fuzzBlockType).numInputs,
			std::get<FuzzBusType>(fuzzBlockType).numOutputs,
			std::get<FuzzBusType>(fuzzBlockType).inputLaneWidth,
			std::get<FuzzBusType>(fuzzBlockType).outputLaneWidth
		);
	} else if (std::holds_alternative<FuzzCustomCircuitType>(fuzzBlockType)) {
		CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
		circuit_id_t circuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / std::get<FuzzCustomCircuitType>(fuzzBlockType).path).string()).at(0);
		Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
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
	std::unordered_map<int, int> mapping;
	std::unordered_set<int> usedIndices;
	for (const auto& action : editActions) {
		if (std::holds_alternative<PlaceBlockAction>(action)) {
			const PlaceBlockAction& placeAction = std::get<PlaceBlockAction>(action);
			usedIndices.insert(placeAction.fuzzBlockTypeIndex);
		}
	}
	std::vector<FuzzBlockType> newBlockTypesUsed;
	int newIndex = 0;
	for (size_t i = 0; i < blockTypesUsed.size(); ++i) {
		if (usedIndices.find(i) != usedIndices.end()) {
			mapping[i] = newIndex;
			newBlockTypesUsed.push_back(blockTypesUsed[i]);
			newIndex++;
		}
	}
	blockTypesUsed = std::move(newBlockTypesUsed);
	for (FuzzEditAction& action : editActions) {
		if (std::holds_alternative<PlaceBlockAction>(action)) {
			PlaceBlockAction& placeAction = std::get<PlaceBlockAction>(action);
			placeAction.fuzzBlockTypeIndex = mapping[placeAction.fuzzBlockTypeIndex];
		}
	}
}
