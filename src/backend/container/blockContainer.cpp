#include "blockContainer.h"

#include "backend/circuit/circuit.h"
#include "backend/circuit/circuitManager.h"
#include "block/block.h"
#include "backend/blockData/blockDataManager.h"

void BlockContainer::clear(Difference* difference) {
	difference->setIsClear();
	for (const std::pair<const unsigned int, Block>& block : blocks) {
		difference->addRemovedBlock(block.second.getPosition(), block.second.getOrientation(), block.second.type());
	}

	lastId = 0;
	grid.clear();
	blocks.clear();
	blockTypeCounts.clear();
}

bool BlockContainer::checkCollision(Position positionA, Position positionB) const {
	for (auto iter = positionA.iterTo(positionB); iter; iter++) {
		if (checkCollision(*iter)) return true;
	}
	return false;
}

bool BlockContainer::checkCollision(Position positionA, Position positionB, block_id_t idToIgnore) const {
	for (auto iter = positionA.iterTo(positionB); iter; iter++) {
		const Cell* cell = getCell(*iter);
		if (cell && cell->getBlockId() != idToIgnore) return true;
	}
	return false;
}

bool BlockContainer::checkCollision(Position position, Orientation orientation, BlockType blockType) const {
	return checkCollision(position, position + blockDataManager->getBlockSize(blockType, orientation).getLargestVectorInArea());
}

bool BlockContainer::checkCollision(Position position, Orientation orientation, BlockType blockType, block_id_t idToIgnore) const {
	return checkCollision(position, position + blockDataManager->getBlockSize(blockType, orientation).getLargestVectorInArea(), idToIgnore);
}

unsigned int BlockContainer::getBlockTypeCountRecursive(BlockType blockType) const {
	if (selfBlockType == blockType) return 0;
	unsigned int count = 0;
	if (blockTypeCounts.size() > blockType) count += blockTypeCounts[blockType];
	for (unsigned int i = 0; i < blockTypeCounts.size(); i++) {
		if ((BlockType)i == blockType || blockTypeCounts[i] == 0) continue;
		circuit_id_t circuitId = circuitManager->getCircuitBlockDataManager()->getCircuitId((BlockType)i);
		if (circuitId == 0) continue;
		SharedCircuit circuit = circuitManager->getCircuit(circuitId);
		count += circuit->getBlockContainer()->getBlockTypeCountRecursive(blockType) * blockTypeCounts[i];
	}
	return count;
}

bool BlockContainer::tryInsertBlock(Position position, Orientation orientation, BlockType blockType, Difference* difference) {
	if (!canInsertBlocktype(blockType)) return false;
	if (checkCollision(position, orientation, blockType)) return false;
	block_id_t id = getNewId();
	auto iter = blocks.insert(std::make_pair(id, getBlockClass(blockDataManager, blockType))).first;
	iter->second.setId(id);
	iter->second.setPosition(position);
	iter->second.setOrientation(orientation);
	if (blockTypeCounts.size() <= blockType) blockTypeCounts.resize(blockType + 1);
	blockTypeCounts[blockType]++;
	placeBlockCells(&iter->second);
	difference->addPlacedBlock(position, orientation, blockType);
	return true;
}

bool BlockContainer::canInsertBlocktype(BlockType blockType) const {
	if (selfBlockType == blockType || !blockDataManager->blockExists(blockType))
		return false;
	circuit_id_t circuitId = circuitManager->getCircuitBlockDataManager()->getCircuitId(blockType);
	if (circuitId != 0 && circuitManager->getCircuit(circuitId)->getBlockContainer()->getBlockTypeCountRecursive(selfBlockType) != 0)
		return false;
	return true;
}

