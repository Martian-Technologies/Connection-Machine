#include "evaluator.h"

#include "backend/circuit/circuitManager.h"
#include "util/algorithm.h"
#include "util/id.h"
#include "layers/layer2_evalSimulator.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

Evaluator::~Evaluator() = default;

Evaluator::Evaluator(
	evaluator_id_t evaluatorId,
	CircuitManager& circuitManager,
	BlockDataManager& blockDataManager,
	CircuitBlockDataManager& circuitBlockDataManager,
	circuit_id_t circuitId,
	DataUpdateEventManager& dataUpdateEventManager
) :
	evaluatorId(evaluatorId), circuitManager(circuitManager), blockDataManager(blockDataManager), circuitBlockDataManager(circuitBlockDataManager),
	evalCircuitContainer(), dataUpdateEventManager(dataUpdateEventManager), receiver(dataUpdateEventManager), evalConfig(dataUpdateEventManager, evaluatorId),
	middleIdProvider(1), evalSimulator(std::make_unique<EvalSimulator>(evalConfig, middleIdProvider, dirtySimulatorIds, dirtyMiddleIds, blockDataManager)) {
	const auto circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) {
		logError("Circuit with ID {} not found", "Evaluator::Evaluator", circuitId);
		return;
	}
	logInfo("Creating Evaluator with ID {} for Circuit ID {}", "Evaluator", evaluatorId, circuitId);
	evalCircuitContainer.addCircuit(eval_circuit_id_t(0), circuitId);
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	const Difference difference = blockContainer.getCreationDifference();
	receiver.linkFunction("circuitBlockDataConnectionPositionRemove", std::bind(&Evaluator::removeCircuitIO, this, std::placeholders::_1));
	receiver.linkFunction("circuitBlockDataConnectionPositionSet", std::bind(&Evaluator::setCircuitIO, this, std::placeholders::_1));

	makeEdit(std::make_shared<Difference>(difference), circuitId);
}

std::string Evaluator::getEvaluatorName() const {
	std::optional<circuit_id_t> circuitId = evalCircuitContainer.getCircuitId(eval_circuit_id_t(0));
	if (!circuitId.has_value()) {
		return "Eval " + std::to_string(evaluatorId) + " (No Circuit)";
	}
	auto circuit = circuitManager.getCircuit(circuitId.value());
	if (!circuit) {
		return "Eval " + std::to_string(evaluatorId) + " (Invalid Circuit)";
	}
	return "Eval " + std::to_string(evaluatorId) + " (" + circuit->getCircuitNameNumber() + ")";
}

void Evaluator::makeEdit(DifferenceSharedPtr difference, circuit_id_t circuitId) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	changedICs = false;
	// logInfo("_________________________________________________________________________________________");
	// logInfo("Applying edit to Evaluator with ID {} for Circuit ID {}", "Evaluator::makeEdit", evaluatorId, circuitId);
	{
		SimPauseGuard pauseGuard = evalSimulator->beginEdit();
		std::unique_lock lk(simMutex);
		DiffCache diffCache(circuitManager);
		for (eval_circuit_id_t evalCircuitId : evalCircuitContainer.ids()) {
			if (evalCircuitContainer.getCircuitId(evalCircuitId) == circuitId) {
				makeEditInPlace(pauseGuard, evalCircuitId, difference, diffCache);
			}
		}
		evalSimulator->endEdit(pauseGuard);
	}
	if (changedICs) {
		dataUpdateEventManager.sendEvent("addressTreeMakeBranch");
	}
	processDirtyNodes();
}

void Evaluator::makeEditInPlace(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DifferenceSharedPtr difference, DiffCache& diffCache) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif

	if (difference->clearsAll()) {
		edit_deleteICContents(pauseGuard, evalCircuitId);
		return;
	}

	std::optional<circuit_id_t> circuitId = evalCircuitContainer.getCircuitId(evalCircuitId);
	if (!circuitId.has_value()) {
		logError("EvalCircuit with id {} not found", "Evaluator::makeEditInPlace", evalCircuitId);
		return;
	}
	SharedCircuit circuit = circuitManager.getCircuit(circuitId.value());
	if (!circuit) {
		logError("Circuit with id {} not found", "Evaluator::makeEditInPlace", circuitId.value());
		return;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();

	const std::vector<Difference::Modification>& modifications = difference->getModifications();
	for (const Difference::Modification& modification : modifications) {
		const auto& [modificationType, modificationData] = modification;
		switch (modificationType) {
		case Difference::ModificationType::REMOVED_BLOCK: {
			const auto& [position, orientation, blockType] = std::get<Difference::block_modification_t>(modificationData);
			edit_removeBlock(pauseGuard, evalCircuitId, diffCache, position, orientation, blockType);
			break;
		}
		case Difference::ModificationType::PLACE_BLOCK: {
			const auto& [position, orientation, blockType] = std::get<Difference::block_modification_t>(modificationData);
			edit_placeBlock(pauseGuard, evalCircuitId, diffCache, position, orientation, blockType);
			break;
		}
		case Difference::ModificationType::MOVE_BLOCK: {
			const auto& [curPosition, curOrientation, newPosition, newOrientation, finalMove] = std::get<Difference::move_modification_t>(modificationData);
			edit_moveBlock(pauseGuard, evalCircuitId, diffCache, curPosition, curOrientation, newPosition, newOrientation, finalMove);
			break;
		}
		case Difference::ModificationType::REMOVED_CONNECTION: {
			const auto& [outputBlockPosition, outputPosition, inputBlockPosition, inputPosition] = std::get<Difference::connection_modification_t>(modificationData);
			edit_removeConnection(pauseGuard, evalCircuitId, diffCache, blockContainer, outputBlockPosition, outputPosition, inputBlockPosition, inputPosition);
			break;
		}
		case Difference::ModificationType::CREATED_CONNECTION: {
			const auto& [outputBlockPosition, outputPosition, inputBlockPosition, inputPosition] = std::get<Difference::connection_modification_t>(modificationData);
			edit_createConnection(pauseGuard, evalCircuitId, diffCache, blockContainer, outputBlockPosition, outputPosition, inputBlockPosition, inputPosition);
			break;
		}
		}
	}
}

void Evaluator::edit_removeBlock(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	DiffCache& diffCache,
	Position position,
	Orientation orientation,
	BlockType type
) {
	// Find the circuit and remove the block
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_removeBlock", evalCircuitId);
		return;
	}
	std::optional<CircuitNode> node = evalCircuit->getNode(position);
	if (!node.has_value()) {
		logError("Node at position {} not found", "Evaluator::edit_removeBlock", position.toString());
		return;
	}
	circuitNodeToBlockTypeMap.erase(node.value());
	removeDependentInterCircuitConnections(pauseGuard, node.value());
	if (node->isIC()) {
		eval_circuit_id_t icId = node->getEvalCircuitId();
		EvalCircuit* icCircuit = evalCircuitContainer.getCircuit(icId);
		edit_deleteICContents(pauseGuard, icId);
		evalCircuitContainer.removeCircuit(icId);
		changedICs = true;
		evalCircuit->removeNode(position);
		return;
	}
	middle_id_t gateId = node->getMiddleId();
	middleIdToEvalPositionMap.erase(gateId);
	evalSimulator->removeGate(pauseGuard, gateId);
	middleIdProvider.releaseId(gateId);
	evalCircuit->removeNode(position);
}

