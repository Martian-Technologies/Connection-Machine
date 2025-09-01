#include "generatedCircuit.h"

void GeneratedCircuit::addConnectionPort(
	bool isInput,
	connection_end_id_t connectionEndId,
	Vector positionOnBlock,
	block_id_t internalBlockId,
	connection_end_id_t internalBlockConnectionEndId,
	const std::string& portName
) {
	ports.emplace_back(isInput, connectionEndId, positionOnBlock, internalBlockId, internalBlockConnectionEndId, portName);
}

block_id_t GeneratedCircuit::addBlock(Position position, Orientation orientation, BlockType type) {
	block_id_t id = getNewBlockId();
	blocks.try_emplace(id, position, orientation, type);
	positionMap.insert(position, id);
	valid = false;
	return id;
}

block_id_t GeneratedCircuit::addBlock(BlockType type) {
	block_id_t id = getNewBlockId();
	blocks.try_emplace(id, Position(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()), Orientation(), type);
	valid = false;
	return id;
}

void GeneratedCircuit::addConnection(block_id_t outputBlockId, connection_end_id_t outputId, block_id_t inputBlockId, connection_end_id_t inputId) {
	connections.push_back(ConnectionData(outputBlockId, outputId, inputBlockId, inputId));
	valid = false;
}