bool BlockContainer::tryRemoveBlock(Position position, Difference* difference) {
	Cell* cell = getCell(position);
	if (cell == nullptr) return false;
	auto iter = blocks.find(cell->getBlockId());
	Block& block = iter->second;
	removeBlockCells(&block);
	// make sure to remove all connections from this block
	for (auto& connectionIter : block.getConnectionContainer().getConnections()) {
		std::optional<Position> connectionPosition = block.getConnectionPosition(connectionIter.first);
		if (!connectionPosition) continue;
		bool isInput = block.isConnectionInput(connectionIter.first);
		const phmap::flat_hash_set<ConnectionEnd>* connections = block.getConnectionContainer().getConnections(connectionIter.first);
		if (!connections) continue;
		for (auto& connectionEnd : *connections) {
			Block* otherBlock = getBlock_(connectionEnd.getBlockId());
			if (otherBlock && otherBlock->getConnectionContainer().tryRemoveConnection(connectionEnd.getConnectionId(), ConnectionEnd(block.id(), connectionIter.first))) {
				std::optional<Position> otherPosition = otherBlock->getConnectionPosition(connectionEnd.getConnectionId());
				if (!otherPosition) continue;
				if (isInput) difference->addRemovedConnection(otherBlock->getPosition(), otherPosition.value(), block.getPosition(), connectionPosition.value());
				else difference->addRemovedConnection(block.getPosition(), connectionPosition.value(), otherBlock->getPosition(), otherPosition.value());
			}
		}
	}
	blockTypeCounts[block.type()]--;
	difference->addRemovedBlock(block.getPosition(), block.getOrientation(), block.type());
	block.destroy();
	blocks.erase(iter);
	return true;
}

bool BlockContainer::tryMoveBlock(Position positionOfBlock, Position position, Orientation transformAmount, Difference* difference, MoveType moveType) {
	Block* block = getBlock_(positionOfBlock);
	if (!block) return false;
	Orientation newOrientation = transformAmount * block->getOrientation();
	Position newPosition = position + (block->getPosition() - positionOfBlock);
	if (checkCollision(newPosition, newOrientation, block->type(), block->id())) return false;
	// do move
	difference->addMovedBlock(block->getPosition(), block->getOrientation(), newPosition, newOrientation, moveType);
	removeBlockCells(block);
	block->setPosition(newPosition);
	block->setOrientation(newOrientation);
	placeBlockCells(block);
	return true;
}

bool BlockContainer::trySetType(Position positionOfBlock, BlockType type, Difference* difference) {
	if (type == selfBlockType) return false;
	Block* oldBlock = getBlock_(positionOfBlock);
	if (!oldBlock) return false;
	if (oldBlock->type() == type) return true;
	Position pos = oldBlock->getPosition();
	Orientation rot = oldBlock->getOrientation();
	auto connections = oldBlock->getConnectionContainer().getConnections();
	tryRemoveBlock(positionOfBlock, difference);
	tryInsertBlock(pos, rot, type, difference);
	Block* newBlock = getBlock_(pos);
	if (!newBlock) return false;
	for (const auto& connectionData : connections) {
		ConnectionEnd end(newBlock->id(), connectionData.first);
		bool isInput = blockDataManager->isConnectionInput(type, connectionData.first);
		for (ConnectionEnd otherEnd : connectionData.second) {
			if (isInput) tryCreateConnection(otherEnd, end, difference);
			else tryCreateConnection(end, otherEnd, difference);
		}
	}
	return true;
}

void BlockContainer::resizeBlockType(BlockType blockType, Size newSize, Difference* difference) {
	if (blockTypeCounts.size() <= blockType || blockTypeCounts[blockType] == 0) return;
	for (auto& pair : blocks) {
		Block* block = &(pair.second);
		if (block->type() != blockType) continue;
		removeBlockCells(block);
		Position position = block->getPosition();
		Size newRotatedSize = block->getOrientation() * newSize;

		while (true) {
			bool hitCell = false;
			for (auto iter = newRotatedSize.iter(); iter; ++iter) {
				Cell* cell = getCell(position + *iter);
				if (cell) {
					// logError("found overlap at {}", "", (position + *iter).toString());
					hitCell = true;
					break;
				}
			}
			if (hitCell) {
				position.x += 1;
			} else break;
		}
		placeBlockCells(block->id(), position, newRotatedSize);
		if (block->getPosition() == position) continue;
		difference->addMovedBlock(block->getPosition(), block->getOrientation(), position, block->getOrientation());
		block->setPosition(position);
	}
}

