#ifndef fuzzTestcase_h
#define fuzzTestcase_h

#include "backend/address.h"
#include "backend/position/position.h"
#include "backend/container/block/blockDefs.h"
#include "backend/evaluator/simulator/logicState.h"
#include "computerAPI/circuits/textParser.h"
#include "backend/container/block/connectionEnd.h"

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

struct MoveBlockAction {
	Position oldPosition;
	Position newPosition;
	Orientation orientationOffset;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "MoveBlockAction";
		j["oldPosition"] = { {"x", oldPosition.x}, {"y", oldPosition.y} };
		j["newPosition"] = { {"x", newPosition.x}, {"y", newPosition.y} };
		j["orientationOffset"] = { {"rotation", static_cast<uint8_t>(orientationOffset.rotation)}, {"flipped", orientationOffset.flipped} };
		return j;
	}
	static MoveBlockAction fromJson(const nlohmann::json& j) {
		MoveBlockAction action;
		action.oldPosition = Position(j["oldPosition"]["x"], j["oldPosition"]["y"]);
		action.newPosition = Position(j["newPosition"]["x"], j["newPosition"]["y"]);
		action.orientationOffset = Orientation(static_cast<Rotation>(j["orientationOffset"]["rotation"]), j["orientationOffset"]["flipped"]);
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

struct UpdateCircuitIOAction {
	bool isInput;
	Vector portOffset;
	unsigned int bitWidth;
	connection_end_id_t connectionEndId;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "UpdateCircuitIOAction";
		j["isInput"] = isInput;
		j["portOffset"] = { {"dx", portOffset.dx}, {"dy", portOffset.dy} };
		j["bitWidth"] = bitWidth;
		j["connectionEndId"] = connectionEndId.get();
		return j;
	}
	static UpdateCircuitIOAction fromJson(const nlohmann::json& j) {
		UpdateCircuitIOAction action;
		action.isInput = j["isInput"];
		action.portOffset = Vector(j["portOffset"]["dx"], j["portOffset"]["dy"]);
		action.bitWidth = j["bitWidth"];
		action.connectionEndId = connection_end_id_t(j["connectionEndId"]);
		return action;
	}
};
struct RemoveCircuitIOAction {
	connection_end_id_t connectionEndId;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "RemoveCircuitIOAction";
		j["connectionEndId"] = connectionEndId.get();
		return j;
	}
	static RemoveCircuitIOAction fromJson(const nlohmann::json& j) {
		RemoveCircuitIOAction action;
		action.connectionEndId = connection_end_id_t(j["connectionEndId"]);
		return action;
	}
};
struct ResizeCircuitAction {
	Size newSize;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "ResizeCircuitAction";
		j["newSize"] = { {"w", newSize.w}, {"h", newSize.h} };
		return j;
	}
	static ResizeCircuitAction fromJson(const nlohmann::json& j) {
		ResizeCircuitAction action;
		action.newSize = Size(j["newSize"]["w"], j["newSize"]["h"]);
		return action;
	}
};

