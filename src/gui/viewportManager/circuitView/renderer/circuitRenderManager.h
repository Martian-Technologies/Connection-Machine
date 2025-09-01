#ifndef circuitRenderManager_h
#define circuitRenderManager_h

#include "backend/container/difference.h"
#include "gpu/mainRendererDefs.h"

class Circuit;

class CircuitRenderManager {
public:
	CircuitRenderManager(Circuit* circuit, ViewportId viewportId);
	~CircuitRenderManager();
	void addDifference(DifferenceSharedPtr diff);

private:
	struct RenderedBlock {
		RenderedBlock(BlockType type, Orientation orientation) : type(type), orientation(orientation) {}
		std::unordered_map<std::pair<Position, Position>, Position> connectionsToOtherBlock;
		BlockType type;
		Orientation orientation;
	};

	Circuit* circuit;
	ViewportId viewportId;

	std::unordered_map<Position, RenderedBlock> renderedBlocks;
};

#endif /* circuitRenderManager_h */
