#ifndef mainRendererDefs_h
#define mainRendererDefs_h

#include "backend/selection.h"
#include "gpu/blockRenderDataManager.h"

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
		Block(BlockRenderDataId blockRenderDataId, Position position, Orientation orientation)
		: blockRenderDataId(blockRenderDataId), position(position), orientation(orientation) { }
		BlockRenderDataId blockRenderDataId;
		Position position;
		Orientation orientation;
	};

	BlockPreview() = default;
	BlockPreview(BlockRenderDataId blockRenderDataId, Position position, Orientation orientation)
		: blocks({BlockPreview::Block(blockRenderDataId, position, orientation)}) { }
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