void Evaluator::edit_deleteICContents(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId) {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_deleteIC", evalCircuitId);
		return;
	}
	std::vector<std::pair<Position, CircuitNode>> nodesToRemove;
	evalCircuit->forEachNode([&](Position pos, const CircuitNode& node) {
		nodesToRemove.emplace_back(pos, node);
	});
	for (const auto& [pos, node] : nodesToRemove) {
		removeDependentInterCircuitConnections(pauseGuard, node);
		if (node.isIC()) {
			eval_circuit_id_t icId = node.getEvalCircuitId();
			edit_deleteICContents(pauseGuard, icId);
			evalCircuitContainer.removeCircuit(icId);
			changedICs = true;
		} else {
			middle_id_t gateId = node.getMiddleId();
			circuitNodeToBlockTypeMap.erase(node);
			middleIdToEvalPositionMap.erase(gateId);
			evalSimulator->removeGate(pauseGuard, gateId);
			middleIdProvider.releaseId(gateId);
		}
		evalCircuit->removeNode(pos);
	}
}

void Evaluator::edit_placeBlock(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	DiffCache& diffCache,
	Position position,
	Orientation orientation,
	BlockType blockType
) {
	const circuit_id_t ICId = circuitBlockDataManager.getCircuitId(blockType);
	if (ICId != 0) {
		edit_placeIC(pauseGuard, evalCircuitId, diffCache, position, orientation, ICId);
		return;
	}

	middle_id_t gateId = middleIdProvider.getNewId();
	evalSimulator->addGate(pauseGuard, blockType, gateId);
	middleIdToEvalPositionMap[gateId] = { position, evalCircuitId };
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_placeBlock", evalCircuitId);
		return;
	}
	CircuitNode node = CircuitNode::fromMiddle(gateId);
	evalCircuit->setNode(position, node);
	circuitNodeToBlockTypeMap[node] = blockType;
	dirtyBlockAt(position, evalCircuitId);
	checkToCreateExternalConnections(pauseGuard, evalCircuitId, position);
}

void Evaluator::edit_placeIC(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	DiffCache& diffCache,
	Position position,
	Orientation orientation,
	circuit_id_t circuitId
) {
	changedICs = true;
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_placeIC", evalCircuitId);
		return;
	}
	eval_circuit_id_t newEvalCircuitId = evalCircuitContainer.addCircuit(evalCircuitId, circuitId);
	evalCircuit->setNode(position, CircuitNode::fromIC(newEvalCircuitId));
	dirtyBlockAt(position, evalCircuitId);
	DifferenceSharedPtr diff = diffCache.getDifference(circuitId);
	makeEditInPlace(pauseGuard, newEvalCircuitId, diff, diffCache);
}

void Evaluator::edit_removeConnection(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	DiffCache& diffCache,
	const BlockContainer& blockContainer,
	Position outputBlockPosition,
	Position outputPosition,
	Position inputBlockPosition,
	Position inputPosition
) {
	std::optional<EvalConnectionPoint> outputPoint = getConnectionPoint(evalCircuitId, outputPosition, Direction::OUT);
	if (!outputPoint.has_value()) {
		// logError("Output connection point not found for position {}", "Evaluator::edit_removeConnection", outputPosition.toString());
		return;
	}
	std::optional<EvalConnectionPoint> inputPoint = getConnectionPoint(evalCircuitId, inputPosition, Direction::IN);
	if (!inputPoint.has_value()) {
		// logError("Input connection point not found for position {}", "Evaluator::edit_removeConnection", inputPosition.toString());
		return;
	}
	EvalConnection connection(outputPoint.value(), inputPoint.value());
	auto it = std::find_if(interCircuitConnections.begin(), interCircuitConnections.end(), [&connection](const auto& pair) { return pair.connection == connection; });
	if (it != interCircuitConnections.end()) {
		interCircuitConnections.erase(it);
	}
	evalSimulator->removeConnection(pauseGuard, connection);
}

void Evaluator::edit_createConnection(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	DiffCache& diffCache,
	const BlockContainer& blockContainer,
	Position outputBlockPosition,
	Position outputPosition,
	Position inputBlockPosition,
	Position inputPosition
) {
	std::set<CircuitPortDependency> circuitPortDependencies;
	std::set<CircuitNode> circuitNodeDependencies;
	std::set<EvalPosition> visitedEvalPositions;

	std::optional<EvalConnectionPoint> outputPoint =
		getConnectionPoint(evalCircuitId, outputPosition, Direction::OUT, circuitPortDependencies, circuitNodeDependencies, visitedEvalPositions, false);
	if (!outputPoint.has_value()) {
		return;
	}
	std::optional<EvalConnectionPoint> inputPoint =
		getConnectionPoint(evalCircuitId, inputPosition, Direction::IN, circuitPortDependencies, circuitNodeDependencies, visitedEvalPositions, false);
	if (!inputPoint.has_value()) {
		return;
	}
	EvalConnection connection(outputPoint.value(), inputPoint.value());

	// if (!checkIfBitWidthsMatch(connection)) {
	// 	return;
	// }

	if (!circuitPortDependencies.empty() || !circuitNodeDependencies.empty()) {
		interCircuitConnections.push_back({ connection, circuitPortDependencies, circuitNodeDependencies });
	}
	for (const EvalPosition& evalPosition : visitedEvalPositions) {
		dirtyNodes.insert(evalPosition);
	}
	evalSimulator->makeConnection(pauseGuard, connection);
}

// bool Evaluator::checkIfBitWidthsMatch(
// 	const EvalConnection& connection
// ) const {
// 	CircuitNode outputNode = CircuitNode::fromMiddle(connection.source.gateId);
// 	CircuitNode inputNode = CircuitNode::fromMiddle(connection.destination.gateId);
// 	if (!circuitNodeToBlockTypeMap.contains(outputNode) || !circuitNodeToBlockTypeMap.contains(inputNode)) {
// 		logError("CircuitNode not found in circuitNodeToBlockTypeMap", "Evaluator::checkIfBitWidthsMatch");
// 		return false;
// 	}
// 	BlockType outputBlockType = circuitNodeToBlockTypeMap.at(outputNode);
// 	unsigned int outputBitWidth;
// 	if (outputBlockType == BlockType::SWITCH || outputBlockType == BlockType::BUTTON || outputBlockType == BlockType::TICK_BUTTON) {
// 		outputBitWidth = 1;
// 	} else {
// 		BlockData* outputBlockData = blockDataManager.getBlockData(outputBlockType);
// 		connection_end_id_t outputConnectionEndId = connection.source.portId;
// 		outputBitWidth = outputBlockData->getConnectionBitWidth(outputConnectionEndId);
// 	}

// 	BlockType inputBlockType = circuitNodeToBlockTypeMap.at(inputNode);
// 	unsigned int inputBitWidth;
// 	if (inputBlockType == BlockType::LIGHT) {
// 		inputBitWidth = 1;
// 	} else{
// 		BlockData* inputBlockData = blockDataManager.getBlockData(inputBlockType);
// 		connection_end_id_t inputConnectionEndId = connection.destination.portId;
// 		inputBitWidth = inputBlockData->getConnectionBitWidth(inputConnectionEndId);
// 	}

// 	return outputBitWidth == inputBitWidth;
// }

