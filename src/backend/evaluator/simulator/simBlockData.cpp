#include "simBlockData.h"

SimBlockData::PortInfo SimBlockData::getPortInfo(
	BlockType blockType,
	connection_end_id_t connectionEndId
) {
	switch (blockType) {
		case BlockType::AND:
		case BlockType::OR:
		case BlockType::XOR:
		case BlockType::NAND:
		case BlockType::NOR:
		case BlockType::XNOR:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::INPUT, false };
			} else if (connectionEndId == 1) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for gate block type");
			break;
		case BlockType::BUFFER:
		case BlockType::NOT:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::INPUT, true };
			} else if (connectionEndId == 1) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for buffer/not block type");
			break;
		case BlockType::BUTTON:
		case BlockType::TICK_BUTTON:
		case BlockType::SWITCH:
		case BlockType::CONSTANT_OFF:
		case BlockType::CONSTANT_ON:
		case BlockType::CONSTANT_Z:
		case BlockType::CONSTANT_X:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for constant block type");
			break;
		case BlockType::JUNCTION:
		case BlockType::JUNCTION_L:
		case BlockType::JUNCTION_H:
		case BlockType::JUNCTION_X:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::BIDIRECTIONAL, false };
			}
			assert(false && "Invalid connection end ID for junction block type");
			break;
		case BlockType::TRISTATE_BUFFER:
			if (connectionEndId == 0) {
				return PortInfo { PortDirection::INPUT, true };
			} else if (connectionEndId == 1) {
				return PortInfo { PortDirection::INPUT, true };
			} else if (connectionEndId == 2) {
				return PortInfo { PortDirection::OUTPUT, false };
			}
			assert(false && "Invalid connection end ID for tristate buffer block type");
			break;
		default:
			assert(false && "Unknown block type in getPortInfo");
	}
	assert(false && "Unreachable code in getPortInfo");
	return PortInfo { PortDirection::INPUT, false };
}

SimBlockData::ConnectionDirection SimBlockData::getConnectionDirection(
	BlockType blockTypeA,
	connection_end_id_t connectionEndIdA,
	BlockType blockTypeB,
	connection_end_id_t connectionEndIdB
) {
	PortDirection portADirection = SimBlockData::getPortDirection(blockTypeA, connectionEndIdA);
	if (portADirection == PortDirection::OUTPUT) {
		return ConnectionDirection::AtoB;
	} else if (portADirection == PortDirection::INPUT) {
		return ConnectionDirection::BtoA;
	}

	PortDirection portBDirection = SimBlockData::getPortDirection(blockTypeB, connectionEndIdB);
	if (portBDirection == PortDirection::OUTPUT) {
		return ConnectionDirection::BtoA;
	} else if (portBDirection == PortDirection::INPUT) {
		return ConnectionDirection::AtoB;
	}

	assert(false && "Both ports are bidirectional in getConnectionDirection");
	return ConnectionDirection::AtoB; // to silence compiler warning
}

const std::vector<connection_end_id_t>& SimBlockData::getOutputPorts(BlockType blockType) {
	static std::vector<connection_end_id_t> zero = { connection_end_id_t(0) };
	static std::vector<connection_end_id_t> one = { connection_end_id_t(1) };
	static std::vector<connection_end_id_t> two = { connection_end_id_t(2) };
	switch (blockType) {
	case BlockType::AND:
	case BlockType::OR:
	case BlockType::XOR:
	case BlockType::NAND:
	case BlockType::NOR:
	case BlockType::XNOR:
	case BlockType::BUFFER:
	case BlockType::NOT:
		return one;
	case BlockType::BUTTON:
	case BlockType::TICK_BUTTON:
	case BlockType::SWITCH:
	case BlockType::CONSTANT_OFF:
	case BlockType::CONSTANT_ON:
	case BlockType::CONSTANT_Z:
	case BlockType::CONSTANT_X:
	case BlockType::JUNCTION:
	case BlockType::JUNCTION_L:
	case BlockType::JUNCTION_H:
	case BlockType::JUNCTION_X:
		return zero;
	case BlockType::TRISTATE_BUFFER:
		return two;
	default:
		assert(false && "Unknown block type in getOutputPorts");
		return zero;
	}
}