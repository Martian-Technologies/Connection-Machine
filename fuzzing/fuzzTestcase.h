#ifndef fuzzTestcase_h
#define fuzzTestcase_h

#include "backend/position/position.h"
#include "backend/container/block/blockDefs.h"
#include "backend/evaluator/simulator/logicState.h"
#include "computerAPI/circuits/textParser.h"

#include <nlohmann/json.hpp>

class Environment;

struct PlaceBlockAction {
	Position position;
	int fuzzBlockTypeIndex;
	Orientation orientation;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "PlaceBlockAction";
		j["position"] = { {"x", position.x}, {"y", position.y} };
		j["fuzzBlockTypeIndex"] = fuzzBlockTypeIndex;
		j["orientation"] = { {"rotation", static_cast<uint8_t>(orientation.rotation)}, {"flipped", orientation.flipped} };
		return j;
	}
	static PlaceBlockAction fromJson(const nlohmann::json& j) {
		PlaceBlockAction action;
		action.position = Position(j["position"]["x"], j["position"]["y"]);
		action.fuzzBlockTypeIndex = j["fuzzBlockTypeIndex"];
		action.orientation = Orientation(static_cast<Rotation>(j["orientation"]["rotation"]), j["orientation"]["flipped"]);
		return action;
	}
};

struct RemoveBlockAction {
	Position position;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "RemoveBlockAction";
		j["position"] = { {"x", position.x}, {"y", position.y} };
		return j;
	}
	static RemoveBlockAction fromJson(const nlohmann::json& j) {
		RemoveBlockAction action;
		action.position = Position(j["position"]["x"], j["position"]["y"]);
		return action;
	}
};

struct CreateConnectionAction {
	Position source;
	Position destination;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "CreateConnectionAction";
		j["source"] = { {"x", source.x}, {"y", source.y} };
		j["destination"] = { {"x", destination.x}, {"y", destination.y} };
		return j;
	}
	static CreateConnectionAction fromJson(const nlohmann::json& j) {
		CreateConnectionAction action;
		action.source = Position(j["source"]["x"], j["source"]["y"]);
		action.destination = Position(j["destination"]["x"], j["destination"]["y"]);
		return action;
	}
};

struct RemoveConnectionAction {
	Position source;
	Position destination;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "RemoveConnectionAction";
		j["source"] = { {"x", source.x}, {"y", source.y} };
		j["destination"] = { {"x", destination.x}, {"y", destination.y} };
		return j;
	}
	static RemoveConnectionAction fromJson(const nlohmann::json& j) {
		RemoveConnectionAction action;
		action.source = Position(j["source"]["x"], j["source"]["y"]);
		action.destination = Position(j["destination"]["x"], j["destination"]["y"]);
		return action;
	}
};

struct SetBlockStateAction {
	Position position;
	logic_state_t state;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "SetBlockStateAction";
		j["position"] = { {"x", position.x}, {"y", position.y} };
		j["state"] = static_cast<uint8_t>(state);
		return j;
	}
	static SetBlockStateAction fromJson(const nlohmann::json& j) {
		SetBlockStateAction action;
		action.position = Position(j["position"]["x"], j["position"]["y"]);
		action.state = static_cast<logic_state_t>(j["state"]);
		return action;
	}
};

struct TickEvalAction {
	unsigned int numTicks;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "TickEvalAction";
		j["numTicks"] = numTicks;
		return j;
	}
	static TickEvalAction fromJson(const nlohmann::json& j) {
		TickEvalAction action;
		action.numTicks = j["numTicks"];
		return action;
	}
};

struct FuzzEditAction {
	std::variant<
		PlaceBlockAction,
		RemoveBlockAction,
		CreateConnectionAction,
		RemoveConnectionAction
	> action;
	unsigned int circuitIndex;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["circuitIndex"] = circuitIndex;
		std::visit([&j](auto&& arg) {
			j["action"] = arg.toJson();
		}, action);
		return j;
	}
	static FuzzEditAction fromJson(const nlohmann::json& j) {
		FuzzEditAction editAction;
		editAction.circuitIndex = j["circuitIndex"];
		const nlohmann::json& actionJson = j["action"];
		std::string type = actionJson["type"];
		if (type == "PlaceBlockAction") {
			editAction.action = PlaceBlockAction::fromJson(actionJson);
		} else if (type == "RemoveBlockAction") {
			editAction.action = RemoveBlockAction::fromJson(actionJson);
		} else if (type == "CreateConnectionAction") {
			editAction.action = CreateConnectionAction::fromJson(actionJson);
		} else if (type == "RemoveConnectionAction") {
			editAction.action = RemoveConnectionAction::fromJson(actionJson);
		}
		return editAction;
	}
};

// using FuzzEditAction = std::variant<
// 	PlaceBlockAction,
// 	RemoveBlockAction,
// 	CreateConnectionAction,
// 	RemoveConnectionAction
// >;