void Evaluator::removeDependentInterCircuitConnections(SimPauseGuard& pauseGuard, CircuitPortDependency circuitPortDependency) {
	// delete any connections that have the pair {circuitId, connectionEndId} in their traceSet
	for (auto it = interCircuitConnections.begin(); it != interCircuitConnections.end();) {
		if (it->circuitPortDependencies.find(circuitPortDependency) != it->circuitPortDependencies.end()) {
			evalSimulator->removeConnection(pauseGuard, it->connection);
			it = interCircuitConnections.erase(it);
		} else {
			++it;
		}
	}
}

void Evaluator::removeDependentInterCircuitConnections(SimPauseGuard& pauseGuard, CircuitNode node) {
	for (auto it = interCircuitConnections.begin(); it != interCircuitConnections.end();) {
		if (it->circuitNodeDependencies.find(node) != it->circuitNodeDependencies.end()) {
			evalSimulator->removeConnection(pauseGuard, it->connection);
			it = interCircuitConnections.erase(it);
		} else {
			++it;
		}
	}
}

void Evaluator::removeCircuitIO(const DataUpdateEventManager::EventData* data) {
	const DataUpdateEventManager::EventDataWithValue<RemoveCircuitIOData>* eventData =
		dynamic_cast<const DataUpdateEventManager::EventDataWithValue<RemoveCircuitIOData>*>(data);
	if (!eventData) {
		logError("Invalid event data type", "Evaluator::removeCircuitIO");
		return;
	}
	std::tuple<BlockType, connection_end_id_t, Position> dataValue = eventData->get();
	BlockType blockType = std::get<0>(dataValue);
	connection_end_id_t connectionEndId = std::get<1>(dataValue);
	Position position = std::get<2>(dataValue);

	circuit_id_t circuitId = circuitBlockDataManager.getCircuitId(blockType);
	SimPauseGuard pauseGuard = evalSimulator->beginEdit();
	removeDependentInterCircuitConnections(pauseGuard, { circuitId, connectionEndId });
	evalSimulator->endEdit(pauseGuard);
	processDirtyNodes();
}

void Evaluator::setCircuitIO(const DataUpdateEventManager::EventData* data) {
	const DataUpdateEventManager::EventDataWithValue<SetCircuitIOData>* eventData =
		dynamic_cast<const DataUpdateEventManager::EventDataWithValue<SetCircuitIOData>*>(data);
	if (!eventData) {
		logError("Invalid event data type", "Evaluator::setCircuitIO");
		return;
	}
	std::pair<BlockType, connection_end_id_t> dataValue = eventData->get();
	BlockType blockType = dataValue.first;
	connection_end_id_t connectionEndId = dataValue.second;

	circuit_id_t circuitId = circuitBlockDataManager.getCircuitId(blockType);
	if (circuitId == 0) {
		logError("Circuit ID for BlockType {} is 0, cannot set IO", "Evaluator::setCircuitIO", blockType);
		return;
	}
	SimPauseGuard pauseGuard = evalSimulator->beginEdit();
	removeDependentInterCircuitConnections(pauseGuard, { circuitId, connectionEndId });
	// get the new position
	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(circuitId);
	if (!circuitBlockData) {
		logError("CircuitBlockData for Circuit ID {} not found", "Evaluator::setCircuitIO", circuitId);
		return;
	}
	const Position* position = circuitBlockData->getConnectionIdToPosition(connectionEndId);
	if (!position) {
		logError("Position for connection end ID {} not found in CircuitBlockData for Circuit ID {}", "Evaluator::setCircuitIO", connectionEndId, circuitId);
		return;
	}
	// use checkToCreateExternalConnections
	// iterate over eval_circuit_id_t
	for (eval_circuit_id_t evalCircuitId : evalCircuitContainer.ids()) {
		EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
		if (!evalCircuit) {
			continue;
		}
		if (evalCircuit->getCircuitId() != circuitId) {
			continue;
		}
		checkToCreateExternalConnections(pauseGuard, evalCircuitId, *position);
	}
	evalSimulator->endEdit(pauseGuard);
	processDirtyNodes();
}

std::optional<connection_end_id_t> Evaluator::getPortId(
	const circuit_id_t circuitId,
	const Position blockPosition,
	const Position portPosition,
	Direction direction
) const {
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) [[unlikely]] {
		return std::nullopt;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	return getPortId(blockContainer, blockPosition, portPosition, direction);
}

std::optional<connection_end_id_t> Evaluator::getPortId(
	const BlockContainer& blockContainer,
	const Position blockPosition,
	const Position portPosition,
	Direction direction
) const {
	const Block* block = blockContainer.getBlock(blockPosition);
	if (!block) [[unlikely]] {
		return std::nullopt;
	}

	// Directly return the result based on direction
	if (direction == Direction::IN) {
		return block->getInputOrBidirectionalConnectionId(portPosition);
	} else {
		return block->getOutputOrBidirectionalConnectionId(portPosition);
	}
}

std::optional<EvalConnectionPoint> Evaluator::getConnectionPoint(
	const eval_circuit_id_t evalCircuitId,
	const Position portPosition,
	Direction direction
) const {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) [[unlikely]] {
		return std::nullopt;
	}

	circuit_id_t circuitId = evalCircuit->getCircuitId();
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) [[unlikely]] {
		return std::nullopt;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	// Get the block first - this is the most likely failure point
	const Block* block = blockContainer.getBlock(portPosition);
	if (!block) [[unlikely]] {
		return std::nullopt;
	}

	const BlockType blockType = block->type();
	const Position blockPosition = block->getPosition();

	std::optional<CircuitNode> node = evalCircuit->getNode(blockPosition);
	if (!node.has_value()) [[unlikely]] {
		return std::nullopt;
	}

	switch (blockType) {
	case BlockType::SWITCH:
	case BlockType::BUTTON:
	case BlockType::TICK_BUTTON:
		if (direction == Direction::IN) [[likely]] {
			return EvalConnectionPoint(node->getMiddleId(), connection_end_id_t(0));
		}
		break;
	case BlockType::LIGHT:
		if (direction == Direction::OUT) [[likely]] {
			return EvalConnectionPoint(node->getMiddleId(), connection_end_id_t(0));
		}
		break;
	default: break;
	}

	std::optional<connection_end_id_t> portId;
	if (direction == Direction::IN) {
		const std::optional<connection_end_id_t> port = block->getInputOrBidirectionalConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	} else {
		const std::optional<connection_end_id_t> port = block->getOutputOrBidirectionalConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	}

	if (!portId.has_value()) [[unlikely]] {
		return std::nullopt;
	}

	if (!node->isIC()) [[likely]] {
		return EvalConnectionPoint(node->getMiddleId(), portId.value());
	}

	const circuit_id_t innerCircuitId = circuitBlockDataManager.getCircuitId(blockType);
	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(innerCircuitId);
	if (!circuitBlockData) [[unlikely]] {
		logError("CircuitBlockData for type {} not found", "Evaluator::getConnectionPoint", blockType);
		return std::nullopt;
	}

	const Position* internalPosition = circuitBlockData->getConnectionIdToPosition(portId.value());
	if (!internalPosition) [[unlikely]] {
		logError("Internal position for port ID {} not found in CircuitBlockData for type {}", "Evaluator::getConnectionPoint", portId.value(), blockType);
		return std::nullopt;
	}

	return getConnectionPoint(node->getEvalCircuitId(), *internalPosition, direction);
}