struct SetBlockStateAction {
	Address address;
	logic_state_t state;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "SetBlockStateAction";
		j["address"] = nlohmann::json::array();
		for (int i = 0; i < address.size(); ++i) {
			Position position = address.getPosition(i);
			j["address"].push_back({ {"x", position.x} , {"y", position.y} });
		}
		j["state"] = static_cast<uint8_t>(state);
		return j;
	}
	static SetBlockStateAction fromJson(const nlohmann::json& j) {
		SetBlockStateAction action;
		action.address = Address();
		for (int i = 0; i < j["address"].size(); ++i) {
			nlohmann::json pos = j["address"][i];
			Position position = Position(pos["x"], pos["y"]);
			action.address.addBlockId(position);
		}
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
	FuzzEditAction(PlaceBlockAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(RemoveBlockAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(MoveBlockAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(CreateConnectionAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(RemoveConnectionAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(UpdateCircuitIOAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(RemoveCircuitIOAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	FuzzEditAction(ResizeCircuitAction action, unsigned int circuitIndex) : action(action), circuitIndex(circuitIndex) {}
	std::variant <
		PlaceBlockAction,
		RemoveBlockAction,
		MoveBlockAction,
		CreateConnectionAction,
		RemoveConnectionAction,
		UpdateCircuitIOAction,
		RemoveCircuitIOAction,
		ResizeCircuitAction
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
		unsigned int cIndex = j["circuitIndex"];
		const nlohmann::json& actionJson = j["action"];
		std::string type = actionJson["type"];
		if (type == "PlaceBlockAction") {
			return { PlaceBlockAction::fromJson(actionJson), cIndex };
		} else if (type == "RemoveBlockAction") {
			return { RemoveBlockAction::fromJson(actionJson), cIndex };
		} else if (type == "MoveBlockAction") {
			return { MoveBlockAction::fromJson(actionJson), cIndex };
		} else if (type == "CreateConnectionAction") {
			return { CreateConnectionAction::fromJson(actionJson), cIndex };
		} else if (type == "RemoveConnectionAction") {
			return { RemoveConnectionAction::fromJson(actionJson), cIndex };
		} else if (type == "UpdateCircuitIOAction") {
			return { UpdateCircuitIOAction::fromJson(actionJson), cIndex };
		} else if (type == "RemoveCircuitIOAction") {
			return { RemoveCircuitIOAction::fromJson(actionJson), cIndex };
		} else if (type == "ResizeCircuitAction") {
			return { ResizeCircuitAction::fromJson(actionJson), cIndex };
		}
		logError("Unknown FuzzEditAction {}", "FuzzEditAction::fromJson", j.dump());
	}
};

struct FuzzTestAction {
	FuzzTestAction(SetBlockStateAction action) : action(action) {}
	FuzzTestAction(TickEvalAction action) : action(action) {}
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
		std::string type = j["type"];
		if (type == "SetBlockStateAction") {
			return SetBlockStateAction::fromJson(j);
		} else if (type == "TickEvalAction") {
			return TickEvalAction::fromJson(j);
		}
		logError("Unknown FuzzTestAction {}", "FuzzTestAction::fromJson", j.dump());
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

struct FuzzOtherCircuitType {
	unsigned int circuitIndex;
	nlohmann::json toJson() const {
		nlohmann::json j;
		j["type"] = "FuzzOtherCircuitType";
		j["circuitIndex"] = circuitIndex;
		return j;
	}
	static FuzzOtherCircuitType fromJson(const nlohmann::json& j) {
		FuzzOtherCircuitType otherType;
		otherType.circuitIndex = j["circuitIndex"];
		return otherType;
	}
};

struct FuzzBlockType {
	FuzzBlockType(FuzzPrimitiveType primitiveType) : type(primitiveType) {}
	FuzzBlockType(FuzzBusType busType) : type(busType) {}
	FuzzBlockType(FuzzCustomCircuitType customCircuitType) : type(customCircuitType) {}
	FuzzBlockType(FuzzOtherCircuitType otherCircuitType) : type(otherCircuitType) {}
	std::variant <
		FuzzPrimitiveType,
		FuzzBusType,
		FuzzCustomCircuitType,
		FuzzOtherCircuitType
	> type;
	nlohmann::json toJson() const {
		nlohmann::json j;
		std::visit([&j](auto&& arg) {
			j = arg.toJson();
		}, type);
		return j;
	}
	static FuzzBlockType fromJson(const nlohmann::json& j) {
		std::string typeStr = j["type"];
		if (typeStr == "FuzzPrimitiveType") {
			return FuzzPrimitiveType::fromJson(j);
		} else if (typeStr == "FuzzBusType") {
			return FuzzBusType::fromJson(j);
		} else if (typeStr == "FuzzCustomCircuitType") {
			return FuzzCustomCircuitType::fromJson(j);
		} else if (typeStr == "FuzzOtherCircuitType") {
			return FuzzOtherCircuitType::fromJson(j);
		}
		logError("Unknown FuzzBlockType {}", "FuzzBlockType::fromJson", j.dump());
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