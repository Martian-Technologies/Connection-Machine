#include "circuit.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

#include "backend/proceduralCircuits/generatedCircuit.h"
#include "logging/logging.h"
#include "parsedCircuit.h"

Circuit::Circuit(circuit_id_t circuitId, CircuitManager* circuitManager, BlockDataManager* blockDataManager, DataUpdateEventManager* dataUpdateEventManager, const std::string& name, const std::string& uuid) :
	circuitId(circuitId), blockContainer(circuitManager, blockDataManager), circuitUUID(uuid), circuitName(name), dataUpdateEventManager(dataUpdateEventManager), dataUpdateEventReceiver(dataUpdateEventManager) {
	dataUpdateEventReceiver.linkFunction("preBlockSizeChange", std::bind(&Circuit::blockSizeChange, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("preBlockDataSetConnection", std::bind(&Circuit::addConnectionPort, this, std::placeholders::_1));
	dataUpdateEventReceiver.linkFunction("preBlockDataRemoveConnection", std::bind(&Circuit::removeConnectionPort, this, std::placeholders::_1));
}

void Circuit::clear(bool clearUndoTree) {
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	blockContainer.clear(difference.get());
	sendDifference(std::move(difference));
	if (clearUndoTree) {
		undoSystem.clear();
	}
}

void Circuit::connectListener(void* object, CircuitDiffListenerFunction func, unsigned int priority) {
	listenerFunctions.emplace_back(object, priority, func);
	std::sort(listenerFunctions.begin(), listenerFunctions.end(), [](const CircuitDiffListenerData& a, const CircuitDiffListenerData& b) { return a.priority < b.priority; });
}

void Circuit::disconnectListener(void* object) {
	auto iter = std::find_if(listenerFunctions.begin(), listenerFunctions.end(), [object](const CircuitDiffListenerData& other) { return object == other.obj; });
	if (iter != listenerFunctions.end())
		listenerFunctions.erase(iter);
}

bool Circuit::tryInsertBlock(Position position, Orientation orientation, BlockType blockType) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryInsertBlock(position, orientation, blockType, difference.get());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryRemoveBlock(Position position) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryRemoveBlock(position, difference.get());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryMoveBlock(Position positionOfBlock, Position position) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryMoveBlock(positionOfBlock, position, Orientation(), difference.get());
	assert(out != difference->empty());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryMoveBlocks(const SharedSelection& selection, Vector movement, Orientation transformAmount) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (movement == Vector(0) && transformAmount == Orientation()) return true;
	Position selectionOrigin = getSelectionOrigin(selection);
	Position newSelectionOrigin = selectionOrigin + movement;
	std::unordered_set<Position> positions;
	std::unordered_set<const Block*> blocks;
	flattenSelection(selection, positions);
	for (auto iter = positions.begin(); iter != positions.end(); ++iter) {
		const Block* block = blockContainer.getBlock(*iter);
		if (block) {
			if (blocks.contains(block) || blocks.contains(block)) continue;
			Position pos1 = newSelectionOrigin + transformAmount * (block->getPosition() - selectionOrigin);
			Position pos2 = newSelectionOrigin + transformAmount * (block->getLargestPosition() - selectionOrigin);
			for (auto iter = pos1.iterTo(pos2); iter; iter++) {
				const Block* otherBlock = blockContainer.getBlock(*iter);
				if (otherBlock == nullptr || otherBlock == block) continue;
				if (!positions.contains(*iter)) {
					return false;
				}
			}
			blocks.insert(block);
		}
	}

	DifferenceSharedPtr difference1 = std::make_shared<Difference>();
	std::vector<Position> stackToMoveBack;
	for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
		const Block* block = *iter;
		Position posToMoveTo = (
			newSelectionOrigin +
			transformAmount * (block->getPosition() - selectionOrigin) -
			transformAmount.transformVectorWithArea(Vector(0), block->size())
		);
		if (!blockContainer.tryMoveBlock(
			block->getPosition(),
			posToMoveTo,
			transformAmount,
			difference1.get())
		) {
			stackToMoveBack.push_back(posToMoveTo);
			pushOntoStack(block->getPosition(), difference1.get());
			continue;
		}
	}
	sendDifference(std::move(difference1));
	if (stackToMoveBack.empty()) return true;
	DifferenceSharedPtr difference2 = std::make_shared<Difference>();
	for (unsigned int i = stackToMoveBack.size(); i > 0; i--) {
		popOffStack(stackToMoveBack[i-1], transformAmount, false, difference2.get());
	}
	sendDifference(difference2);
	return true;
}

