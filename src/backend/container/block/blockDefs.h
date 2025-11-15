#ifndef blockDefs_h
#define blockDefs_h

typedef char block_data_index_t;

typedef std::uint8_t block_size_t;
typedef unsigned int block_id_t;

enum BlockType : std::uint16_t {
	NONE,
	AND,
	OR,
	XOR,
	NAND,
	NOR,
	XNOR,
	BUFFER,
	NOT,
	JUNCTION,
	JUNCTION_L,
	JUNCTION_H,
	JUNCTION_X,
	TRISTATE_BUFFER,
	BUTTON,
	TICK_BUTTON,
	SWITCH,
	CONSTANT_OFF,
	CONSTANT_ON,
	CONSTANT_Z,
	CONSTANT_X,
	LIGHT,
	COLOR_LIGHT,
	CUSTOM, // placeholder for custom blocks in parsed circuit
};

inline std::string blocktype_to_string(const BlockType blockType) {
	switch (blockType) {
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
	case BlockType::COLOR_LIGHT: return "COLOR_LIGHT";
	default: return std::to_string(blockType);
	}
}

template <>
struct fmt::formatter<BlockType> : fmt::formatter<std::string> {
    auto format(BlockType blockType, format_context& ctx) const {
        return formatter<std::string>::format(blocktype_to_string(blockType), ctx);
    }
};

#endif /* blockDefs_h */
