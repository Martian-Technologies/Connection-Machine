#include "textParser.h"
#include "backend/evaluator/simulator/logicState.h"

BlockType stringToBlockType(const std::string& str) {
	if (str == "NONE") return BlockType::NONE;
	if (str == "AND") return BlockType::AND;
	if (str == "OR") return BlockType::OR;
	if (str == "XOR") return BlockType::XOR;
	if (str == "NAND") return BlockType::NAND;
	if (str == "NOR") return BlockType::NOR;
	if (str == "XNOR") return BlockType::XNOR;
	if (str == "BUFFER") return BlockType::BUFFER;
	if (str == "NOT") return BlockType::NOT;
	if (str == "JUNCTION") return BlockType::JUNCTION;
	if (str == "JUNCTION_L") return BlockType::JUNCTION_L;
	if (str == "JUNCTION_H") return BlockType::JUNCTION_H;
	if (str == "JUNCTION_X") return BlockType::JUNCTION_X;
	if (str == "TRISTATE_BUFFER") return BlockType::TRISTATE_BUFFER;
	if (str == "BUTTON") return BlockType::BUTTON;
	if (str == "TICK_BUTTON") return BlockType::TICK_BUTTON;
	if (str == "SWITCH") return BlockType::SWITCH;
	if (str == "CONSTANT_OFF") return BlockType::CONSTANT_OFF;
	if (str == "CONSTANT_ON") return BlockType::CONSTANT_ON;
	if (str == "CONSTANT_Z") return BlockType::CONSTANT_Z;
	if (str == "CONSTANT_X") return BlockType::CONSTANT_X;
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

std::optional<logic_state_t> stringToLogicState(const std::string& str) {
	if (str == "LOW" || str == "L" || str == "l" || str == "0") return (logic_state_t)0;
	if (str == "HIGH" || str == "H" || str == "h" || str == "1") return (logic_state_t)1;
	if (str == "FLOATING" || str == "Z" || str == "z" || str == "2") return (logic_state_t)2;
	if (str == "UNDEFINED" || str == "X" || str == "x" || str == "3") return (logic_state_t)3;
	return std::nullopt;
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
	case BlockType::BUFFER: return "BUFFER";
	case BlockType::NOT: return "NOT";
	case BlockType::JUNCTION: return "JUNCTION";
	case BlockType::JUNCTION_L: return "JUNCTION_L";
	case BlockType::JUNCTION_H: return "JUNCTION_H";
	case BlockType::JUNCTION_X: return "JUNCTION_X";
	case BlockType::TRISTATE_BUFFER: return "TRISTATE_BUFFER";
	case BlockType::BUTTON: return "BUTTON";
	case BlockType::TICK_BUTTON: return "TICK_BUTTON";
	case BlockType::SWITCH: return "SWITCH";
	case BlockType::CONSTANT_OFF: return "CONSTANT_OFF";
	case BlockType::CONSTANT_ON: return "CONSTANT_ON";
	case BlockType::CONSTANT_Z: return "CONSTANT_Z";
	case BlockType::CONSTANT_X: return "CONSTANT_X";
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