std::optional<EvalConnectionPoint> Evaluator::getConnectionPoint(
	const eval_circuit_id_t evalCircuitId,
	const Position portPosition,
	Direction direction,
	std::set<CircuitPortDependency>& circuitPortDependencies,
	std::set<CircuitNode>& circuitNodeDependencies,
	std::set<EvalPosition>& visitedEvalPositions,
	bool isInterCircuit
) const {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) [[unlikely]] {
		return std::nullopt;
	}

	SharedCircuit circuit = circuitManager.getCircuit(evalCircuit->getCircuitId());
	if (!circuit) [[unlikely]] {
		return std::nullopt;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	const Block* block = blockContainer.getBlock(portPosition);
	if (!block) [[unlikely]] {
		return std::nullopt;
	}

	visitedEvalPositions.insert({ portPosition, evalCircuitId });

	const BlockType blockType = block->type();
	const Position blockPosition = block->getPosition();

	std::optional<CircuitNode> node = evalCircuit->getNode(blockPosition);
	if (!node.has_value()) [[unlikely]] {
		return std::nullopt;
	}
	if (isInterCircuit) {
		circuitNodeDependencies.insert(node.value());
	}
	switch (blockType) {
	case BlockType::SWITCH:
	case BlockType::BUTTON:
	case BlockType::TICK_BUTTON:
		if (direction == Direction::IN) [[likely]] {
			return EvalConnectionPoint(node->getMiddleId(), connection_end_id_t(0));
		}
		break;
	case BlockType::LIGHT:
		if (direction == Direction::OUT) [[likely]] {
			return EvalConnectionPoint(node->getMiddleId(), connection_end_id_t(0));
		}
		break;
	default: break;
	}

	std::optional<connection_end_id_t> portId;
	if (direction == Direction::IN) {
		const std::optional<connection_end_id_t> port = block->getInputOrBidirectionalConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	} else {
		const std::optional<connection_end_id_t> port = block->getOutputOrBidirectionalConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	}

	if (!portId.has_value()) [[unlikely]] {
		return std::nullopt;
	}

	if (!node->isIC()) [[likely]] {
		return EvalConnectionPoint(node->getMiddleId(), portId.value());
	}

	const circuit_id_t circuitId = circuitBlockDataManager.getCircuitId(blockType);
	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(circuitId);
	if (!circuitBlockData) [[unlikely]] {
		logError("CircuitBlockData for type {} not found", "Evaluator::getConnectionPoint", blockType);
		return std::nullopt;
	}

	const Position* internalPosition = circuitBlockData->getConnectionIdToPosition(portId.value());
	if (!internalPosition) [[unlikely]] {
		logError("Internal position for port ID {} not found in CircuitBlockData for type {}", "Evaluator::getConnectionPoint", portId.value(), blockType);
		return std::nullopt;
	}

	circuitPortDependencies.insert({ circuitId, portId.value() });
	return getConnectionPoint(node->getEvalCircuitId(), *internalPosition, direction, circuitPortDependencies, circuitNodeDependencies, visitedEvalPositions, true);
}

void Evaluator::edit_moveBlock(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	DiffCache& diffCache,
	Position curPosition,
	Orientation curOrientation,
	Position newPosition,
	Orientation newOrientation,
	MoveType finalMove
) {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_moveBlock", evalCircuitId);
		return;
	}
	// delete interCircuit connections that are dependent on the block being moved
	std::optional<CircuitNode> node = evalCircuit->getNode(curPosition);
	if (!node.has_value()) {
		logError("Node at position {} not found", "Evaluator::edit_moveBlock", curPosition.toString());
		return;
	}
	if (node->isIC()) {
		changedICs = true;
	}
	removeDependentInterCircuitConnections(pauseGuard, node.value());
	if (!node->isIC()){
		middle_id_t gateId = node->getMiddleId();
		middleIdToEvalPositionMap.erase(gateId);
		middleIdToEvalPositionMap[gateId] = { newPosition, evalCircuitId };
	}
	evalCircuit->moveNode(curPosition, newPosition);
	if (finalMove != MoveType::MULTI_BEGIN && finalMove != MoveType::MULTI_MIDDLE) {
		checkToCreateExternalConnections(pauseGuard, evalCircuitId, newPosition);
	}
	dirtyBlockAt(newPosition, evalCircuitId);
}

const EvalAddressTree Evaluator::buildAddressTree() const { return buildAddressTree(eval_circuit_id_t(0)); }

const EvalAddressTree Evaluator::buildAddressTree(eval_circuit_id_t evalCircuitId) const {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::buildAddressTree", evalCircuitId);
		return EvalAddressTree(0);
	}
	EvalAddressTree root = EvalAddressTree(evalCircuit->getCircuitId());
	evalCircuit->forEachNode([this, &root](Position pos, const CircuitNode& node) {
		if (node.isIC()) {
			root.addBranch(pos, buildAddressTree(node.getEvalCircuitId()));
		}
	});
	return root;
}

std::optional<middle_id_t> Evaluator::getMiddleId(const eval_circuit_id_t startingPoint, const Address& address) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(startingPoint, address);
	circuit_id_t circuitId = evalCircuitContainer.getCircuitId(evalCircuitId).value_or(0);
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) {
		logError("Circuit with ID {} not found", "Evaluator::getMiddleId", circuitId);
		return std::nullopt;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	Position blockPosition = address.getPosition(address.size() - 1);
	const Block* block = blockContainer.getBlock(blockPosition);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::getMiddleId", blockPosition.toString());
		return std::nullopt;
	}
	std::optional<CircuitNode> node = evalCircuitContainer.getNode(block->getPosition(), evalCircuitId);
	if (!node.has_value()) {
		logError("Node not found for address {}", "Evaluator::getMiddleId", address.toString());
		return std::nullopt;
	}
	if (node->isIC()) {
		return std::nullopt;
	}
	return node->getMiddleId();
}

std::optional<middle_id_t> Evaluator::getMiddleId(const eval_circuit_id_t startingPoint, const Address& address, const BlockContainer& blockContainer) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(startingPoint, address);
	Position blockPosition = address.getPosition(address.size() - 1);
	const Block* block = blockContainer.getBlock(blockPosition);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::getMiddleId", blockPosition.toString());
		return std::nullopt;
	}
	std::optional<CircuitNode> node = evalCircuitContainer.getNode(block->getPosition(), evalCircuitId);
	if (!node.has_value()) {
		logError("Node not found for address {}", "Evaluator::getMiddleId", address.toString());
		return std::nullopt;
	}
	if (node->isIC()) {
		return std::nullopt;
	}
	return node->getMiddleId();
}

std::optional<middle_id_t> Evaluator::getMiddleId(const Address& address) const { return getMiddleId(eval_circuit_id_t(0), address); }

logic_state_t Evaluator::getState(const Address& address) {
	std::shared_lock lk(simMutex);
	std::optional<eval_circuit_id_t> evalCircuitIdOpt = evalCircuitContainer.traverseToTopLevelIC(address);
	if (!evalCircuitIdOpt.has_value()) {
		logError("Failed to traverse to top-level IC for address {}", "Evaluator::getState", address.toString());
		return logic_state_t::UNDEFINED;
	}

	eval_circuit_id_t evalCircuitId = evalCircuitIdOpt.value();
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::getState", evalCircuitId);
		return logic_state_t::UNDEFINED;
	}

	circuit_id_t circuitId = evalCircuit->getCircuitId();
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) {
		logError("Circuit with ID {} not found", "Evaluator::getState", circuitId);
		return logic_state_t::UNDEFINED;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();

	std::optional<EvalConnectionPoint> connectionPointOpt = getConnectionPoint(evalCircuitId, address.getPosition(address.size() - 1), Direction::OUT);
	if (!connectionPointOpt.has_value()) {
		logError("Connection point not found for address {}", "Evaluator::getState", address.toString());
		return logic_state_t::UNDEFINED;
	}
	return evalSimulator->getState(connectionPointOpt.value());
}