bool BlockContainer::connectionExists(Position outputPosition, Position inputPosition) const {
	const Block* input = getBlock(inputPosition);
	if (!input) return false;
	std::optional<connection_end_id_t> inputConnectionId = input->getInputConnectionId(inputPosition);
	if (!inputConnectionId) return false;
	const Block* output = getBlock(outputPosition);
	if (!output) return false;
	std::optional<connection_end_id_t> outputConnectionId = output->getOutputConnectionId(outputPosition);
	if (!outputConnectionId) return false;
	return input->getConnectionContainer().hasConnection(inputConnectionId.value(), ConnectionEnd(output->id(), outputConnectionId.value()));
}

const phmap::flat_hash_set<ConnectionEnd>* BlockContainer::getInputConnections(Position position) const {
	const Block* block = getBlock(position);
	return block ? block->getInputConnections(position) : nullptr;
}

const phmap::flat_hash_set<ConnectionEnd>* BlockContainer::getOutputConnections(Position position) const {
	const Block* block = getBlock(position);
	return block ? block->getOutputConnections(position) : nullptr;
}

const std::optional<ConnectionEnd> BlockContainer::getInputConnectionEnd(Position position) const {
	const Block* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getInputConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

const std::optional<ConnectionEnd> BlockContainer::getOutputConnectionEnd(Position position) const {
	const Block* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getOutputConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

bool BlockContainer::tryCreateConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd, Difference* difference) {
	Block* input = getBlock_(inputConnectionEnd.getBlockId());
	if (!input || !input->connectionExists(inputConnectionEnd.getConnectionId())) return false;
	Block* output = getBlock_(outputConnectionEnd.getBlockId());
	if (
		!output || !output->connectionExists(outputConnectionEnd.getConnectionId()) ||
		input->type() == BlockType::JUNCTION && output->type() == BlockType::JUNCTION && input->getConnectionContainer().hasConnection(
			outputConnectionEnd.getConnectionId(),
			ConnectionEnd(outputConnectionEnd.getBlockId(), inputConnectionEnd.getConnectionId())
		)
	) return false;
	if (input->getConnectionContainer().tryMakeConnection(inputConnectionEnd.getConnectionId(), outputConnectionEnd)) {
		bool secondSuc = output->getConnectionContainer().tryMakeConnection(outputConnectionEnd.getConnectionId(), inputConnectionEnd);
		assert(secondSuc);
		difference->addCreatedConnection(
			output->getPosition(), output->getConnectionPosition(outputConnectionEnd.getConnectionId()).value(),
			input->getPosition(), input->getConnectionPosition(inputConnectionEnd.getConnectionId()).value()
		);
		return true;
	}
	return false;
}

bool BlockContainer::tryCreateConnection(Position outputPosition, Position inputPosition, Difference* difference) {
	Block* input = getBlock_(inputPosition);
	if (!input) return false;
	std::optional<connection_end_id_t> inputConnectionId = input->getInputConnectionId(inputPosition);
	if (!inputConnectionId) return false;
	Block* output = getBlock_(outputPosition);
	if (!output) return false;
	std::optional<connection_end_id_t> outputConnectionId = output->getOutputConnectionId(outputPosition);
	if (
		!outputConnectionId ||
		(input->type() == BlockType::JUNCTION && output->type() == BlockType::JUNCTION && input->getConnectionContainer().hasConnection(
			outputConnectionId.value(),
			ConnectionEnd(output->id(), inputConnectionId.value())
		))
	) return false;
	if (input->getConnectionContainer().tryMakeConnection(inputConnectionId.value(), ConnectionEnd(output->id(), outputConnectionId.value()))) {
		bool secondSuc = output->getConnectionContainer().tryMakeConnection(outputConnectionId.value(), ConnectionEnd(input->id(), inputConnectionId.value()));
		assert(secondSuc);
		difference->addCreatedConnection(output->getPosition(), outputPosition, input->getPosition(), inputPosition);
		return true;
	}
	return false;
}

bool BlockContainer::tryRemoveConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd, Difference* difference) {
	Block* input = getBlock_(inputConnectionEnd.getBlockId());
	if (!input) return false;
	Block* output = getBlock_(outputConnectionEnd.getBlockId());
	if (!output) return false;
	if (input->getConnectionContainer().tryRemoveConnection(inputConnectionEnd.getConnectionId(), outputConnectionEnd)) {
		bool secondSuc = output->getConnectionContainer().tryRemoveConnection(outputConnectionEnd.getConnectionId(), inputConnectionEnd);
		assert(secondSuc);
		difference->addRemovedConnection(
			output->getPosition(), output->getConnectionPosition(outputConnectionEnd.getConnectionId()).value(),
			input->getPosition(), input->getConnectionPosition(inputConnectionEnd.getConnectionId()).value()
		);
		return true;
	}
	if (input->type() == BlockType::JUNCTION && output->type() == BlockType::JUNCTION) {
		if (input->getConnectionContainer().tryRemoveConnection(
			outputConnectionEnd.getConnectionId(),
			ConnectionEnd(outputConnectionEnd.getBlockId(), inputConnectionEnd.getConnectionId())
		)) {
			bool secondSuc = output->getConnectionContainer().tryRemoveConnection(
				inputConnectionEnd.getConnectionId(),
				ConnectionEnd(inputConnectionEnd.getBlockId(), outputConnectionEnd.getConnectionId())
			);
			assert(secondSuc);
			difference->addRemovedConnection(
				output->getPosition(), output->getConnectionPosition(inputConnectionEnd.getConnectionId()).value(),
				input->getPosition(), input->getConnectionPosition(outputConnectionEnd.getConnectionId()).value()
			);
			return true;
		}
	}
	return false;
}

