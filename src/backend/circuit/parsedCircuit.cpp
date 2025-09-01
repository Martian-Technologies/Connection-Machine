#include "parsedCircuit.h"

void ParsedCircuit::addConnectionPort(
	bool isInput,
	connection_end_id_t connectionEndId,
	Vector positionOnBlock,
	block_id_t internalBlockId,
	connection_end_id_t internalBlockConnectionEndId,
	const std::string& portName
) {
	ports.emplace_back(isInput, connectionEndId, positionOnBlock, internalBlockId, internalBlockConnectionEndId, portName);
	valid = false;
}

void ParsedCircuit::addConnectionPort(
	bool isInput,
	connection_end_id_t connectionEndId,
	Vector positionOnBlock,
	const std::string& portName
) {
	ports.emplace_back(isInput, connectionEndId, positionOnBlock, portName);
	valid = false;
}

void ParsedCircuit::addBlock(block_id_t blockId, FPosition position, Orientation orientation, BlockType type) {
	blocks.try_emplace(blockId, position, orientation, type);
	valid = false;
}

void ParsedCircuit::addBlock(block_id_t blockId, BlockType type) {
	blocks.emplace(blockId, type);
	valid = false;
}

void ParsedCircuit::addConnection(block_id_t outputBlockId, connection_end_id_t outputEndId, block_id_t inputBlockId, connection_end_id_t inputEndId) {
	connections.emplace_back(outputBlockId, outputEndId, inputBlockId, inputEndId);
	valid = false;
}