void Evaluator::setState(const Address& address, logic_state_t state) {
	std::unique_lock lk(simMutex);
	std::optional<eval_circuit_id_t> evalCircuitIdOpt = evalCircuitContainer.traverseToTopLevelIC(address);
	if (!evalCircuitIdOpt.has_value()) {
		logError("Failed to traverse to top-level IC for address {}", "Evaluator::setState", address.toString());
		return;
	}

	eval_circuit_id_t evalCircuitId = evalCircuitIdOpt.value();
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::setState", evalCircuitId);
		return;
	}

	circuit_id_t circuitId = evalCircuit->getCircuitId();
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) {
		logError("Circuit with ID {} not found", "Evaluator::setState", circuitId);
		return;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();

	std::optional<EvalConnectionPoint> connectionPointOpt = getConnectionPoint(evalCircuitId, address.getPosition(address.size() - 1), Direction::OUT);
	if (connectionPointOpt.has_value()) {
		evalSimulator->setState(connectionPointOpt.value(), state);
		return;
	}
	std::optional<EvalConnectionPoint> connectionPointOptIn = getConnectionPoint(evalCircuitId, address.getPosition(address.size() - 1), Direction::IN);
	if (connectionPointOptIn.has_value()) {
		evalSimulator->setState(connectionPointOptIn.value(), state);
		return;
	}
	std::optional<middle_id_t> middleIdOpt = getMiddleId(evalCircuitId, address, blockContainer);
	if (middleIdOpt.has_value()) {
		EvalConnectionPoint connectionPoint(middleIdOpt.value(), connection_end_id_t(0));
		evalSimulator->setState(connectionPoint, state);
		return;
	}
	logError("Failed to get connection point for address {}", "Evaluator::setState", address.toString());
}

void Evaluator::checkToCreateExternalConnections(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, Position position) {
	// logInfo("Checking to create external connections for evalCircuitId {} at position {}", "Evaluator::checkToCreateExternalConnections", evalCircuitId,
	// position.toString());
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::checkToCreateExternalConnections", evalCircuitId);
		return;
	}
	if (evalCircuit->isRoot()) {
		// logInfo("EvalCircuit is root, no external connections to create", "Evaluator::checkToCreateExternalConnections");
		return;
	}
	std::optional<CircuitNode> node = evalCircuit->getNode(position);
	if (!node.has_value()) {
		logError("Node at position {} not found", "Evaluator::checkToCreateExternalConnections", position.toString());
		return;
	}
	// logInfo("Node found: {}", "Evaluator::checkToCreateExternalConnections", node->toString());
	SharedCircuit circuit = circuitManager.getCircuit(evalCircuit->getCircuitId());
	if (!circuit) {
		logError("Circuit with id {} not found", "Evaluator::makeEditInPlace", evalCircuit->getCircuitId());
		return;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	const Block* block = blockContainer.getBlock(position);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::checkToCreateExternalConnections", position.toString());
		return;
	}

	// Get block data to iterate through all connections
	const BlockData* blockData = blockDataManager.getBlockData(block->type());
	if (!blockData) {
		logError("BlockData not found for block type {}", "Evaluator::checkToCreateExternalConnections", static_cast<int>(block->type()));
		return;
	}

	// Iterate through all connections of the block
	struct ConnectionData {
		Position portPosition;
		Direction direction;
	};
	std::vector<ConnectionData> connectionDataList = {};
	if (blockData->isDefaultData()) {
		// logInfo("Block type {} is default data", "Evaluator::checkToCreateExternalConnections", static_cast<int>(block->type()));
		connectionDataList.push_back({ position, Direction::IN });
		connectionDataList.push_back({ position, Direction::OUT });
	} else {
		const auto& connections = blockData->getConnections();
		// logInfo("Found {} connections for block type {}", "Evaluator::checkToCreateExternalConnections", connections.size(), static_cast<int>(block->type()));
		for (const auto& [connectionId, connectionOffset] : connections) {
			// Determine direction (input or output or both)
			bool connectionIsInput = block->isConnectionInputOrBidirectional(connectionId);
			bool connectionIsOutput = block->isConnectionOutputOrBidirectional(connectionId);
			if (!connectionIsInput && !connectionIsOutput) {
				continue;
			}
			std::optional<Position> portPosition = block->getConnectionPosition(connectionId);
			if (!portPosition.has_value()) {
				logError(
					"Port position not found for connection ID {} in block type {}",
					"Evaluator::checkToCreateExternalConnections",
					connectionId,
					static_cast<int>(block->type())
				);
				continue;
			}
			if (connectionIsInput) {
				connectionDataList.push_back({ portPosition.value(), Direction::IN });
			}
			if (connectionIsOutput) {
				connectionDataList.push_back({ portPosition.value(), Direction::OUT });
			}
		}
		if (block->type() == BlockType::SWITCH || block->type() == BlockType::BUTTON || block->type() == BlockType::TICK_BUTTON) {
			connectionDataList.push_back({ position, Direction::IN });
		}
		if (block->type() == BlockType::LIGHT) {
			connectionDataList.push_back({ position, Direction::OUT });
		}
	}
	for (const auto& connectionData : connectionDataList) {
		Position portPosition = connectionData.portPosition;
		Direction direction = connectionData.direction;

		// logInfo("Checking connection at position {} with direction {}", "Evaluator::checkToCreateExternalConnections", portPosition.toString(), (direction ==
		// Direction::IN ? "IN" : "OUT"));

		std::set<CircuitPortDependency> circuitPortDependencies;
		std::set<CircuitNode> circuitNodeDependencies;
		std::set<EvalPosition> visitedEvalPositions;
		circuitNodeDependencies.insert(node.value());
		std::optional<EvalConnectionPoint> connectionPoint =
			getConnectionPoint(evalCircuitId, portPosition, direction, circuitPortDependencies, circuitNodeDependencies, visitedEvalPositions, false);

		if (connectionPoint.has_value()) {
			traceOutwardsIC(pauseGuard, evalCircuitId, portPosition, direction, connectionPoint.value(), circuitPortDependencies, circuitNodeDependencies, visitedEvalPositions);
		} else {
			logError("Connection point not found for position {}", "Evaluator::checkToCreateExternalConnections", portPosition.toString());
		}
		for (const EvalPosition& evalPosition : visitedEvalPositions) {
			dirtyNodes.insert(evalPosition);
		}
	}
}