bool BlockContainer::tryRemoveConnection(Position outputPosition, Position inputPosition, Difference* difference) {
	Block* input = getBlock_(inputPosition);
	if (!input) return false;
	std::optional<connection_end_id_t> inputConnectionId = input->getInputConnectionId(inputPosition);
	if (!inputConnectionId) return false;
	Block* output = getBlock_(outputPosition);
	if (!output) return false;
	std::optional<connection_end_id_t> outputConnectionId = output->getOutputConnectionId(outputPosition);
	if (!outputConnectionId) return false;
	if (input->getConnectionContainer().tryRemoveConnection(inputConnectionId.value(), ConnectionEnd(output->id(), outputConnectionId.value()))) {
		output->getConnectionContainer().tryRemoveConnection(outputConnectionId.value(), ConnectionEnd(input->id(), inputConnectionId.value()));
		difference->addRemovedConnection(output->getPosition(), outputPosition, input->getPosition(), inputPosition);
		return true;
	}
	if (input->type() == BlockType::JUNCTION && output->type() == BlockType::JUNCTION) {
		if (input->getConnectionContainer().tryRemoveConnection(outputConnectionId.value(), ConnectionEnd(output->id(), inputConnectionId.value()))) {
			bool secondSuc = output->getConnectionContainer().tryRemoveConnection(inputConnectionId.value(), ConnectionEnd(input->id(), outputConnectionId.value()));
			assert(secondSuc);
			difference->addRemovedConnection(input->getPosition(), inputPosition, output->getPosition(), outputPosition);
			return true;
		}
	}
	return false;
}

void BlockContainer::addConnectionPort(BlockType blockType, connection_end_id_t endId, Difference* difference) { } // do nothing because the connection containers use hashes rn

