#include "blockContainer3d.h"

// #include "backend/circuit/circuit.h"
// #include "backend/circuit/circuitManager.h"
#include "block/block3d.h"
#include "backend/blockData/blockDataManager.h"

void BlockContainer3d::clear(Difference3d* difference) {
	difference->setIsClear();
	for (const std::pair<const unsigned int, Block3d>& block : blocks) {
		difference->addRemovedBlock(block.second.getPosition(), block.second.getOrientation(), block.second.type());
	}

	lastId = 0;
	grid.clear();
	blocks.clear();
	blockTypeCounts.clear();
}

bool BlockContainer3d::checkCollision(Position3 positionA, Position3 positionB) const {
	for (auto iter = positionA.iterTo(positionB); iter; iter++) {
		if (checkCollision(*iter)) return true;
	}
	return false;
}

bool BlockContainer3d::checkCollision(Position3 positionA, Position3 positionB, block_id_t idToIgnore) const {
	for (auto iter = positionA.iterTo(positionB); iter; iter++) {
		const Cell* cell = getCell(*iter);
		if (cell && cell->getBlockId() != idToIgnore) return true;
	}
	return false;
}

bool BlockContainer3d::checkCollision(Position3 position, Orientation3d orientation, BlockType blockType) const {
	Size size = blockDataManager.getBlockSize(blockType);
	return checkCollision(position, position + (orientation * Size3(size.w, size.h, 1)).getLargestVectorInArea());
}

bool BlockContainer3d::checkCollision(Position3 position, Orientation3d orientation, BlockType blockType, block_id_t idToIgnore) const {
	Size size = blockDataManager.getBlockSize(blockType, idToIgnore);
	return checkCollision(position, position + (orientation * Size3(size.w, size.h, 1)).getLargestVectorInArea());
}

// unsigned int BlockContainer3d::getBlockTypeCountRecursive(BlockType blockType) const {
// 	if (selfBlockType == blockType) return 0;
// 	unsigned int count = 0;
// 	if (blockTypeCounts.size() > blockType) count += blockTypeCounts[blockType];
// 	for (unsigned int i = 0; i < blockTypeCounts.size(); i++) {
// 		if ((BlockType)i == blockType || blockTypeCounts[i] == 0) continue;
// 		circuit_id_t circuitId = circuitManager.getCircuitBlockDataManager().getCircuitId((BlockType)i);
// 		if (circuitId == 0) continue;
// 		SharedCircuit circuit = circuitManager.getCircuit(circuitId);
// 		count += circuit->getBlockContainer().getBlockTypeCountRecursive(blockType) * blockTypeCounts[i];
// 	}
// 	return count;
// }

bool BlockContainer3d::tryInsertBlock(Position3 position, Orientation3d orientation, BlockType blockType, Difference3d* difference) {
	if (!canInsertBlocktype(blockType)) return false;
	if (checkCollision(position, orientation, blockType)) return false;
	block_id_t id = getNewId();
	auto iter = blocks.insert(std::make_pair(id, getBlock3dClass(blockDataManager, blockType))).first;
	iter->second.setId(id);
	iter->second.setPosition(position);
	iter->second.setOrientation(orientation);
	if (blockTypeCounts.size() <= blockType) blockTypeCounts.resize(blockType + 1);
	blockTypeCounts[blockType]++;
	placeBlockCells(&iter->second);
	difference->addPlacedBlock(position, orientation, blockType);
	return true;
}

bool BlockContainer3d::canInsertBlocktype(BlockType blockType) const {
	if (selfBlockType == blockType || !blockDataManager.blockExists(blockType))
		return false;
	// circuit_id_t circuitId = circuitManager.getCircuitBlockDataManager().getCircuitId(blockType);
	// if (circuitId != 0 && circuitManager.getCircuit(circuitId)->getBlockContainer().getBlockTypeCountRecursive(selfBlockType) != 0)
	// 	return false;
	return true;
}