void Evaluator::traceOutwardsIC(
	SimPauseGuard& pauseGuard,
	eval_circuit_id_t evalCircuitId,
	Position position,
	Direction direction,
	const EvalConnectionPoint& targetConnectionPoint,
	std::set<CircuitPortDependency>& circuitPortDependencies,
	std::set<CircuitNode>& circuitNodeDependencies,
	std::set<EvalPosition>& visitedEvalPositions
	) {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::traceOutwardsIC", evalCircuitId);
		return;
	}
	if (evalCircuit->isRoot()) {
		return;
	}
	circuit_id_t circuitId = evalCircuit->getCircuitId();
	CircuitBlockData* circuitBlockData = circuitBlockDataManager.getCircuitBlockData(circuitId);
	if (!circuitBlockData) {
		logError("CircuitBlockData for circuit ID {} not found", "Evaluator::traceOutwardsIC", circuitId);
		return;
	}
	SharedCircuit innerCircuit = circuitManager.getCircuit(evalCircuit->getCircuitId());
	if (!innerCircuit) {
		logError("Inner circuit for evalCircuitId {} not found", "Evaluator::traceOutwardsIC", evalCircuitId);
		return;
	}
	const BlockContainer& innerBlockContainer = innerCircuit->getBlockContainer();
	const Block* block = innerBlockContainer.getBlock(position);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::traceOutwardsIC", position.toString());
		return;
	}
	std::optional<CircuitNode> node = evalCircuit->getNode(block->getPosition());
	if (!node.has_value()) {
		logError("CircuitNode not found for block at position {}", "Evaluator::traceOutwardsIC", block->getPosition().toString());
		return;
	}
	// go through all the connection_end_ids
	eval_circuit_id_t parentEvalCircuitId = evalCircuit->getParentEvalId();
	EvalCircuit* parentEvalCircuit = evalCircuitContainer.getCircuit(parentEvalCircuitId);
	if (!parentEvalCircuit) {
		logError("Parent EvalCircuit with id {} not found", "Evaluator::traceOutwardsIC", parentEvalCircuitId);
		return;
	}

	SharedCircuit parentCircuit = circuitManager.getCircuit(parentEvalCircuit->getCircuitId());
	if (!parentCircuit) {
		logError("Circuit with id {} not found", "Evaluator::traceOutwardsIC", parentEvalCircuit->getCircuitId());
		return;
	}
	const BlockContainer& parentCircuitBlockContainer = parentCircuit->getBlockContainer();
	std::optional<Position> parentCircuitPositionOpt = parentEvalCircuit->getPosition(CircuitNode::fromIC(evalCircuitId));
	if (!parentCircuitPositionOpt.has_value()) {
		logError("Parent circuit position not found for evalCircuitId {}", "Evaluator::traceOutwardsIC", evalCircuitId);
		return;
	}
	const Block* parentCircuitBlock = parentCircuitBlockContainer.getBlock(parentCircuitPositionOpt.value());
	if (!parentCircuitBlock) {
		logError("Block not found at position {}", "Evaluator::traceOutwardsIC", parentCircuitPositionOpt.value().toString());
		return;
	}

	circuitNodeDependencies.insert(node.value());

	BidirectionalMultiSecondKeyMap<connection_end_id_t, Position>::constIteratorPairT2 connectionIds = circuitBlockData->getConnectionPositionToId(position);

	for (auto iter = connectionIds.first; iter != connectionIds.second; ++iter) {
		connection_end_id_t connectionEndId = iter->second;
		// if (parentCircuitBlock->isConnectionInputOrBidirectional(connectionEndId) != (direction == Direction::IN)) {
		// 	continue;
		// }
		if (direction == Direction::IN && !parentCircuitBlock->isConnectionInput(connectionEndId)) {
			continue;
		}
		if (direction == Direction::OUT && !parentCircuitBlock->isConnectionOutput(connectionEndId)) {
			continue;
		}
		std::optional<Position> connectionPosOpt = parentCircuitBlock->getConnectionPosition(connectionEndId);
		if (!connectionPosOpt) {
			logError("Connection position not found at connectionEndId {}", "Evaluator::traceOutwardsIC", connectionEndId);
			continue;
		}
		Position connectionPos = connectionPosOpt.value();
		const std::unordered_set<ConnectionEnd>* connectionEnds1 = parentCircuitBlock->getBidirectionalConnections(connectionPos);
		const std::unordered_set<ConnectionEnd>* connectionEnds2 = (direction == Direction::IN) ? parentCircuitBlock->getInputConnections(connectionPos) : parentCircuitBlock->getOutputConnections(connectionPos);
		if (!connectionEnds1 && !connectionEnds2) {
			// logError("Connection ends not found at position {}", "Evaluator::traceOutwardsIC", connectionPos.toString());
			continue;
		}

		// merge the two sets
		std::unordered_set<ConnectionEnd> connectionEnds = {};
		if (connectionEnds1 && connectionEnds2) {
			connectionEnds.insert(connectionEnds1->begin(), connectionEnds1->end());
			connectionEnds.insert(connectionEnds2->begin(), connectionEnds2->end());
		} else if (connectionEnds1) {
			connectionEnds = *connectionEnds1;
		} else {
			connectionEnds = *connectionEnds2;
		}

		circuitPortDependencies.insert({ circuitId, connectionEndId });

		for (const ConnectionEnd& connectionEnd : connectionEnds) {
			const Block* targetBlock = parentCircuitBlockContainer.getBlock(connectionEnd.getBlockId());
			if (!targetBlock) {
				logError("Target block not found for connectionEnd {}", "Evaluator::traceOutwardsIC", connectionEnd.toString());
				continue;
			}
			std::optional<Position> targetConnectionPointPositionOpt = targetBlock->getConnectionPosition(connectionEnd.getConnectionId());
			if (!targetConnectionPointPositionOpt) {
				logError("Target connection position not found for connectionEnd {}", "Evaluator::traceOutwardsIC", connectionEnd.toString());
				continue;
			}
			Position targetConnectionPointPosition = targetConnectionPointPositionOpt.value();
			std::set<CircuitPortDependency> circuitPortDependenciesCopy = circuitPortDependencies;
			std::set<CircuitNode> circuitNodeDependenciesCopy = circuitNodeDependencies;
			// get connection point
			std::optional<EvalConnectionPoint> connectionPoint =
				getConnectionPoint(parentEvalCircuitId, targetConnectionPointPosition, !direction, circuitPortDependenciesCopy, circuitNodeDependenciesCopy, visitedEvalPositions, false);
			if (!connectionPoint.has_value()) {
				continue;
			}
			if (connectionPoint->gateId == targetConnectionPoint.gateId && direction == Direction::OUT) {
				// this is a safety against a gate wiring to itself twice if it connects to itself in a round-about way
				continue;
			}
			// If we reached here, we found a valid connection point
			// EvalConnection evalConnection(connectionPoint.value(), targetConnectionPoint);
			EvalConnection evalConnection;
			if (direction == Direction::IN) {
				evalConnection = EvalConnection(connectionPoint.value(), targetConnectionPoint);
			} else {
				evalConnection = EvalConnection(targetConnectionPoint, connectionPoint.value());
			}
			// if (checkIfBitWidthsMatch(evalConnection)) {
			evalSimulator->makeConnection(pauseGuard, evalConnection);
			interCircuitConnections.push_back({ evalConnection, circuitPortDependenciesCopy, circuitNodeDependenciesCopy });
			// }

			traceOutwardsIC(
				pauseGuard,
				parentEvalCircuitId,
				targetConnectionPointPosition,
				direction,
				targetConnectionPoint,
				circuitPortDependencies,
				circuitNodeDependencies,
				visitedEvalPositions
			);
		}

		circuitPortDependencies.erase({ circuitId, connectionEndId });
	}

	circuitNodeDependencies.erase(node.value());
}

