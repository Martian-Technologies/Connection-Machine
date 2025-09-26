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
	JUNCTION,
	TRISTATE_BUFFER,
	//TIMER,
	BUTTON,
	TICK_BUTTON,
	SWITCH,
	CONSTANT,
	LIGHT,
	BUS,
	CUSTOM, // placeholder for custom blocks in parsed circuit
};

template <>
struct fmt::formatter<BlockType> : fmt::formatter<std::string> {
    auto format(BlockType blockType, format_context& ctx) const {
        return formatter<std::string>::format(to_string((unsigned int)blockType), ctx);
    }
};

#endif /* blockDefs_h */