bool BlockContainer3d::tryRemoveBlock(Position3 position, Difference3d* difference) {
	Cell* cell = getCell(position);
	if (cell == nullptr) return false;
	auto iter = blocks.find(cell->getBlockId());
	Block3d& block = iter->second;
	removeBlockCells(&block);
	// make sure to remove all connections from this block
	const BlockData* blockData = blockDataManager.getBlockData(block.type());
	Size size2d = blockData->getSize();
	Size3 size(size2d.w, size2d.h, 1);
	for (auto& connectionIter : block.getConnectionContainer().getConnections()) {
		Vector connectionVector = blockData->getConnectionVector(connectionIter.first).value();
		std::optional<Position3> connectionPosition = block.getPosition() + block.getOrientation().transformVectorWithArea(Vector3(connectionVector.dx, connectionVector.dy, 0), size);
		if (!connectionPosition) continue;
		BlockData::ConnectionData::PortType portType = blockData->getConnectionPortType(connectionIter.first);
		if (portType == BlockData::ConnectionData::PortType::INPUT) {
			for (ConnectionEnd otherEnd : connectionIter.second) {
				Block3d* otherBlock = getBlock_(otherEnd.getBlockId());
				if (otherBlock && otherBlock->getConnectionContainer().tryRemoveConnection(otherEnd.getConnectionId(), ConnectionEnd(block.id(), connectionIter.first))) {
					std::optional<Position3> otherPosition = otherBlock->getConnectionPosition(otherEnd.getConnectionId());
					if (!otherPosition) continue;
					difference->addRemovedConnection(otherBlock->getPosition(), otherPosition.value(), block.getPosition(), connectionPosition.value());
				}
			}
		} else if (portType == BlockData::ConnectionData::PortType::OUTPUT) {
			for (ConnectionEnd otherEnd : connectionIter.second) {
				Block3d* otherBlock = getBlock_(otherEnd.getBlockId());
				if (otherBlock && otherBlock->getConnectionContainer().tryRemoveConnection(otherEnd.getConnectionId(), ConnectionEnd(block.id(), connectionIter.first))) {
					std::optional<Position3> otherPosition = otherBlock->getConnectionPosition(otherEnd.getConnectionId());
					if (!otherPosition) continue;
					difference->addRemovedConnection(block.getPosition(), connectionPosition.value(), otherBlock->getPosition(), otherPosition.value());
				}
			}
		} else {
			for (ConnectionEnd otherEnd : connectionIter.second) {
				Block3d* otherBlock = getBlock_(otherEnd.getBlockId());
				if (otherBlock && otherBlock->getConnectionContainer().tryRemoveConnection(otherEnd.getConnectionId(), ConnectionEnd(block.id(), connectionIter.first))) {
					std::optional<Position3> otherPosition = otherBlock->getConnectionPosition(otherEnd.getConnectionId());
					if (!otherPosition) continue;
					if (otherBlock->isConnectionInput(otherEnd.getConnectionId())) {
						difference->addRemovedConnection(block.getPosition(), connectionPosition.value(), otherBlock->getPosition(), otherPosition.value());
					} else {
						difference->addRemovedConnection(otherBlock->getPosition(), otherPosition.value(), block.getPosition(), connectionPosition.value());
					}
				}
			}
		}
	}
	blockTypeCounts[block.type()]--;
	difference->addRemovedBlock(block.getPosition(), block.getOrientation(), block.type());
	block.destroy();
	blocks.erase(iter);
	return true;
}

bool BlockContainer3d::tryMoveBlock(Position3 positionOfBlock, Position3 position, Orientation3d transformAmount, Difference3d* difference, MoveType moveType) {
	Block3d* block = getBlock_(positionOfBlock);
	if (!block) return false;
	Orientation3d newOrientation = transformAmount * block->getOrientation();
	Position3 newPosition3 = position + (block->getPosition() - positionOfBlock);
	if (checkCollision(newPosition3, newOrientation, block->type(), block->id())) return false;
	// do move
	difference->addMovedBlock(block->getPosition(), block->getOrientation(), newPosition3, newOrientation, moveType);
	removeBlockCells(block);
	block->setPosition(newPosition3);
	block->setOrientation(newOrientation);
	placeBlockCells(block);
	return true;
}

bool BlockContainer3d::trySetType(Position3 positionOfBlock, BlockType type, Difference3d* difference) {
	if (type == selfBlockType) return false;
	Block3d* oldBlock = getBlock_(positionOfBlock);
	if (!oldBlock) return false;
	BlockType oldBlockType = oldBlock->type();
	if (oldBlockType == type) return true;
	if (!(((
			type == BlockType::AND ||
			type == BlockType::OR ||
			type == BlockType::XOR ||
			type == BlockType::NOR ||
			type == BlockType::NAND ||
			type == BlockType::XNOR ||
			type == BlockType::BUFFER ||
			type == BlockType::NOT
		) && (
			oldBlockType == BlockType::AND ||
			oldBlockType == BlockType::OR ||
			oldBlockType == BlockType::XOR ||
			oldBlockType == BlockType::NOR ||
			oldBlockType == BlockType::NAND ||
			oldBlockType == BlockType::XNOR ||
			oldBlockType == BlockType::BUFFER ||
			oldBlockType == BlockType::NOT
	)) || ((
			type == BlockType::CONSTANT_OFF ||
			type == BlockType::CONSTANT_ON ||
			type == BlockType::CONSTANT_Z ||
			type == BlockType::CONSTANT_X
		) && (
			oldBlockType != BlockType::CONSTANT_OFF ||
			oldBlockType != BlockType::CONSTANT_ON ||
			oldBlockType != BlockType::CONSTANT_Z ||
			oldBlockType != BlockType::CONSTANT_X
	)))) return false;
	Position3 pos = oldBlock->getPosition();
	Orientation3d rot = oldBlock->getOrientation();
	auto connections = oldBlock->getConnectionContainer().getConnections();
	tryRemoveBlock(positionOfBlock, difference);
	tryInsertBlock(pos, rot, type, difference);
	Block3d* newBlock = getBlock_(pos);
	if (!newBlock) return false;
	const BlockData* blockData = blockDataManager.getBlockData(type);
	for (const auto& connectionData : connections) {
		ConnectionEnd end(newBlock->id(), connectionData.first);
		BlockData::ConnectionData::PortType portType = blockData->getConnectionPortType(connectionData.first);
		if (portType == BlockData::ConnectionData::PortType::INPUT) {
			for (ConnectionEnd otherEnd : connectionData.second) {
				tryCreateConnection(otherEnd, end, difference);
			}
		} else if (portType == BlockData::ConnectionData::PortType::OUTPUT) {
			for (ConnectionEnd otherEnd : connectionData.second) {
				tryCreateConnection(end, otherEnd, difference);
			}
		} else {
			for (ConnectionEnd otherEnd : connectionData.second) {
				const Block3d* otherBlock = getBlock(otherEnd.getBlockId());
				if (blockDataManager.isConnectionInput(otherBlock->type(), otherEnd.getConnectionId())) tryCreateConnection(end, otherEnd, difference);
				else tryCreateConnection(otherEnd, end, difference);
			}
		}
	}
	return true;
}