// using FuzzTestAction = std::variant<
// 	SetBlockStateAction,
// 	TickEvalAction
// >;
struct FuzzTestAction {
	std::variant<
		SetBlockStateAction,
		TickEvalAction
	> action;
	nlohmann::json toJson() const {
		nlohmann::json j;
		std::visit([&j](auto&& arg) {
			j = arg.toJson();
		}, action);
		return j;
	}
	static FuzzTestAction fromJson(const nlohmann::json& j) {
		FuzzTestAction testAction;
		std::string type = j["type"];
		if (type == "SetBlockStateAction") {
			testAction.action = SetBlockStateAction::fromJson(j);
		} else if (type == "TickEvalAction") {
			testAction.action = TickEvalAction::fromJson(j);
		}
		return testAction;
	}
};

struct FuzzPrimitiveType {
	BlockType blockType;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "FuzzPrimitiveType";
		j["name"] = blockTypeToString(blockType);
		return j;
	}
	static FuzzPrimitiveType fromJson(const nlohmann::json& j) {
		FuzzPrimitiveType primitiveType;
		primitiveType.blockType = stringToBlockType(j["name"]);
		return primitiveType;
	}
};

struct FuzzBusType {
	unsigned int numInputs;
	unsigned int numOutputs;
	unsigned int inputLaneWidth;
	unsigned int outputLaneWidth;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "FuzzBusType";
		j["numInputs"] = numInputs;
		j["numOutputs"] = numOutputs;
		j["inputLaneWidth"] = inputLaneWidth;
		j["outputLaneWidth"] = outputLaneWidth;
		return j;
	}
	static FuzzBusType fromJson(const nlohmann::json& j) {
		FuzzBusType busType;
		busType.numInputs = j["numInputs"];
		busType.numOutputs = j["numOutputs"];
		busType.inputLaneWidth = j["inputLaneWidth"];
		busType.outputLaneWidth = j["outputLaneWidth"];
		return busType;
	}
};

struct FuzzCustomCircuitType {
	std::string path;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "FuzzCustomCircuitType";
		j["path"] = path;
		return j;
	}
	static FuzzCustomCircuitType fromJson(const nlohmann::json& j) {
		FuzzCustomCircuitType customType;
		customType.path = j["path"];
		return customType;
	}
};

// using FuzzBlockType = std::variant<
// 	FuzzPrimitiveType,
// 	FuzzBusType,
// 	FuzzCustomCircuitType
// >;
struct FuzzBlockType {
	std::variant<
		FuzzPrimitiveType,
		FuzzBusType,
		FuzzCustomCircuitType
	> type;
	nlohmann::json toJson() const {
		nlohmann::json j;
		std::visit([&j](auto&& arg) {
			j = arg.toJson();
		}, type);
		return j;
	}
	static FuzzBlockType fromJson(const nlohmann::json& j) {
		FuzzBlockType fuzzBlockType;
		std::string typeStr = j["type"];
		if (typeStr == "FuzzPrimitiveType") {
			fuzzBlockType.type = FuzzPrimitiveType::fromJson(j);
		} else if (typeStr == "FuzzBusType") {
			fuzzBlockType.type = FuzzBusType::fromJson(j);
		} else if (typeStr == "FuzzCustomCircuitType") {
			fuzzBlockType.type = FuzzCustomCircuitType::fromJson(j);
		}
		return fuzzBlockType;
	}
};

BlockType getBlockTypeFromFuzzBlockType(const FuzzBlockType& fuzzBlockType, Environment& environment);
std::vector<BlockType> makeBlockTypesUsableVector(const std::vector<FuzzBlockType>& fuzzBlockTypes, Environment& environment);

class FuzzTestcase {
public:
	FuzzTestcase(std::vector<FuzzBlockType> blockTypesUsed) : blockTypesUsed(blockTypesUsed) {}

	const std::vector<FuzzBlockType>& getBlockTypesUsed() const {
		return blockTypesUsed;
	}

	void addEditAction(const FuzzEditAction& action) {
		editActions.push_back(action);
	}

	void addTestAction(const FuzzTestAction& action) {
		testActions.push_back(action);
	}

	const std::vector<FuzzEditAction>& getEditActions() const {
		return editActions;
	}

	const std::vector<FuzzTestAction>& getTestActions() const {
		return testActions;
	}

	void removeEditAction(size_t index) {
		if (index < editActions.size()) {
			editActions.erase(editActions.begin() + index);
		}
	}

	void removeTestAction(size_t index) {
		if (index < testActions.size()) {
			testActions.erase(testActions.begin() + index);
		}
	}

	bool getRunRealistic() const {
		return runRealistic;
	}

	void setRealistic(bool realistic) {
		runRealistic = realistic;
	}

	std::string serialize() const;
	static FuzzTestcase deserialize(const std::string& data);

	void tryRemoveBlockTypesNotUsed();

private:
	std::vector<FuzzBlockType> blockTypesUsed;
	std::vector<FuzzEditAction> editActions;
	std::vector<FuzzTestAction> testActions;
	bool runRealistic;
};

#endif /* fuzzTestcase_h */