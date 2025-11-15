#ifndef generatedCircuitValidator_h
#define generatedCircuitValidator_h

#include "backend/container/block/connectionEnd.h"
#include "generatedCircuit.h"

class BlockDataManager;

class GeneratedCircuitValidator {
public:
	GeneratedCircuitValidator(GeneratedCircuit& generatedCircuit, BlockDataManager& blockDataManager) :
		generatedCircuit(generatedCircuit), blockDataManager(blockDataManager) {
		validate();
	}
private:
	struct ConnectionHash {
		size_t operator()(const GeneratedCircuit::ConnectionData& connectionData) const {
			return std::hash<block_id_t>()(connectionData.outputBlockId) ^ std::hash<connection_end_id_t>()(connectionData.outputId) ^
				   std::hash<block_id_t>()(connectionData.inputBlockId) ^ std::hash<connection_end_id_t>()(connectionData.inputId);
		}
	};

	void validate();
	bool validateBlockData();
	bool validateBlockTypes();
	bool handleInvalidConnections();
	bool setOverlapsUnpositioned();

	bool handleUnpositionedBlocks();

	bool isIntegerPosition(const FPosition& pos) const { return pos.x == std::floor(pos.x) && pos.y == std::floor(pos.y); }
	block_id_t generateNewBlockId() const {
		block_id_t id = 0;
		// slow
		while (generatedCircuit.blocks.find(id) != generatedCircuit.blocks.end()) {
			++id;
		}
		return id;
	}

	BlockDataManager& blockDataManager;
	GeneratedCircuit& generatedCircuit;
	std::unordered_set<Position> occupiedPositions;
};

#endif /* generatedCircuitValidator_h */
