#ifndef circuitRenderManager_h
#define circuitRenderManager_h

#include "backend/container/difference.h"
#include "backend/circuit/circuitDefs.h"
#include "gpu/mainRendererDefs.h"

class Environment;
class Backend;

class CircuitRenderManager {
public:
	CircuitRenderManager(Environment& environment, circuit_id_t circuitId, ViewportId viewportId);
	~CircuitRenderManager();
	void addDifference(DifferenceSharedPtr diff);

private:
	struct InputRenderedBlockPort {
		std::optional<std::vector<Position>> ordering = std::nullopt;
		// port -> block
		std::unordered_map<Position, Position> connections;
	};
	struct OutputRenderedBlockPort {
		// port -> block
		std::unordered_map<Position, Position> connections;
	};
	struct RenderedBlock {
		RenderedBlock(BlockType type, Orientation orientation) : type(type), orientation(orientation) {}
		std::unordered_map<Position, OutputRenderedBlockPort> outputConnections;
		std::unordered_map<Position, InputRenderedBlockPort> inputConnections;
		BlockType type;
		Orientation orientation;
	};

	void createConnectionsForInputPort(Position inputBlockPosition, Position inputPortPosition);
	void sortPortVector(Position inputPortPosition, bool orderOnX, std::vector<Position>& outputPortPositions);
	void orderConnection(Position& outputBlockPosition, Position& outputPortPosition, Position& inputBlockPosition, Position& inputPortPosition);

	circuit_id_t circuitId;
	ViewportId viewportId;
	Environment& environment;
	Backend& backend;

	std::unordered_map<Position, RenderedBlock> renderedBlocks;
};

#endif /* circuitRenderManager_h */