void Circuit::setType(const SharedSelection& selection, BlockType type) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	setType(selection, type, difference.get());
	sendDifference(std::move(difference));
}

void Circuit::setType(const SharedSelection& selection, BlockType type, Difference* difference) {

	// Cell Selection
	SharedCellSelection cellSelection = selectionCast<CellSelection>(selection);
	if (cellSelection) {
		blockContainer.trySetType(cellSelection->getPosition(), type, difference);
	}

	// Dimensional Selection
	SharedDimensionalSelection dimensionalSelection = selectionCast<DimensionalSelection>(selection);
	if (dimensionalSelection) {
		for (dimensional_selection_size_t i = dimensionalSelection->size(); i > 0; i--) {
			setType(dimensionalSelection->getSelection(i - 1), type, difference);
		}
	}
}

void Circuit::tryInsertOverArea(Position cellA, Position cellB, Orientation transformAmount, BlockType blockType) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (cellA.x > cellB.x) std::swap(cellA.x, cellB.x);
	if (cellA.y > cellB.y) std::swap(cellA.y, cellB.y);

	DifferenceSharedPtr difference = std::make_shared<Difference>();
	for (coordinate_t x = cellA.x; x <= cellB.x; x++) {
		for (coordinate_t y = cellA.y; y <= cellB.y; y++) {
			blockContainer.tryInsertBlock(Position(x, y), transformAmount, blockType, difference.get());
		}
	}
	sendDifference(std::move(difference));
}

void Circuit::tryRemoveOverArea(Position cellA, Position cellB) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (cellA.x > cellB.x) std::swap(cellA.x, cellB.x);
	if (cellA.y > cellB.y) std::swap(cellA.y, cellB.y);

	DifferenceSharedPtr difference = std::make_shared<Difference>();
	for (coordinate_t x = cellA.x; x <= cellB.x; x++) {
		for (coordinate_t y = cellA.y; y <= cellB.y; y++) {
			blockContainer.tryRemoveBlock(Position(x, y), difference.get());
		}
	}
	sendDifference(std::move(difference));
}

bool Circuit::checkCollision(const SharedSelection& selection) {
	// Cell Selection
	SharedCellSelection cellSelection = selectionCast<CellSelection>(selection);
	if (cellSelection) {
		return blockContainer.checkCollision(cellSelection->getPosition());
	}

	// Dimensional Selection
	SharedDimensionalSelection dimensionalSelection = selectionCast<DimensionalSelection>(selection);
	if (dimensionalSelection) {
		for (dimensional_selection_size_t i = dimensionalSelection->size(); i > 0; i--) {
			if (checkCollision(dimensionalSelection->getSelection(i - 1))) return true;
		}
	}
	return false;
}

bool Circuit::tryInsertParsedCircuit(const ParsedCircuit& parsedCircuit, Position position) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (!parsedCircuit.isValid()) return false;

	for (const auto& [oldId, block] : parsedCircuit.getBlocks()) {
		if (blockContainer.checkCollision(block.position.snap(), block.orientation, block.type)) {
			return false;
		}
	}
	logInfo("all blocks can be placed");

	DifferenceSharedPtr difference = std::make_shared<Difference>();

	std::unordered_map<block_id_t, block_id_t> realIds;
	for (const auto& [oldId, block] : parsedCircuit.getBlocks()) {
		Position targetPos = block.position.snap();
		block_id_t newId;
		if (!blockContainer.tryInsertBlock(targetPos, block.orientation, block.type, difference.get())) {
			logError("Failed to insert block while inserting block.", "Circuit");
		} else {
			realIds[oldId] = blockContainer.getBlock(targetPos)->id();
		}
	}

	for (const auto& conn : parsedCircuit.getConns()) {
		const ParsedCircuit::BlockData* parsedBlock = parsedCircuit.getBlock(conn.outputBlockId);
		if (!parsedBlock) {
			logError("Could not get block from parsed circuit while inserting block.", "Circuit");
			continue;
		}
		if (blockContainer.getBlockDataManager()->isConnectionInput(parsedBlock->type, conn.outputEndId)) {
			// skip inputs
			continue;
		}

		ConnectionEnd output(realIds[conn.outputBlockId], conn.outputEndId);
		ConnectionEnd input(realIds[conn.inputBlockId], conn.inputEndId);
		if (!blockContainer.tryCreateConnection(output, input, difference.get())) {
			logError("Failed to create connection while inserting block (could be a duplicate connection in parsing):[{},{}] -> [{},{}]", "", conn.inputBlockId, conn.inputEndId, conn.outputBlockId, conn.outputEndId);
		}
	}
	sendDifference(std::move(difference));
	return true;
}