void BlockContainer3d::resizeBlockType(BlockType blockType, Size3 newSize, Difference3d* difference) {
	if (blockTypeCounts.size() <= blockType || blockTypeCounts[blockType] == 0) return;
	for (auto& pair : blocks) {
		Block3d* block = &(pair.second);
		if (block->type() != blockType) continue;
		removeBlockCells(block);
		Position3 position = block->getPosition();
		Size3 newRotatedSize = block->getOrientation() * newSize;

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

bool BlockContainer3d::connectionExists(Position3 outputPosition, Position3 inputPosition) const {
	const Block3d* input = getBlock(inputPosition);
	if (!input) return false;
	std::optional<connection_end_id_t> inputConnectionId = input->getInputConnectionId(inputPosition);
	if (!inputConnectionId) return false;
	const Block3d* output = getBlock(outputPosition);
	if (!output) return false;
	std::optional<connection_end_id_t> outputConnectionId = output->getOutputConnectionId(outputPosition);
	if (!outputConnectionId) return false;
	return input->getConnectionContainer().hasConnection(inputConnectionId.value(), ConnectionEnd(output->id(), outputConnectionId.value()));
}

bool BlockContainer3d::connectionExists(ConnectionEnd connectionEndA, ConnectionEnd connectionEndB) const {
	const Block3d* blockA = getBlock(connectionEndA.getBlockId());
	if (!blockA) return false;
	return blockA->getConnectionContainer().hasConnection(connectionEndA.getConnectionId(), ConnectionEnd(connectionEndB.getBlockId(), connectionEndB.getConnectionId()));
}

const std::unordered_set<ConnectionEnd>* BlockContainer3d::getInputConnections(Position3 position) const {
	const Block3d* block = getBlock(position);
	return block ? block->getInputConnections(position) : nullptr;
}

const std::unordered_set<ConnectionEnd>* BlockContainer3d::getOutputConnections(Position3 position) const {
	const Block3d* block = getBlock(position);
	return block ? block->getOutputConnections(position) : nullptr;
}

const std::unordered_set<ConnectionEnd>* BlockContainer3d::getBidirectionalConnections(Position3 position) const {
	const Block3d* block = getBlock(position);
	return block ? block->getOutputConnections(position) : nullptr;
}

const std::optional<ConnectionEnd> BlockContainer3d::getInputConnectionEnd(Position3 position) const {
	const Block3d* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getInputConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

const std::optional<ConnectionEnd> BlockContainer3d::getOutputConnectionEnd(Position3 position) const {
	const Block3d* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getOutputConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

const std::optional<ConnectionEnd> BlockContainer3d::getBidirectionalConnectionEnd(Position3 position) const {
	const Block3d* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getBidirectionalConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

const std::optional<ConnectionEnd> BlockContainer3d::getInputOrBidirectionalConnectionEnd(Position3 position) const {
	const Block3d* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getInputOrBidirectionalConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

const std::optional<ConnectionEnd> BlockContainer3d::getOutputOrBidirectionalConnectionEnd(Position3 position) const {
	const Block3d* block = getBlock(position);
	if (!block) return std::nullopt;
	std::optional<connection_end_id_t> connectionData = block->getOutputOrBidirectionalConnectionId(position);
	if (!connectionData) return std::nullopt;
	return ConnectionEnd(block->id(), connectionData.value());
}

bool BlockContainer3d::tryCreateConnection(ConnectionEnd connectionEndA, ConnectionEnd connectionEndB, Difference3d* difference) {
	if (connectionEndA.getConnectionId() == connectionEndB.getConnectionId() && connectionEndA.getBlockId() == connectionEndB.getBlockId()) return false; // ports cant self connect
	Block3d* blockA = getBlock_(connectionEndA.getBlockId());
	if (!blockA || blockA->getConnectionContainer().hasConnection(connectionEndA.getConnectionId(), connectionEndB)) return false;
	const BlockData* blockABlockData = blockDataManager.getBlockData(blockA->type());
	BlockData::ConnectionData::PortType portAType = blockABlockData->getConnectionPortType(connectionEndA.getConnectionId());
	if (portAType == BlockData::ConnectionData::PortType::NONE) return false;
	Block3d* blockB = getBlock_(connectionEndB.getBlockId());
	if (!blockB) return false;
	const BlockData* blockBBlockData = blockDataManager.getBlockData(blockB->type());
	BlockData::ConnectionData::PortType portBType = blockBBlockData->getConnectionPortType(connectionEndB.getConnectionId());
	if (
		portBType == BlockData::ConnectionData::PortType::NONE ||
		(portBType == BlockData::ConnectionData::PortType::OUTPUT && portAType == BlockData::ConnectionData::PortType::OUTPUT) ||
		(portBType == BlockData::ConnectionData::PortType::INPUT && portAType == BlockData::ConnectionData::PortType::INPUT)
	) return false;
	if (blockA->type() == BlockType::JUNCTION) {
		unsigned int blockABitWidth = getBitwidthOfJunction(blockA->id());
		if (blockABitWidth != 0) {
			if (blockB->type() == BlockType::JUNCTION) {
				unsigned int blockBBitWidth = getBitwidthOfJunction(blockB->id());
				if (blockBBitWidth != 0 && blockABitWidth != blockBBitWidth) return false;
			} else if (blockABitWidth != blockBBlockData->getConnectionBitWidth(connectionEndB.getConnectionId())) {
				return false;
			}
		}
	} else if (blockB->type() == BlockType::JUNCTION) {
		unsigned int blockBBitWidth = getBitwidthOfJunction(blockB->blockId);
		if (blockBBitWidth != 0 && blockBBitWidth != blockABlockData->getConnectionBitWidth(connectionEndA.getConnectionId())) {
			return false;
		}
	} else if (
		blockABlockData->getConnectionBitWidth(connectionEndA.getConnectionId()) !=
		blockBBlockData->getConnectionBitWidth(connectionEndB.getConnectionId())
	) {
		return false;
	}
	if (blockA->getConnectionContainer().tryMakeConnection(connectionEndA.getConnectionId(), connectionEndB)) {
		bool secondSuc = blockB->getConnectionContainer().tryMakeConnection(connectionEndB.getConnectionId(), connectionEndA);
		assert(secondSuc);
		if (portAType == BlockData::ConnectionData::PortType::INPUT || portBType == BlockData::ConnectionData::PortType::OUTPUT) {
			difference->addCreatedConnection(
				blockB->getPosition(), blockB->getConnectionPosition(connectionEndB.getConnectionId()).value(),
				blockA->getPosition(), blockA->getConnectionPosition(connectionEndA.getConnectionId()).value()
			);
		} else {
			difference->addCreatedConnection(
				blockA->getPosition(), blockA->getConnectionPosition(connectionEndA.getConnectionId()).value(),
				blockB->getPosition(), blockB->getConnectionPosition(connectionEndB.getConnectionId()).value()
			);
		}
		return true;
	}
	return false;
}

bool BlockContainer3d::tryCreateConnection(Position3 outputPosition, Position3 inputPosition, Difference3d* difference) {
	Block3d* input = getBlock_(inputPosition);
	if (!input) return false;
	const BlockData* inputBlockData = blockDataManager.getBlockData(input->type());
	std::optional<connection_end_id_t> inputConnectionId = input->getInputConnectionId(inputPosition);
	BlockData::ConnectionData::PortType inputPortType = BlockData::ConnectionData::PortType::INPUT;
	if (!inputConnectionId) {
		if (outputPosition == inputPosition) return false; // bidirectional ports cant self connect
		inputConnectionId = input->getBidirectionalConnectionId(inputPosition);
		if (!inputConnectionId) return false;
		inputPortType = BlockData::ConnectionData::PortType::BIDIRECTIONAL;
	}
	Block3d* output = getBlock_(outputPosition);
	if (!output) return false;
	const BlockData* outputBlockData = blockDataManager.getBlockData(output->type());
	std::optional<connection_end_id_t> outputConnectionId = output->getInputConnectionId(outputPosition);
	BlockData::ConnectionData::PortType outputPortType = BlockData::ConnectionData::PortType::OUTPUT;
	if (!outputConnectionId) {
		outputConnectionId = output->getBidirectionalConnectionId(outputPosition);
		if (!outputConnectionId) return false;
		outputPortType = BlockData::ConnectionData::PortType::BIDIRECTIONAL;
	}
	if (input->getConnectionContainer().hasConnection(
		inputConnectionId.value(),
		ConnectionEnd(output->id(), outputConnectionId.value())
	)) return false;
	if (input->type() == BlockType::JUNCTION) {
		unsigned int inputBitWidth = getBitwidthOfJunction(input->id());
		if (inputBitWidth != 0) {
			if (output->type() == BlockType::JUNCTION) {
				unsigned int otherBitWidth = getBitwidthOfJunction(output->id());
				if (otherBitWidth != 0 && inputBitWidth != otherBitWidth) return false;
			} else if (inputBitWidth != outputBlockData->getConnectionBitWidth(outputConnectionId.value())) {
				return false;
			}
		}
	} else if (output->type() == BlockType::JUNCTION) {
		unsigned int outputBitWidth = getBitwidthOfJunction(output->blockId);
		if (outputBitWidth != 0 && outputBitWidth != inputBlockData->getConnectionBitWidth(inputConnectionId.value())) {
			return false;
		}
	} else if (inputBlockData->getConnectionBitWidth(inputConnectionId.value()) != outputBlockData->getConnectionBitWidth(outputConnectionId.value())) return false;
	if (input->getConnectionContainer().tryMakeConnection(inputConnectionId.value(), ConnectionEnd(output->id(), outputConnectionId.value()))) {
		bool secondSuc = output->getConnectionContainer().tryMakeConnection(outputConnectionId.value(), ConnectionEnd(input->id(), inputConnectionId.value()));
		assert(secondSuc);
		difference->addCreatedConnection(output->getPosition(), outputPosition, input->getPosition(), inputPosition);
		return true;
	}
	return false;
}

bool BlockContainer3d::tryRemoveConnection(ConnectionEnd connectionEndB, ConnectionEnd connectionEndA, Difference3d* difference) {
	if (connectionEndA.getConnectionId() == connectionEndB.getConnectionId() && connectionEndA.getBlockId() == connectionEndB.getBlockId()) return false; // ports cant self connect
	Block3d* blockA = getBlock_(connectionEndA.getBlockId());
	if (!blockA) return false;
	if (blockA->getConnectionContainer().tryRemoveConnection(connectionEndA.getConnectionId(), connectionEndB)) {
		Block3d* blockB = getBlock_(connectionEndB.getBlockId());
		assert(blockB);
		bool secondSuc = blockB->getConnectionContainer().tryRemoveConnection(connectionEndB.getConnectionId(), connectionEndA);
		assert(secondSuc);
		BlockData::ConnectionData::PortType portAType = blockDataManager.getBlockData(blockA->type())->getConnectionPortType(connectionEndA.getConnectionId());
		BlockData::ConnectionData::PortType portBType = blockDataManager.getBlockData(blockB->type())->getConnectionPortType(connectionEndB.getConnectionId());
		if (portAType == BlockData::ConnectionData::PortType::INPUT || portBType == BlockData::ConnectionData::PortType::OUTPUT) {
			difference->addRemovedConnection(
				blockB->getPosition(), blockB->getConnectionPosition(connectionEndB.getConnectionId()).value(),
				blockA->getPosition(), blockA->getConnectionPosition(connectionEndA.getConnectionId()).value()
			);
		} else {
			difference->addCreatedConnection(
				blockA->getPosition(), blockA->getConnectionPosition(connectionEndA.getConnectionId()).value(),
				blockB->getPosition(), blockB->getConnectionPosition(connectionEndB.getConnectionId()).value()
			);
		}
		return true;
	}
	return false;
}

bool BlockContainer3d::tryRemoveConnection(Position3 outputPosition, Position3 inputPosition, Difference3d* difference) {
	Block3d* input = getBlock_(inputPosition);
	if (!input) return false;
	const BlockData* inputBlockData = blockDataManager.getBlockData(input->type());
	std::optional<connection_end_id_t> inputConnectionId = input->getInputConnectionId(inputPosition);
	BlockData::ConnectionData::PortType inputPortType = BlockData::ConnectionData::PortType::INPUT;
	if (!inputConnectionId) {
		if (outputPosition == inputPosition) return false; // bidirectional ports cant self connect
		inputConnectionId = input->getBidirectionalConnectionId(inputPosition);
		if (!inputConnectionId) return false;
		inputPortType = BlockData::ConnectionData::PortType::BIDIRECTIONAL;
	}
	Block3d* output = getBlock_(outputPosition);
	if (!output) return false;
	const BlockData* outputBlockData = blockDataManager.getBlockData(output->type());
	std::optional<connection_end_id_t> outputConnectionId = output->getOutputConnectionId(outputPosition);
	BlockData::ConnectionData::PortType outputPortType = BlockData::ConnectionData::PortType::OUTPUT;
	if (!outputConnectionId) {
		outputConnectionId = output->getBidirectionalConnectionId(outputPosition);
		if (!outputConnectionId) return false;
		outputPortType = BlockData::ConnectionData::PortType::BIDIRECTIONAL;
	}
	if (input->getConnectionContainer().tryRemoveConnection(inputConnectionId.value(), ConnectionEnd(output->id(), outputConnectionId.value()))) {
		output->getConnectionContainer().tryRemoveConnection(outputConnectionId.value(), ConnectionEnd(input->id(), inputConnectionId.value()));
		difference->addRemovedConnection(output->getPosition(), outputPosition, input->getPosition(), inputPosition);
		return true;
	}
	return false;
}

void BlockContainer3d::addConnectionPort(BlockType blockType, connection_end_id_t endId, Difference3d* difference) { } // do nothing because the connection containers use hashes rn

void BlockContainer3d::setConnectionPortBitwidth(BlockType blockType, connection_end_id_t endId, unsigned int bitwidth, Difference3d* difference) {
	if (blockTypeCounts.size() <= blockType || blockTypeCounts[blockType] == 0) return;
	BlockData::ConnectionData::PortType portType = blockDataManager.getBlockData(blockType)->getConnectionPortType(endId);
	if (portType == BlockData::ConnectionData::PortType::NONE) {
		logError("Called removeConnectionPort on non existent port id {} for block type {}", "BlockContainer3d", endId, blockType);
		return;
	}
	for (auto& pair : blocks) {
		Block3d& block = pair.second;
		if (block.type() != blockType) continue;
		std::optional<Position3> connectionPosition = block.getConnectionPosition(endId);
		assert(connectionPosition);
		auto connections = block.getConnectionContainer().getConnections(endId);
		if (!connections) continue;
		const std::unordered_set<ConnectionEnd> connectionsCopy = *connections;
		for (auto& connectionEnd : connectionsCopy) {
			Block3d* otherBlock = getBlock_(connectionEnd.getBlockId());
			if (otherBlock->type() == blockType && connectionEnd.getConnectionId() == endId) continue;
			unsigned int otherBitwidth = 1;
			if (otherBlock->type() == BlockType::JUNCTION) {
				otherBitwidth = getBitwidthOfJunctionIgnorePort(otherBlock, blockType, endId);
			} else {
				otherBitwidth = blockDataManager.getBlockData(otherBlock->type())->getConnectionBitWidth(connectionEnd.getConnectionId());
			}
			if (otherBitwidth == 0) continue;
			if (otherBitwidth != bitwidth) {
				if (block.getConnectionContainer().tryRemoveConnection(endId, connectionEnd)) {
					bool secondSuc = otherBlock->getConnectionContainer().tryRemoveConnection(connectionEnd.getConnectionId(), ConnectionEnd(block.id(), endId));
					assert(secondSuc);
					BlockData::ConnectionData::PortType otherPortType = blockDataManager.getBlockData(otherBlock->type())->getConnectionPortType(connectionEnd.getConnectionId());
					if (portType == BlockData::ConnectionData::PortType::INPUT || otherPortType == BlockData::ConnectionData::PortType::OUTPUT) {
						difference->addRemovedConnection(
							otherBlock->getPosition(), otherBlock->getConnectionPosition(connectionEnd.getConnectionId()).value(),
							block.getPosition(), connectionPosition.value()
						);
					} else {
						difference->addCreatedConnection(
							block.getPosition(), connectionPosition.value(),
							otherBlock->getPosition(), otherBlock->getConnectionPosition(connectionEnd.getConnectionId()).value()
						);
					}
				}
			}
		}
	}
}

void BlockContainer3d::removeConnectionPort(BlockType blockType, connection_end_id_t endId, Difference3d* difference) {
	if (blockTypeCounts.size() <= blockType || blockTypeCounts[blockType] == 0) return;
	BlockData::ConnectionData::PortType portType = blockDataManager.getBlockData(blockType)->getConnectionPortType(endId);
	if (portType == BlockData::ConnectionData::PortType::NONE) {
		logError("Called removeConnectionPort on non existent port id {} for block type {}", "BlockContainer3d", endId, blockType);
		return;
	}
	for (auto& pair : blocks) {
		Block3d& block = pair.second;
		if (block.type() != blockType) continue;
		std::optional<Position3> connectionPosition = block.getConnectionPosition(endId);
		assert(connectionPosition);
		auto connections = block.getConnectionContainer().getConnections(endId);
		if (!connections) continue;
		const std::unordered_set<ConnectionEnd> connectionsCopy = *connections;
		for (auto& connectionEnd : connectionsCopy) {
			if (block.getConnectionContainer().tryRemoveConnection(endId, connectionEnd)) {
				Block3d* otherBlock = getBlock_(connectionEnd.getBlockId());
				assert(otherBlock);
				bool secondSuc = otherBlock->getConnectionContainer().tryRemoveConnection(connectionEnd.getConnectionId(), ConnectionEnd(block.id(), endId));
				assert(secondSuc);
				BlockData::ConnectionData::PortType otherPortType = blockDataManager.getBlockData(otherBlock->type())->getConnectionPortType(connectionEnd.getConnectionId());
				if (portType == BlockData::ConnectionData::PortType::INPUT || otherPortType == BlockData::ConnectionData::PortType::OUTPUT) {
					difference->addRemovedConnection(
						otherBlock->getPosition(), otherBlock->getConnectionPosition(connectionEnd.getConnectionId()).value(),
						block.getPosition(), connectionPosition.value()
					);
				} else {
					difference->addRemovedConnection(
						block.getPosition(), connectionPosition.value(),
						otherBlock->getPosition(), otherBlock->getConnectionPosition(connectionEnd.getConnectionId()).value()
					);
				}
			}
		}
	}
}

void BlockContainer3d::placeBlockCells(Position3 position, Orientation3d orientation, BlockType type, block_id_t blockId) {
	Size size = blockDataManager.getBlockSize(type);
	for (auto iter = (orientation * Size3(size.w, size.h, 1)).iter(); iter; iter++) {
		insertCell(position + *iter, Cell(blockId));
	}
}

void BlockContainer3d::placeBlockCells(block_id_t id, Position3 position, Size3 size) {
	for (auto iter = size.iter(); iter; iter++) {
		insertCell(position + *iter, Cell(id));
	}
}

void BlockContainer3d::placeBlockCells(const Block3d* block) {
	for (auto iter = block->size().iter(); iter; iter++) {
		insertCell(block->getPosition() + *iter, Cell(block->id()));
	}
}

void BlockContainer3d::removeBlockCells(const Block3d* block) {
	for (auto iter = block->size().iter(); iter; iter++) {
		removeCell(block->getPosition() + *iter);
	}
}

Difference3d BlockContainer3d::getCreationDifference() const {
	Difference3d difference;
	for (const std::pair<const unsigned int, Block3d>& block : blocks) {
		difference.addPlacedBlock(block.second.getPosition(), block.second.getOrientation(), block.second.type());
	}
	for (const std::pair<const unsigned int, Block3d>& block : blocks) {
		for (auto& connectionIter : block.second.getConnectionContainer().getConnections()) {
			if (block.second.isConnectionInput(connectionIter.first)) continue;
			const auto connections = block.second.getConnectionContainer().getConnections(connectionIter.first);
			if (!connections) continue;
			if (block.second.isConnectionBidirectional(connectionIter.first)) {
				for (auto otherConnectionIter : *connections) {
					const Block3d* otherBlock = getBlock(otherConnectionIter.getBlockId());
					if (otherBlock->isConnectionOutput(otherConnectionIter.getConnectionId())) continue;
					if (otherBlock->isConnectionBidirectional(otherConnectionIter.getConnectionId()) && otherConnectionIter.getBlockId() > block.first) continue;
					difference.addCreatedConnection(
						block.second.getPosition(),
						block.second.getConnectionPosition(connectionIter.first).value(),
						otherBlock->getPosition(),
						otherBlock->getConnectionPosition(otherConnectionIter.getConnectionId()).value());
				}
			} else {
				for (auto otherConnectionIter : *connections) {
					const Block3d* otherBlock = getBlock(otherConnectionIter.getBlockId());
					difference.addCreatedConnection(
						block.second.getPosition(),
						block.second.getConnectionPosition(connectionIter.first).value(),
						otherBlock->getPosition(),
						otherBlock->getConnectionPosition(otherConnectionIter.getConnectionId()).value());
				}
			}
		}
	}
	return difference;
}

Difference3dSharedPtr BlockContainer3d::getCreationDifferenceShared() const {
	Difference3dSharedPtr difference = std::make_shared<Difference3d>();
	for (const std::pair<const unsigned int, Block3d>& block : blocks) {
		difference->addPlacedBlock(block.second.getPosition(), block.second.getOrientation(), block.second.type());
	}
	for (const std::pair<const unsigned int, Block3d>& block : blocks) {
		for (auto& connectionIter : block.second.getConnectionContainer().getConnections()) {
			if (block.second.isConnectionInput(connectionIter.first)) continue;
			auto connections = block.second.getConnectionContainer().getConnections(connectionIter.first);
			if (!connections) continue;
			// for (auto otherConnectionIter : *connections) {
			// 	difference->addCreatedConnection(
			// 		iter.second.getPosition(),
			// 		iter.second.getConnectionPosition(connectionIter.first).value(),
			// 		getBlock(otherConnectionIter.getBlockId())->getPosition(),
			// 		getBlock(otherConnectionIter.getBlockId())->getConnectionPosition(otherConnectionIter.getConnectionId()).value()
			// 	);
			// }
			if (block.second.isConnectionBidirectional(connectionIter.first)) {
				for (auto otherConnectionIter : *connections) {
					const Block3d* otherBlock = getBlock(otherConnectionIter.getBlockId());
					if (otherBlock->isConnectionOutput(otherConnectionIter.getConnectionId())) continue;
					if (otherBlock->isConnectionBidirectional(otherConnectionIter.getConnectionId()) && otherConnectionIter.getBlockId() > block.first) continue;
					difference->addCreatedConnection(
						block.second.getPosition(),
						block.second.getConnectionPosition(connectionIter.first).value(),
						otherBlock->getPosition(),
						otherBlock->getConnectionPosition(otherConnectionIter.getConnectionId()).value());
				}
			} else {
				for (auto otherConnectionIter : *connections) {
					const Block3d* otherBlock = getBlock(otherConnectionIter.getBlockId());
					difference->addCreatedConnection(
						block.second.getPosition(),
						block.second.getConnectionPosition(connectionIter.first).value(),
						otherBlock->getPosition(),
						otherBlock->getConnectionPosition(otherConnectionIter.getConnectionId()).value());
				}
			}
		}
	}
	return difference;
}

unsigned int BlockContainer3d::getBitwidthOfJunction(const Block3d* block) const {
	if (block == nullptr || block->type() != BlockType::JUNCTION) return 0; // will not work for anything but a junction
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			const Block3d* connectedBlock = getBlock(connection.getBlockId());
			if (connectedBlock->type() != BlockType::JUNCTION) {
				return getBlockDataManager().getBlockData(connectedBlock->type())->getConnectionBitWidth(connection.getConnectionId());
			}
		}
	}
	std::unordered_set<block_id_t> visited = {block->id()};
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			unsigned int bitWidth = getBitwidthOfJunction(connection.getBlockId(), visited);
			if (bitWidth != 0) return bitWidth;
		}
	}
	return 0;
}

unsigned int BlockContainer3d::getBitwidthOfJunction(block_id_t blockId, std::unordered_set<block_id_t>& visited) const {
	if (!visited.insert(blockId).second) return 0;
	const Block3d* block = getBlock(blockId);
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			const Block3d* connectedBlock = getBlock(connection.getBlockId());
			if (connectedBlock->type() != BlockType::JUNCTION) {
				return getBlockDataManager().getBlockData(connectedBlock->type())->getConnectionBitWidth(connection.getConnectionId());
			}
		}
	}
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			unsigned int bitWidth = getBitwidthOfJunction(connection.getBlockId(), visited);
			if (bitWidth != 0) return bitWidth;
		}
	}
	return 0;
}