void Evaluator::processDirtyNodes() {
	for (const simulator_id_t id : dirtySimulatorIds) {
		auto it = pinSimulatorIdToEvalPositionMap.equal_range(id);
		for (auto iter = it.first; iter != it.second; ++iter) {
			const auto& [simulatorId, evalPosition] = *iter;
			dirtyNodes.insert(evalPosition);
		}
		pinSimulatorIdToEvalPositionMap.erase(id);
	}
	dirtySimulatorIds.clear();

	for (const middle_id_t id : dirtyMiddleIds) {
		auto it = middleIdToEvalPositionMap.find(id);
		if (it != middleIdToEvalPositionMap.end()) {
			const EvalPosition& evalPosition = it->second;
			dirtyBlockAt(evalPosition.position, evalPosition.evalCircuitId);
		}
	}
	dirtyMiddleIds.clear();

	std::vector<EvalPosition> dirtyPointsToProcess;
	std::vector<EvalConnectionPoint> pointToGatherPinSimIds;
	std::vector<EvalConnectionPoint> blocksToGatherBlockSimIds;
	std::unordered_set<middle_id_t> blocksToGatherBlockSimIdsSet;

	for (const EvalPosition& evalPosition : dirtyNodes) {
		Position position = evalPosition.position;
		EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalPosition.evalCircuitId);
		if (!evalCircuit) [[unlikely]] {
			continue;
		}
		const SharedCircuit circuit = circuitManager.getCircuit(evalCircuit->getCircuitId());
		const Block* block = circuit->getBlockContainer().getBlock(position);
		if (!block) [[unlikely]] {
			continue;
		}
		BlockType blockType = block->type();
		if (blockType == BlockType::NONE) {
			continue;
		}
		const BlockData* blockData = blockDataManager.getBlockData(blockType);
		bool shouldSendBlockUpdate = blockData->hasBlockState();
		if (shouldSendBlockUpdate) {
			middle_id_t middleId = evalCircuit->getNode(block->getPosition())->getMiddleId();
			if (!blocksToGatherBlockSimIdsSet.contains(middleId)) {
				blocksToGatherBlockSimIdsSet.insert(middleId);
				blocksToGatherBlockSimIds.push_back({middleId, connection_end_id_t(0)});
			}
		}
		std::optional<EvalConnectionPoint> connectionPoint = getConnectionPoint(evalPosition.evalCircuitId, evalPosition.position, Direction::OUT);
		if (!connectionPoint.has_value()) {
			continue;
		}
		dirtyPointsToProcess.push_back(evalPosition);
		pointToGatherPinSimIds.push_back(connectionPoint.value());
	}
	dirtyNodes.clear();

	std::vector<simulator_id_t> blockSimIds = evalSimulator->getBlockSimulatorIds(to_optional_vector(blocksToGatherBlockSimIds));
	std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> pinSimIds = evalSimulator->getPinSimulatorIds(to_optional_vector(pointToGatherPinSimIds));

	std::unordered_map<eval_circuit_id_t, std::vector<SimulatorMappingUpdate>> simulatorMappingUpdates;

	for (int i = 0; i < dirtyPointsToProcess.size(); ++i) {
		const EvalPosition& evalPosition = dirtyPointsToProcess.at(i);
		std::variant<simulator_id_t, std::vector<simulator_id_t>> pinSimId = pinSimIds.at(i);
		simulatorMappingUpdates[evalPosition.evalCircuitId].push_back({ evalPosition.position, pinSimId, SimulatorMappingUpdateType::PIN });
		if (std::holds_alternative<simulator_id_t>(pinSimId)) {
			simulator_id_t singlePinSimId = std::get<simulator_id_t>(pinSimId);
			pinSimulatorIdToEvalPositionMap.insert({ singlePinSimId, evalPosition });
		} else {
			const std::vector<simulator_id_t>& multiplePinSimIds = std::get<std::vector<simulator_id_t>>(pinSimId);
			for (simulator_id_t multiplePinSimId : multiplePinSimIds) {
				pinSimulatorIdToEvalPositionMap.insert({ multiplePinSimId, evalPosition });
			}
		}
		// logInfo(
		// 	"Processed dirty point at evalCircuitId {}, position {}, simulatorId {}",
		// 	"Evaluator::processDirtyNodes",
		// 	evalPosition.evalCircuitId,
		// 	evalPosition.position.toString(),
		// 	std::to_string(pinSimId)
		// 	);
	}
	for (int i = 0; i < blocksToGatherBlockSimIds.size(); ++i) {
		const EvalConnectionPoint& blockPoint = blocksToGatherBlockSimIds.at(i);
		simulator_id_t blockSimId = blockSimIds.at(i);
		EvalPosition evalPosition = middleIdToEvalPositionMap.at(blockPoint.gateId);
		simulatorMappingUpdates[evalPosition.evalCircuitId].push_back({ evalPosition.position, blockSimId, SimulatorMappingUpdateType::BLOCK });
		// logInfo(
		// 	"Processed dirty block at evalCircuitId {}, position {}, simulatorId {}",
		// 	"Evaluator::processDirtyNodes",
		// 	evalPosition.evalCircuitId,
		// 	evalPosition.position.toString(),
		// 	std::to_string(blockSimId)
		// );
	}

	for (const auto& [evalCircuitId, updates] : simulatorMappingUpdates) {
		sendSimulatorMappingUpdate(evalCircuitId, updates);
	}
}

void Evaluator::dirtyBlockAt(Position position, eval_circuit_id_t evalCircuitId) {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::dirtyBlockAt", evalCircuitId);
		return;
	}
	circuit_id_t circuitId = evalCircuit->getCircuitId();
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) {
		logError("Circuit with id {} not found", "Evaluator::dirtyBlockAt", circuitId);
		return;
	}
	const BlockContainer& blockContainer = circuit->getBlockContainer();
	const Block* block = blockContainer.getBlock(position);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::dirtyBlockAt", position.toString());
		return;
	}
	if (block->type() == BlockType::LIGHT) {
		dirtyNodes.insert({ position, evalCircuitId });
		return;
	}
	const BlockData* blockData = blockDataManager.getBlockData(block->type());
	if (!blockData) {
		logError("BlockData not found for block type {}", "Evaluator::dirtyBlockAt", static_cast<int>(block->type()));
		return;
	}
	if (blockData->isDefaultData()) {
		dirtyNodes.insert({ position, evalCircuitId });
		return;
	}
	const std::unordered_map<connection_end_id_t, BlockData::ConnectionData>& connections = blockData->getConnections();
	for (const auto& [connectionId, connectionData] : connections) {
		std::optional<Position> portPositionOpt = block->getConnectionPosition(connectionId);
		if (!portPositionOpt) {
			logError("Port position not found for connection ID {}", "Evaluator::dirtyBlockAt", connectionId);
			continue;
		}
		dirtyNodes.insert({ portPositionOpt.value(), evalCircuitId });
	}
}

std::vector<simulator_id_t> Evaluator::getBlockSimulatorIds(const Address& addressOrigin, const std::vector<Position>& positions) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(addressOrigin);
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	std::vector<std::optional<EvalConnectionPoint>> connectionPoints;
	connectionPoints.reserve(positions.size());
	for (const Position& portPosition : positions) {
		std::optional<CircuitNode> node = evalCircuit->getNode(portPosition);
		if (!node.has_value() || node->isIC()) {
			connectionPoints.push_back(std::nullopt);
			continue;
		}
		connectionPoints.push_back(EvalConnectionPoint(node->getMiddleId(), connection_end_id_t(0)));
	}
	return evalSimulator->getBlockSimulatorIds(connectionPoints);
}

std::vector<std::variant<simulator_id_t, std::vector<simulator_id_t>>> Evaluator::getPinSimulatorIds(
	const Address& addressOrigin,
	const std::vector<Position>& positions
) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(addressOrigin);
	std::vector<std::optional<EvalConnectionPoint>> connectionPoints;
	connectionPoints.reserve(positions.size());
	for (const Position& portPosition : positions) {
		connectionPoints.push_back(getConnectionPoint(evalCircuitId, portPosition, Direction::OUT));
	}
	return evalSimulator->getPinSimulatorIds(connectionPoints);
}