bool Circuit::tryInsertGeneratedCircuit(const GeneratedCircuit& generatedCircuit, Position position) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (!generatedCircuit.isValid()) return false;

	for (const auto& [oldId, block] : generatedCircuit.getBlocks()) {
		if (blockContainer.checkCollision(block.position, block.orientation, block.type)) {
			return false;
		}
	}
	logInfo("all blocks can be placed");

	DifferenceSharedPtr difference = std::make_shared<Difference>();

	std::unordered_map<block_id_t, block_id_t> realIds;
	for (const auto& [oldId, block] : generatedCircuit.getBlocks()) {
		Position targetPos = block.position;
		block_id_t newId;
		if (!blockContainer.tryInsertBlock(targetPos, block.orientation, block.type, difference.get())) {
			logError("Failed to insert block while inserting block.", "Circuit");
		} else {
			realIds[oldId] = blockContainer.getBlock(targetPos)->id();
		}
	}

	for (const auto& conn : generatedCircuit.getConns()) {
		const GeneratedCircuit::GeneratedCircuitBlockData* parsedBlock = generatedCircuit.getBlock(conn.outputBlockId);
		if (!parsedBlock) {
			logError("Could not get block from parsed circuit while inserting block.", "Circuit");
			continue;
		}
		if (blockContainer.getBlockDataManager()->isConnectionInput(parsedBlock->type, conn.outputId)) {
			// skip inputs
			continue;
		}

		ConnectionEnd output(realIds[conn.outputBlockId], conn.outputId);
		ConnectionEnd input(realIds[conn.inputBlockId], conn.inputId);
		if (!blockContainer.tryCreateConnection(output, input, difference.get())) {
			logError("Failed to create connection while inserting block (could be a duplicate connection in parsing):[{},{}] -> [{},{}]", "", conn.inputBlockId, conn.inputId, conn.outputBlockId, conn.outputId);
		}
	}
	sendDifference(std::move(difference));
	return true;
}

bool Circuit::tryInsertCopiedBlocks(const SharedCopiedBlocks& copiedBlocks, Position position, Orientation transformAmount) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	Vector totalOffset = Vector(position.x, position.y) + (Position() - copiedBlocks->getMinPosition());
	for (const CopiedBlocks::CopiedBlockData& block : copiedBlocks->getCopiedBlocks()) {
		if (blockContainer.checkCollision(
			position + transformAmount * (block.position - copiedBlocks->getMinPosition()) - transformAmount.transformVectorWithArea(Vector(0), blockContainer.getBlockDataManager()->getBlockSize(block.blockType, block.orientation)),
			transformAmount * block.orientation,
			block.blockType
		)) {
			return false;
		}
	}
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	for (const CopiedBlocks::CopiedBlockData& block : copiedBlocks->getCopiedBlocks()) {
		if (!blockContainer.tryInsertBlock(
			position + transformAmount * (block.position - copiedBlocks->getMinPosition()) - transformAmount.transformVectorWithArea(Vector(0), blockContainer.getBlockDataManager()->getBlockSize(block.blockType, block.orientation)),
			transformAmount * block.orientation,
			block.blockType, difference.get()
		)) {
			logError("Failed to insert block while inserting block.");
		}
	}
	for (const std::pair<Position, Position>& conn : copiedBlocks->getCopiedConnections()) {
		if (!blockContainer.tryCreateConnection(
			position + transformAmount * (conn.second - copiedBlocks->getMinPosition()),
			position + transformAmount * (conn.first - copiedBlocks->getMinPosition()),
			difference.get()
		)) {
			logError("Failed to create connection while inserting block.");
		}
	}
	sendDifference(std::move(difference));
	return true;
}

