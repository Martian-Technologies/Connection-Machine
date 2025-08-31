#ifndef mainRendererDefs_h
#define mainRendererDefs_h

#include "backend/position/position.h"
#include "backend/container/block/blockDefs.h"
#include "backend/selection.h"

// Types
typedef uint32_t WindowId;
typedef uint32_t ViewportId;
typedef uint32_t ElementId;

// Element Types ------------------------------
struct SelectionElement {
	SelectionElement() = default;
	SelectionElement(Position topLeft, Position bottomRight, bool inverted = false)
		: topLeft(topLeft), bottomRight(bottomRight), inverted(inverted) { }

	SelectionElement(Position topLeft, bool inverted = false)
		: topLeft(topLeft), bottomRight(topLeft), inverted(inverted) { }

	Position topLeft;
	Position bottomRight;
	bool inverted;
};

struct SelectionObjectElement {
	enum RenderMode {
		SELECTION,
		SELECTION_INVERTED,
		ARROWS
	};
	SelectionObjectElement(SharedSelection selection, RenderMode renderMode = RenderMode::SELECTION)
		: selection(selection), renderMode(renderMode) { }

	SharedSelection selection;
	RenderMode renderMode;
};

struct BlockPreview {
	struct Block {
		Block(BlockType type, Position position, Orientation orientation)
		: type(type), position(position), orientation(orientation) { }
		BlockType type;
		Position position;
		Orientation orientation;
	};

	BlockPreview() = default;
	BlockPreview(BlockType type, Position position, Orientation orientation)
		: blocks({BlockPreview::Block(type, position, orientation)}) { }
	BlockPreview(std::vector<Block>&& blocks) : blocks(std::move(blocks)) {}

	std::vector<Block> blocks;
};

struct ConnectionPreview {
	ConnectionPreview() = default;
	ConnectionPreview(FPosition output, FPosition input)
		: output(output), input(input) { }

	FPosition output;
	FPosition input;
};

struct HalfConnectionPreview {
	HalfConnectionPreview() = default;
	HalfConnectionPreview(FPosition output, FPosition input)
		: output(output), input(input) { }

	FPosition output;
	FPosition input;
};

#endif /* mainRendererDefs_h */