std::vector<logic_state_t> Evaluator::getStatesFromSimulatorIds(const std::vector<simulator_id_t>& simulatorIds) const {
	return evalSimulator->getStatesFromSimulatorIds(simulatorIds);
}

void Evaluator::connectListener(void* object, const Address& address, SimulatorMappingUpdateListenerFunction func) {
	std::optional<eval_circuit_id_t> evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(address);
	if (!evalCircuitId) {
		logError("Failed to connect listener for address {}: No top-level IC found", "Evaluator::connectListener", address.toString());
		return;
	}
	listeners[object] = { evalCircuitId.value(), func };
	std::unordered_map<eval_circuit_id_t, std::vector<SimulatorMappingUpdate>> simulatorMappingUpdates;
	auto evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId.value());
	if (!evalCircuit) {
		logError("Failed to get eval circuit for ID {}", "Evaluator::connectListener", evalCircuitId.value());
		return;
	}
	evalCircuit->forEachNode([this, evalCircuitId](Position pos, const CircuitNode& node) { this->dirtyBlockAt(pos, evalCircuitId.value()); });
	processDirtyNodes();
}

void Evaluator::waitForSprintComplete() {
	while (evalConfig.getSprintCount() > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

bool Evaluator::stepBack() {
	setPause(true);
	return evalSimulator->stepBack();
}

void Evaluator::stepForward() {
	setPause(true);
	bool success = evalSimulator->stepForward();
	if (!success) {
		tickStep();
	}
}

bool Evaluator::skipBack() {
	setPause(true);
	return evalSimulator->skipBack();
}

bool Evaluator::skipForward() {
	setPause(true);
	return evalSimulator->skipForward();
}

bool Evaluator::isViewingReplay() const {
	return evalSimulator->isViewingReplay();
}

double Evaluator::getRealTickrate() const { return evalSimulator->getAverageTickrate(); }

namespace {

// Tunable tolerance for floating-point comparisons
inline constexpr double EPS = 1e-12;

// Decompose x > 0 as x = m * 10^k with m in [1, 10)
inline void decompose(double x, double& m, int& k) {
    // Start with a log10-based estimate for speed
    double lg = std::log10(x);
    // Add a tiny epsilon to stabilize near powers of 10
    k = static_cast<int>(std::floor(lg + EPS));
    double p = std::pow(10.0, k);
    m = x / p;

    // Correct any edge drift so m in [1, 10)
    while (m < 1.0) { m *= 10.0; --k; }
    while (m >= 10.0) { m /= 10.0; ++k; }
}

// True if m is (within EPS) an integer in {1,2,...,9}
inline bool is_seq_mantissa(double m) {
    double r = std::round(m);
    return std::abs(m - r) <= EPS && r >= 1.0 && r <= 9.0;
}

// Smallest sequence value strictly greater than x
inline double next_in_sequence(double x) {
    if (!(x > 0.0)) return std::numeric_limits<double>::quiet_NaN();

    double m; int k;
    decompose(x, m, k);

    if (is_seq_mantissa(m)) {
        int mi = static_cast<int>(std::llround(m));
        if (mi < 9) {
            return (mi + 1) * std::pow(10.0, k);
        } else {
            // mi == 9 -> roll to 1 * 10^(k+1)
            return std::pow(10.0, k + 1);
        }
    } else {
        // Ceil within the decade
        int cm = static_cast<int>(std::ceil(m - EPS)); // bias to avoid ceil(1.000...+tiny)=2
        if (cm <= 9) {
            return cm * std::pow(10.0, k);
        } else {
            // Hit 10 -> roll to next decade
            return std::pow(10.0, k + 1);
        }
    }
}

// Largest sequence value strictly less than x
inline double prev_in_sequence(double x) {
    if (!(x > 0.0)) return std::numeric_limits<double>::quiet_NaN();

    double m; int k;
    decompose(x, m, k);

    if (is_seq_mantissa(m)) {
        int mi = static_cast<int>(std::llround(m));
        if (mi > 1) {
            return (mi - 1) * std::pow(10.0, k);
        } else {
            // mi == 1 -> roll back to 9 * 10^(k-1)
            return 9.0 * std::pow(10.0, k - 1);
        }
    } else {
        // Floor within the decade
        int fm = static_cast<int>(std::floor(m + EPS)); // bias to avoid floor(0.999...-tiny)=0
        if (fm >= 1) {
            return fm * std::pow(10.0, k);
        } else {
            // Fell below 1 -> use previous decade's 9
            return 9.0 * std::pow(10.0, k - 1);
        }
    }
}

}

void Evaluator::increaseTickrateSeq() {
	double currentTickrate = getTickrate();
	double newTickrate = next_in_sequence(currentTickrate);
	setTickrate(newTickrate);
}
void Evaluator::decreaseTickrateSeq() {
	double currentTickrate = getTickrate();
	double newTickrate = std::max(0.1, prev_in_sequence(currentTickrate));
	setTickrate(newTickrate);
}

nlohmann::json Evaluator::dumpState() const {
	nlohmann::json stateJson;
	stateJson["evaluatorId"] = evaluatorId.get();
	stateJson["evalCircuitContainer"] = evalCircuitContainer.dumpState();
	stateJson["evalConfig"] = evalConfig.dumpState();
	stateJson["middleIdProvider"] = middleIdProvider.dumpState();
	stateJson["evalSimulator"] = evalSimulator->dumpState();

	stateJson["interCircuitConnections"] = nlohmann::json::array();
	for (const InterCircuitConnection& connection : interCircuitConnections) {
		stateJson["interCircuitConnections"].push_back(connection.dumpState());
	}

	stateJson["dirtySimulatorIds"] = nlohmann::json::array();
	for (const simulator_id_t simId : dirtySimulatorIds) {
		stateJson["dirtySimulatorIds"].push_back(simId.get());
	}

	stateJson["dirtyMiddleIds"] = nlohmann::json::array();
	for (const middle_id_t mid : dirtyMiddleIds) {
		stateJson["dirtyMiddleIds"].push_back(mid.get());
	}

	stateJson["dirtyNodes"] = nlohmann::json::array();
	for (const EvalPosition& evalPosition : dirtyNodes) {
		stateJson["dirtyNodes"].push_back(evalPosition.dumpState());
	}

	stateJson["pinSimulatorIdToEvalPositionMap"] = nlohmann::json::object();
	for (const auto& [simulatorId, evalPosition] : pinSimulatorIdToEvalPositionMap) {
		std::string simIdStr = std::to_string(simulatorId.get());
		if (!stateJson["pinSimulatorIdToEvalPositionMap"].contains(simIdStr)) {
			stateJson["pinSimulatorIdToEvalPositionMap"][simIdStr] = nlohmann::json::array();
		}
		stateJson["pinSimulatorIdToEvalPositionMap"][simIdStr].push_back(evalPosition.dumpState());
	}

	stateJson["circuitNodeToBlockTypeMap"] = nlohmann::json::object();
	for (const auto& [circuitNode, blockType] : circuitNodeToBlockTypeMap) {
		stateJson["circuitNodeToBlockTypeMap"][circuitNode.toString()] = blocktype_to_string(blockType);
	}



	return stateJson;
}
