#ifndef blockDefs_h
#define blockDefs_h

typedef char block_data_index_t;

typedef unsigned char block_size_t;
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
	BUS_INTERFACE_1,
	BUS_INTERFACE_2,
	BUS_INTERFACE_3,
	BUS_INTERFACE_4,
	CUSTOM, // placeholder for custom blocks in parsed circuit
};

template <>
struct fmt::formatter<BlockType> : fmt::formatter<std::string> {
    auto format(BlockType blockType, format_context& ctx) const {
        return formatter<std::string>::format(to_string((unsigned int)blockType), ctx);
    }
};

#endif /* blockDefs_h */