bool Circuit::tryCreateConnection(Position outputPosition, Position inputPosition) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryCreateConnection(outputPosition, inputPosition, difference.get());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryRemoveConnection(Position outputPosition, Position inputPosition) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryRemoveConnection(outputPosition, inputPosition, difference.get());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryCreateConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryCreateConnection(outputConnectionEnd, inputConnectionEnd, difference.get());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryRemoveConnection(ConnectionEnd outputConnectionEnd, ConnectionEnd inputConnectionEnd) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	bool out = blockContainer.tryRemoveConnection(outputConnectionEnd, inputConnectionEnd, difference.get());
	sendDifference(std::move(difference));
	return out;
}

bool Circuit::tryCreateConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (!sameSelectionShape(outputSelection, inputSelection)) return false;
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	createConnection(std::move(outputSelection), std::move(inputSelection), difference.get());
	sendDifference(std::move(difference));
	return true;
}

bool Circuit::tryRemoveConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	if (!sameSelectionShape(outputSelection, inputSelection)) return false;
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	removeConnection(std::move(outputSelection), std::move(inputSelection), difference.get());
	sendDifference(std::move(difference));
	return true;
}

void Circuit::createConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection, Difference* difference) {
	// Cell Selection
	SharedCellSelection outputCellSelection = selectionCast<CellSelection>(outputSelection);
	SharedCellSelection inputCellSelection = selectionCast<CellSelection>(inputSelection);
	if (outputCellSelection && inputCellSelection) {
		blockContainer.tryCreateConnection(outputCellSelection->getPosition(), inputCellSelection->getPosition(), difference);
		return;
	}

	// Dimensional Selection
	SharedDimensionalSelection outputDimensionalSelection = selectionCast<DimensionalSelection>(outputSelection);
	SharedDimensionalSelection inputDimensionalSelection = selectionCast<DimensionalSelection>(inputSelection);
	if (outputDimensionalSelection && inputDimensionalSelection) {
		if (outputDimensionalSelection->size() == 1) {
			for (dimensional_selection_size_t i = inputDimensionalSelection->size(); i > 0; i--) {
				createConnection(outputDimensionalSelection->getSelection(0), inputDimensionalSelection->getSelection(i - 1), difference);
			}
		} else if (inputDimensionalSelection->size() == 1) {
			for (dimensional_selection_size_t i = outputDimensionalSelection->size(); i > 0; i--) {
				createConnection(outputDimensionalSelection->getSelection(i - 1), inputDimensionalSelection->getSelection(0), difference);
			}
		} else {
			for (dimensional_selection_size_t i = inputDimensionalSelection->size(); i > 0; i--) {
				createConnection(outputDimensionalSelection->getSelection(i - 1), inputDimensionalSelection->getSelection(i - 1), difference);
			}
		}
	}
}

void Circuit::removeConnection(const SharedSelection& outputSelection, const SharedSelection& inputSelection, Difference* difference) {
	// Cell Selection
	const SharedCellSelection& outputCellSelection = selectionCast<CellSelection>(outputSelection);
	const SharedCellSelection& inputCellSelection = selectionCast<CellSelection>(inputSelection);
	if (outputCellSelection && inputCellSelection) {
		blockContainer.tryRemoveConnection(outputCellSelection->getPosition(), inputCellSelection->getPosition(), difference);
		return;
	}

	// Dimensional Selection
	SharedDimensionalSelection outputDimensionalSelection = selectionCast<DimensionalSelection>(outputSelection);
	SharedDimensionalSelection inputDimensionalSelection = selectionCast<DimensionalSelection>(inputSelection);
	if (outputDimensionalSelection && inputDimensionalSelection) {
		if (outputDimensionalSelection->size() == 1) {
			for (dimensional_selection_size_t i = inputDimensionalSelection->size(); i > 0; i--) {
				removeConnection(outputDimensionalSelection->getSelection(0), inputDimensionalSelection->getSelection(i - 1), difference);
			}
		} else if (inputDimensionalSelection->size() == 1) {
			for (dimensional_selection_size_t i = outputDimensionalSelection->size(); i > 0; i--) {
				removeConnection(outputDimensionalSelection->getSelection(i - 1), inputDimensionalSelection->getSelection(0), difference);
			}
		} else {
			for (dimensional_selection_size_t i = inputDimensionalSelection->size(); i > 0; i--) {
				removeConnection(outputDimensionalSelection->getSelection(i - 1), inputDimensionalSelection->getSelection(i - 1), difference);
			}
		}
	}
}

