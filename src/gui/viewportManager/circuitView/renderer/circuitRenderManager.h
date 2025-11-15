#ifndef circuitRenderManager_h
#define circuitRenderManager_h

#include "backend/container/difference.h"
#include "backend/circuit/circuitDefs.h"
#include "gpu/mainRendererDefs.h"

class Backend;

class CircuitRenderManager {
public:
	CircuitRenderManager(Backend& environment, circuit_id_t circuitId, ViewportId viewportId);
	~CircuitRenderManager();
	void addDifference(DifferenceSharedPtr diff);

private:
	struct RenderedBlock {
		RenderedBlock(BlockType type, Orientation orientation) : type(type), orientation(orientation) {}
		std::unordered_map<std::pair<Position, Position>, Position> connectionsToOtherBlock;
		BlockType type;
		Orientation orientation;
	};

	circuit_id_t circuitId;
	ViewportId viewportId;
	Backend& backend;

	std::unordered_map<Position, RenderedBlock> renderedBlocks;
};

#endif /* circuitRenderManager_h */