unsigned int BlockContainer3d::getBitwidthOfJunctionIgnorePort(const Block3d* block, BlockType blockType, connection_end_id_t endId) const {
	if (block == nullptr || block->type() != BlockType::JUNCTION) return 0; // will not work for anything but a junction
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			const Block3d* connectedBlock = getBlock(connection.getBlockId());
			if (connectedBlock->type() != BlockType::JUNCTION && (connectedBlock->type() != blockType || connection.getConnectionId() != endId)) {
				return getBlockDataManager().getBlockData(connectedBlock->type())->getConnectionBitWidth(connection.getConnectionId());
			}
		}
	}
	std::unordered_set<block_id_t> visited = {block->id()};
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			const Block3d* connectedBlock = getBlock(connection.getBlockId());
			if (connectedBlock->type() != BlockType::JUNCTION) continue;
			unsigned int bitWidth = getBitwidthOfJunctionIgnorePort(connectedBlock, blockType, endId, visited);
			if (bitWidth != 0) return bitWidth;
		}
	}
	return 0;
}

unsigned int BlockContainer3d::getBitwidthOfJunctionIgnorePort(const Block3d* block, BlockType blockType, connection_end_id_t endId, std::unordered_set<block_id_t>& visited) const {
	if (!visited.insert(block->id()).second) return 0;
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			const Block3d* connectedBlock = getBlock(connection.getBlockId());
			if (connectedBlock->type() != BlockType::JUNCTION && (connectedBlock->type() != blockType || connection.getConnectionId() != endId)) {
				return getBlockDataManager().getBlockData(connectedBlock->type())->getConnectionBitWidth(connection.getConnectionId());
			}
		}
	}
	for (const auto& connections : block->getConnectionContainer().getConnections()) {
		for (const auto& connection : connections.second) {
			const Block3d* connectedBlock = getBlock(connection.getBlockId());
			if (connectedBlock->type() != BlockType::JUNCTION) continue;
			unsigned int bitWidth = getBitwidthOfJunctionIgnorePort(connectedBlock, blockType, endId, visited);
			if (bitWidth != 0) return bitWidth;
		}
	}
	return 0;
}

nlohmann::json BlockContainer3d::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	stateJson["selfBlockType"] = static_cast<unsigned int>(selfBlockType);
	stateJson["lastId"] = lastId;
	stateJson["blocks"] = nlohmann::json::object();
	for (const auto& [blockId, block] : blocks) {
		stateJson["blocks"][std::to_string(blockId)] = block.dumpState();
	}
	stateJson["grid"] = grid.dumpStateAndInner();
	stateJson["blockTypeCounts"] = blockTypeCounts;
	return stateJson;
}