void Circuit::undo() {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr newDifference = std::make_shared<Difference>();
	const MinimalDifference* difference = undoSystem.undoDifference();
	if (!difference) return;
	startUndo();
	MinimalDifference::block_modification_t blockModification;
	MinimalDifference::connection_modification_t connectionModification;
	MinimalDifference::move_modification_t moveModification;
	const std::vector<MinimalDifference::Modification>& modifications = difference->getModifications();
	bool doAnother = false;
	for (unsigned int i = modifications.size(); i > 0; --i) {
		const MinimalDifference::Modification& modification = modifications[i - 1];
		switch (modification.first) {
		case MinimalDifference::PLACE_BLOCK:
			blockContainer.tryRemoveBlock(std::get<0>(std::get<MinimalDifference::block_modification_t>(modification.second)), newDifference.get());
			break;
		case MinimalDifference::REMOVED_BLOCK:
			blockModification = std::get<MinimalDifference::block_modification_t>(modification.second);
			blockContainer.tryInsertBlock(std::get<0>(blockModification), std::get<1>(blockModification), std::get<2>(blockModification), newDifference.get());
			break;
		case MinimalDifference::CREATED_CONNECTION:
			connectionModification = std::get<MinimalDifference::connection_modification_t>(modification.second);
			blockContainer.tryRemoveConnection(connectionModification.first, connectionModification.second, newDifference.get());
			break;
		case MinimalDifference::REMOVED_CONNECTION:
			connectionModification = std::get<MinimalDifference::connection_modification_t>(modification.second);
			blockContainer.tryCreateConnection(connectionModification.first, connectionModification.second, newDifference.get());
			break;
		case MinimalDifference::MOVE_BLOCK:
			moveModification = std::get<MinimalDifference::move_modification_t>(modification.second);
			MoveType moveType = std::get<4>(moveModification);
			if (moveType == MoveType::MULTI_BEGIN) moveType = MoveType::MULTI_FINAL;
			else if (moveType == MoveType::MULTI_FINAL) moveType = MoveType::MULTI_BEGIN;
			blockContainer.tryMoveBlock(std::get<2>(moveModification), std::get<0>(moveModification), std::get<1>(moveModification).relativeTo(std::get<3>(moveModification)), newDifference.get(), moveType);
			if (moveType == MoveType::MULTI_BEGIN || moveType == MoveType::MULTI_MIDDLE) {
				doAnother = true;
			}
			break;
		}
	}
	sendDifference(newDifference);
	endUndo();
	if (doAnother) {
		undo();
	}
}

void Circuit::redo() {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	DifferenceSharedPtr newDifference = std::make_shared<Difference>();
	const MinimalDifference* difference = undoSystem.redoDifference();
	if (!difference) return;
	startUndo();
	MinimalDifference::block_modification_t blockModification;
	MinimalDifference::connection_modification_t connectionModification;
	MinimalDifference::move_modification_t moveModification;
	bool doAnother = false;
	for (auto modification : difference->getModifications()) {
		switch (modification.first) {
		case MinimalDifference::REMOVED_BLOCK:
			blockContainer.tryRemoveBlock(std::get<0>(std::get<MinimalDifference::block_modification_t>(modification.second)), newDifference.get());
			break;
		case MinimalDifference::PLACE_BLOCK:
			blockModification = std::get<MinimalDifference::block_modification_t>(modification.second);
			blockContainer.tryInsertBlock(std::get<0>(blockModification), std::get<1>(blockModification), std::get<2>(blockModification), newDifference.get());
			break;
		case MinimalDifference::REMOVED_CONNECTION:
			connectionModification = std::get<MinimalDifference::connection_modification_t>(modification.second);
			blockContainer.tryRemoveConnection(connectionModification.first, connectionModification.second, newDifference.get());
			break;
		case MinimalDifference::CREATED_CONNECTION:
			connectionModification = std::get<MinimalDifference::connection_modification_t>(modification.second);
			blockContainer.tryCreateConnection(connectionModification.first, connectionModification.second, newDifference.get());
			break;
		case MinimalDifference::MOVE_BLOCK:
			moveModification = std::get<MinimalDifference::move_modification_t>(modification.second);
			blockContainer.tryMoveBlock(std::get<0>(moveModification), std::get<2>(moveModification), std::get<3>(moveModification).relativeTo(std::get<1>(moveModification)), newDifference.get(), std::get<4>(moveModification));
			if (std::get<4>(moveModification) == MoveType::MULTI_BEGIN || std::get<4>(moveModification) == MoveType::MULTI_MIDDLE) {
				doAnother = true;
			}
			break;
		}
	}
	sendDifference(newDifference);
	endUndo();
	if (doAnother) {
		redo();
	}
}