void BlockContainer::removeConnectionPort(BlockType blockType, connection_end_id_t endId, Difference* difference) {
	if (blockTypeCounts.size() <= blockType || blockTypeCounts[blockType] == 0) return;
	for (auto& pair : blocks) {
		Block& block = pair.second;
		if (block.type() != blockType) continue;
		bool isInput = block.isConnectionInput(endId);
		std::optional<Position> connectionPosition = block.getConnectionPosition(endId);
		if (!connectionPosition) continue;
		const ConnectionContainer& connectionContainer = block.getConnectionContainer();
		const phmap::flat_hash_set<ConnectionEnd>* connections = connectionContainer.getConnections(endId);
		if (!connections) continue;
		const phmap::flat_hash_set<ConnectionEnd> connectionsCopy = *connections;
		for (auto& connectionEnd : connectionsCopy) {
			Block* otherBlock = getBlock_(connectionEnd.getBlockId());
			if (otherBlock && otherBlock->getConnectionContainer().tryRemoveConnection(connectionEnd.getConnectionId(), ConnectionEnd(block.id(), endId))) {
				bool secondSuc = block.getConnectionContainer().tryRemoveConnection(endId, connectionEnd);
				assert(secondSuc);
				std::optional otherPosition = otherBlock->getConnectionPosition(connectionEnd.getConnectionId());
				if (!otherPosition) continue;
				if (isInput) difference->addRemovedConnection(otherBlock->getPosition(), otherPosition.value(), block.getPosition(), connectionPosition.value());
				else difference->addRemovedConnection(block.getPosition(), connectionPosition.value(), otherBlock->getPosition(), otherPosition.value());
			}
		}
	}
}

void BlockContainer::placeBlockCells(Position position, Orientation orientation, BlockType type, block_id_t blockId) {
	for (auto iter = blockDataManager->getBlockSize(type, orientation).iter(); iter; iter++) {
		insertCell(position + *iter, Cell(blockId));
	}
}

void BlockContainer::placeBlockCells(block_id_t id, Position position, Size size) {
	for (auto iter = size.iter(); iter; iter++) {
		insertCell(position + *iter, Cell(id));
	}
}

void BlockContainer::placeBlockCells(const Block* block) {
	for (auto iter = block->size().iter(); iter; iter++) {
		insertCell(block->getPosition() + *iter, Cell(block->id()));
	}
}

void BlockContainer::removeBlockCells(const Block* block) {
	for (auto iter = block->size().iter(); iter; iter++) {
		removeCell(block->getPosition() + *iter);
	}
}

Difference BlockContainer::getCreationDifference() const {
	Difference difference;
	for (const std::pair<const unsigned int, Block>& block : blocks) {
		difference.addPlacedBlock(block.second.getPosition(), block.second.getOrientation(), block.second.type());
	}
	for (const std::pair<const unsigned int, Block>& block : blocks) {
		for (auto& connectionIter : block.second.getConnectionContainer().getConnections()) {
			if (block.second.isConnectionInput(connectionIter.first)) continue;
			const auto connections = block.second.getConnectionContainer().getConnections(connectionIter.first);
			if (!connections) continue;
			for (auto otherConnectionIter : *connections) {
				const Block* otherBlock = getBlock(otherConnectionIter.getBlockId());
				difference.addCreatedConnection(
					block.second.getPosition(),
					block.second.getConnectionPosition(connectionIter.first).value(),
					otherBlock->getPosition(),
					otherBlock->getConnectionPosition(otherConnectionIter.getConnectionId()).value());
			}
		}
	}
	return difference;
}

DifferenceSharedPtr BlockContainer::getCreationDifferenceShared() const {
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	for (auto iter : blocks) {
		difference->addPlacedBlock(iter.second.getPosition(), iter.second.getOrientation(), iter.second.type());
	}
	for (auto iter : blocks) {
		for (auto& connectionIter : iter.second.getConnectionContainer().getConnections()) {
			if (iter.second.isConnectionInput(connectionIter.first)) continue;
			const phmap::flat_hash_set<ConnectionEnd>* connections = iter.second.getConnectionContainer().getConnections(connectionIter.first);
			if (!connections) continue;
			for (auto otherConnectionIter : *connections) {
				difference->addCreatedConnection(
					iter.second.getPosition(),
					iter.second.getConnectionPosition(connectionIter.first).value(),
					getBlock(otherConnectionIter.getBlockId())->getPosition(),
					getBlock(otherConnectionIter.getBlockId())->getConnectionPosition(otherConnectionIter.getConnectionId()).value()
				);
			}
		}
	}
	return difference;
}