void Circuit::blockSizeChange(const DataUpdateEventManager::EventData* eventData) {
	if (!eventData) {
		logError("eventData passed was null", "Circuit");
		undoSystem.addBlocker(); // cant undo after changing block size!
		return;
	}
	auto data = eventData->cast<std::pair<BlockType, Size>>();
	if (!data) {
		logError("Could not get std::pair<BlockType, Size>> from eventData", "Circuit");
		undoSystem.addBlocker(); // cant undo after changing block size!
		return;
	}
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	blockContainer.resizeBlockType(data->get().first, data->get().second, difference.get());
	sendDifference(std::move(difference));
	undoSystem.addBlocker(); // cant undo after changing block size!
}

void Circuit::pushOntoStack(Position blockPosition, Difference * difference, MoveType moveType) {
	assert(moveType != MoveType::SINGLE && moveType != MoveType::MULTI_FINAL && "Can't push blocks to stack and leave them there.");
	const Block* block = blockContainer.getBlock(blockPosition);
	if (!block) {
		logError("Can't find block at {} to put on stack.", "Circuit", blockPosition);
		return;
	}
	blockContainer.tryMoveBlock(block->getPosition(), stackTop, Orientation(), difference, moveType);
	stackTop.y += block->size().h;
}

void Circuit::popOffStack(Position position, Orientation transformAmount, bool resetRotation, Difference * difference, MoveType moveType) {
	assert(moveType != MoveType::SINGLE && "Can't pop blocks off stack with move type single because they have to had been moved there with multi.");
	if (stackTop == stackBottom) {
		logError("Can't pop off empty stack", "Circuit");
		return;
	}
	stackTop.y -= 1; // shifts stackTop to look at the top block
	const Block* block = blockContainer.getBlock(stackTop);
	if (!block) {
		logError("Can't find block on stack, this should never happen", "Circuit");
		return;
	}
	stackTop.y -= block->size().h-1;
	if (resetRotation) transformAmount *= block->getOrientation().inverse();
	blockContainer.tryMoveBlock(block->getPosition(), position, transformAmount, difference, moveType);
}

void Circuit::setBlockType(BlockType blockType) {
	blockContainer.setBlockType(blockType);
	blockContainer.getBlockDataManager()->getBlockData(blockType)->setName(getCircuitNameNumber());
}

void Circuit::addConnectionPort(const DataUpdateEventManager::EventData* eventData) {
	if (!eventData) {
		logError("eventData passed was null", "Circuit");
		return;
	}
	auto data = eventData->cast<std::pair<BlockType, connection_end_id_t>>();
	if (!data) {
		logError("Could not get std::pair<BlockType, Vector>> from eventData", "Circuit");
		return;
	}
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	blockContainer.addConnectionPort(data->get().first, data->get().second, difference.get());
	sendDifference(std::move(difference));
}

void Circuit::removeConnectionPort(const DataUpdateEventManager::EventData* eventData) {
	if (!eventData) {
		logError("eventData passed was null", "Circuit");
		return;
	}
	auto data = eventData->cast<std::pair<BlockType, connection_end_id_t>>();
	if (!data) {
		logError("Could not get std::pair<BlockType, Vector>> from eventData", "Circuit");
		return;
	}
	DifferenceSharedPtr difference = std::make_shared<Difference>();
	blockContainer.removeConnectionPort(data->get().first, data->get().second, difference.get());
	sendDifference(std::move(difference));
}

void Circuit::setCircuitName(const std::string& name) {
	circuitName = name;
	if (blockContainer.getBlockType() == BlockType::NONE) return;
	BlockData* blockData = blockContainer.getBlockDataManager()->getBlockData(blockContainer.getBlockType());
	if (blockData) blockData->setName(getCircuitNameNumber());
}
