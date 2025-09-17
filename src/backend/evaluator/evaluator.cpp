#include "evaluator.h"

#ifdef TRACY_PROFILER
#include <tracy/Tracy.hpp>
#endif

Evaluator::Evaluator(
	evaluator_id_t evaluatorId,
	CircuitManager& circuitManager,
	BlockDataManager& blockDataManager,
	CircuitBlockDataManager& circuitBlockDataManager,
	circuit_id_t circuitId,
	DataUpdateEventManager* dataUpdateEventManager
) : evaluatorId(evaluatorId),
circuitManager(circuitManager),
blockDataManager(blockDataManager),
circuitBlockDataManager(circuitBlockDataManager),
evalCircuitContainer(),
dataUpdateEventManager(dataUpdateEventManager),
receiver(dataUpdateEventManager),
evalConfig(),
middleIdProvider(),
evalSimulator(evalConfig, middleIdProvider, dirtySimulatorIds) {
	const auto circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) {
		logError("Circuit with ID {} not found", "Evaluator::Evaluator", circuitId);
		return;
	}
	logInfo("Creating Evaluator with ID {} for Circuit ID {}", "Evaluator", evaluatorId, circuitId);
	evalCircuitContainer.addCircuit(0, circuitId);
	const auto blockContainer = circuit->getBlockContainer();
	const Difference difference = blockContainer->getCreationDifference();
	receiver.linkFunction("circuitBlockDataConnectionPositionRemove", std::bind(&Evaluator::removeCircuitIO, this, std::placeholders::_1));
	receiver.linkFunction("circuitBlockDataConnectionPositionSet", std::bind(&Evaluator::setCircuitIO, this, std::placeholders::_1));

	makeEdit(std::make_shared<Difference>(difference), circuitId);
}

void Evaluator::makeEdit(DifferenceSharedPtr difference, circuit_id_t circuitId) {
#ifdef TRACY_PROFILER
	ZoneScoped;
#endif
	changedICs = false;
	// logInfo("_________________________________________________________________________________________");
	// logInfo("Applying edit to Evaluator with ID {} for Circuit ID {}", "Evaluator::makeEdit", evaluatorId, circuitId);
	{
		SimPauseGuard pauseGuard = evalSimulator.beginEdit();
		std::unique_lock lk(simMutex);
		DiffCache diffCache(circuitManager);
		for (eval_circuit_id_t evalCircuitId = 0; evalCircuitId < evalCircuitContainer.size(); evalCircuitId++) {
			if (evalCircuitContainer.getCircuitId(evalCircuitId) == circuitId) {
				makeEditInPlace(pauseGuard, evalCircuitId, difference, diffCache);
			}
		}
		evalSimulator.endEdit(pauseGuard);
	}
	if (changedICs) {
		dataUpdateEventManager->sendEvent("addressTreeMakeBranch");
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

	std::optional<eval_circuit_id_t> circuitId = evalCircuitContainer.getCircuitId(evalCircuitId);
	if (!circuitId.has_value()) {
		logError("EvalCircuit with id {} not found", "Evaluator::makeEditInPlace", evalCircuitId);
		return;
	}
	SharedCircuit circuit = circuitManager.getCircuit(circuitId.value());
	if (!circuit) {
		logError("Circuit with id {} not found", "Evaluator::makeEditInPlace", circuitId.value());
		return;
	}
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) {
		logError("BlockContainer not found", "Evaluator::makeEditInPlace");
		return;
	}

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
			edit_moveBlock(pauseGuard, evalCircuitId, diffCache, curPosition, curOrientation, newPosition, newOrientation);
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

void Evaluator::edit_removeBlock(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DiffCache& diffCache, Position position, Orientation orientation, BlockType type) {
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
	if (node->isIC()) {
		eval_circuit_id_t icId = node->getId();
		edit_deleteICContents(pauseGuard, icId);
		evalCircuitContainer.removeCircuit(icId);
		changedICs = true;
		evalCircuit->removeNode(position);
		return;
	}
	evalSimulator.removeGate(pauseGuard, node->getId());
	middleIdProvider.releaseId(node->getId());
	evalCircuit->removeNode(position);
}

void Evaluator::edit_deleteICContents(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId) {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_deleteIC", evalCircuitId);
		return;
	}
	evalCircuit->forEachNode([&](Position pos, const CircuitNode& node) {
		if (node.isIC()) {
			eval_circuit_id_t icId = node.getId();
			edit_deleteICContents(pauseGuard, icId);
			evalCircuitContainer.removeCircuit(icId);
			changedICs = true;
			return;
		}
		evalSimulator.removeGate(pauseGuard, node.getId());
		middleIdProvider.releaseId(node.getId());
	});
}

void Evaluator::edit_placeBlock(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DiffCache& diffCache, Position position, Orientation orientation, BlockType type) {
	GateType gateType = GateType::NONE;
	switch (type) {
	case BlockType::AND: gateType = GateType::AND; break;
	case BlockType::OR: gateType = GateType::OR; break;
	case BlockType::XOR: gateType = GateType::XOR; break;
	case BlockType::NAND: gateType = GateType::NAND; break;
	case BlockType::NOR: gateType = GateType::NOR; break;
	case BlockType::XNOR: gateType = GateType::XNOR; break;
	case BlockType::JUNCTION: gateType = GateType::JUNCTION; break;
	case BlockType::TRISTATE_BUFFER: gateType = GateType::TRISTATE_BUFFER; break;
	case BlockType::BUTTON: gateType = GateType::DUMMY_INPUT; break;
	case BlockType::SWITCH: gateType = GateType::DUMMY_INPUT; break;
	case BlockType::TICK_BUTTON: gateType = GateType::TICK_INPUT; break;
	case BlockType::CONSTANT: gateType = GateType::CONSTANT_ON; break;
	case BlockType::LIGHT: gateType = GateType::JUNCTION; break;
	default: break; // it was giving a warning
	}
	if (gateType == GateType::NONE) {
		const circuit_id_t ICId = circuitBlockDataManager.getCircuitId(type);
		if (ICId != 0) {
			edit_placeIC(pauseGuard, evalCircuitId, diffCache, position, orientation, ICId);
			return;
		}
		logError("Unsupported BlockType {}", "Evaluator::edit_placeBlock", type);
		return;
	}
	middle_id_t gateId = middleIdProvider.getNewId();
	evalSimulator.addGate(pauseGuard, gateType, gateId);
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::edit_placeBlock", evalCircuitId);
		return;
	}
	CircuitNode node = CircuitNode::fromMiddle(gateId);
	evalCircuit->setNode(position, node);
	dirtyBlockAt(position, evalCircuitId);
	checkToCreateExternalConnections(pauseGuard, evalCircuitId, position);
}

void Evaluator::edit_placeIC(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DiffCache& diffCache, Position position, Orientation orientation, circuit_id_t circuitId) {
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

void Evaluator::edit_removeConnection(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DiffCache& diffCache, const BlockContainer* blockContainer, Position outputBlockPosition, Position outputPosition, Position inputBlockPosition, Position inputPosition) {
	std::optional<EvalConnectionPoint> outputPoint = getConnectionPoint(evalCircuitId, blockContainer, outputPosition, Direction::OUT);
	if (!outputPoint.has_value()) {
		// logError("Output connection point not found for position {}", "Evaluator::edit_removeConnection", outputPosition.toString());
		return;
	}
	std::optional<EvalConnectionPoint> inputPoint = getConnectionPoint(evalCircuitId, blockContainer, inputPosition, Direction::IN);
	if (!inputPoint.has_value()) {
		// logError("Input connection point not found for position {}", "Evaluator::edit_removeConnection", inputPosition.toString());
		return;
	}
	EvalConnection connection(outputPoint.value(), inputPoint.value());
	auto it = std::find_if(interCircuitConnections.begin(), interCircuitConnections.end(),
		[&connection](const auto& pair) {
			return pair.connection == connection;
		});
	if (it != interCircuitConnections.end()) {
		interCircuitConnections.erase(it);
	}
	evalSimulator.removeConnection(pauseGuard, connection);
}

void Evaluator::edit_createConnection(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DiffCache& diffCache, const BlockContainer* blockContainer, Position outputBlockPosition, Position outputPosition, Position inputBlockPosition, Position inputPosition) {
	std::set<CircuitPortDependency> circuitPortDependencies;
	std::set<CircuitNode> circuitNodeDependencies;

	std::optional<EvalConnectionPoint> outputPoint = getConnectionPoint(evalCircuitId, outputPosition, Direction::OUT, circuitPortDependencies, circuitNodeDependencies, false);
	if (!outputPoint.has_value()) {
		return;
	}
	std::optional<EvalConnectionPoint> inputPoint = getConnectionPoint(evalCircuitId, inputPosition, Direction::IN, circuitPortDependencies, circuitNodeDependencies, false);
	if (!inputPoint.has_value()) {
		return;
	}
	EvalConnection connection(outputPoint.value(), inputPoint.value());
	if (!circuitPortDependencies.empty() || !circuitNodeDependencies.empty()) {
		interCircuitConnections.push_back({ connection, circuitPortDependencies, circuitNodeDependencies });
	}
	evalSimulator.makeConnection(pauseGuard, connection);
}

void Evaluator::removeDependentInterCircuitConnections(SimPauseGuard& pauseGuard, CircuitPortDependency circuitPortDependency) {
	// delete any connections that have the pair {circuitId, connectionEndId} in their traceSet
	for (auto it = interCircuitConnections.begin(); it != interCircuitConnections.end();) {
		if (it->circuitPortDependencies.find(circuitPortDependency) != it->circuitPortDependencies.end()) {
			evalSimulator.removeConnection(pauseGuard, it->connection);
			it = interCircuitConnections.erase(it);
		} else {
			++it;
		}
	}
}

void Evaluator::removeDependentInterCircuitConnections(SimPauseGuard& pauseGuard, CircuitNode node) {
	for (auto it = interCircuitConnections.begin(); it != interCircuitConnections.end();) {
		if (it->circuitNodeDependencies.find(node) != it->circuitNodeDependencies.end()) {
			evalSimulator.removeConnection(pauseGuard, it->connection);
			it = interCircuitConnections.erase(it);
		} else {
			++it;
		}
	}
}

void Evaluator::removeCircuitIO(const DataUpdateEventManager::EventData* data) {
	const DataUpdateEventManager::EventDataWithValue<RemoveCircuitIOData>* eventData = dynamic_cast<const DataUpdateEventManager::EventDataWithValue<RemoveCircuitIOData>*>(data);
	if (!eventData) {
		logError("Invalid event data type", "Evaluator::removeCircuitIO");
		return;
	}
	std::tuple<BlockType, connection_end_id_t, Position> dataValue = eventData->get();
	BlockType blockType = std::get<0>(dataValue);
	connection_end_id_t connectionEndId = std::get<1>(dataValue);
	Position position = std::get<2>(dataValue);

	circuit_id_t circuitId = circuitBlockDataManager.getCircuitId(blockType);
	SimPauseGuard pauseGuard = evalSimulator.beginEdit();
	removeDependentInterCircuitConnections(pauseGuard, { circuitId, connectionEndId });
	evalSimulator.endEdit(pauseGuard);
	processDirtyNodes();
}

void Evaluator::setCircuitIO(const DataUpdateEventManager::EventData* data) {
	const DataUpdateEventManager::EventDataWithValue<SetCircuitIOData>* eventData = dynamic_cast<const DataUpdateEventManager::EventDataWithValue<SetCircuitIOData>*>(data);
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
	SimPauseGuard pauseGuard = evalSimulator.beginEdit();
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
	for (eval_circuit_id_t evalCircuitId = 0; evalCircuitId < evalCircuitContainer.size(); evalCircuitId++) {
		EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
		if (!evalCircuit) {
			continue;
		}
		if (evalCircuit->getCircuitId() != circuitId) {
			continue;
		}
		checkToCreateExternalConnections(pauseGuard, evalCircuitId, *position);
	}
	evalSimulator.endEdit(pauseGuard);
	processDirtyNodes();
}

std::optional<connection_port_id_t> Evaluator::getPortId(const circuit_id_t circuitId, const Position blockPosition, const Position portPosition, Direction direction) const {
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) [[unlikely]] {
		return std::nullopt;
	}
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) [[unlikely]] {
		return std::nullopt;
	}
	return getPortId(blockContainer, blockPosition, portPosition, direction);
}

std::optional<connection_port_id_t> Evaluator::getPortId(const BlockContainer* blockContainer, const Position blockPosition, const Position portPosition, Direction direction) const {
	const Block* block = blockContainer->getBlock(blockPosition);
	if (!block) [[unlikely]] {
		return std::nullopt;
	}

	// Directly return the result based on direction
	if (direction == Direction::IN) {
		return block->getInputConnectionId(portPosition);
	} else {
		return block->getOutputConnectionId(portPosition);
	}
}

std::optional<EvalConnectionPoint> Evaluator::getConnectionPoint(const eval_circuit_id_t evalCircuitId, const Position portPosition, Direction direction) const {
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) [[unlikely]] {
		return std::nullopt;
	}
	circuit_id_t circuitId = evalCircuit->getCircuitId();
	SharedCircuit circuit = circuitManager.getCircuit(circuitId);
	if (!circuit) [[unlikely]] {
		return std::nullopt;
	}
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) [[unlikely]] {
		return std::nullopt;
	}
	return getConnectionPoint(evalCircuitId, blockContainer, portPosition, direction);
}

std::optional<EvalConnectionPoint> Evaluator::getConnectionPoint(const eval_circuit_id_t evalCircuitId, const BlockContainer* blockContainer, const Position portPosition, Direction direction) const {
	// Get the block first - this is the most likely failure point
	const Block* block = blockContainer->getBlock(portPosition);
	if (!block) [[unlikely]] {
		return std::nullopt;
	}

	const BlockType blockType = block->type();
	const Position blockPosition = block->getPosition();

	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) [[unlikely]] {
		return std::nullopt;
	}

	std::optional<CircuitNode> node = evalCircuit->getNode(blockPosition);
	if (!node.has_value()) [[unlikely]] {
		return std::nullopt;
	}

	switch (blockType) {
	case BlockType::SWITCH:
	case BlockType::BUTTON:
	case BlockType::TICK_BUTTON:
		if (direction == Direction::IN) [[likely]] {
			return EvalConnectionPoint(node->getId(), 0);
		}
		break;
	case BlockType::LIGHT:
		if (direction == Direction::OUT) [[likely]] {
			return EvalConnectionPoint(node->getId(), 0);
		}
		break;
	default:
		break;
	}

	std::optional<connection_port_id_t> portId;
	if (direction == Direction::IN) {
		const std::optional<connection_port_id_t> port = block->getInputConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	} else {
		const std::optional<connection_port_id_t> port = block->getOutputConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	}

	if (!portId.has_value()) [[unlikely]] {
		return std::nullopt;
	}

	if (!node->isIC()) [[likely]] {
		return EvalConnectionPoint(node->getId(), portId.value());
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

	return getConnectionPoint(node->getId(), *internalPosition, direction);
}

std::optional<EvalConnectionPoint> Evaluator::getConnectionPoint(
	const eval_circuit_id_t evalCircuitId,
	const Position portPosition,
	Direction direction,
	std::set<CircuitPortDependency>& circuitPortDependencies,
	std::set<CircuitNode>& circuitNodeDependencies,
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
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	const Block* block = blockContainer->getBlock(portPosition);
	if (!block) [[unlikely]] {
		return std::nullopt;
	}

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
			return EvalConnectionPoint(node->getId(), 0);
		}
		break;
	case BlockType::LIGHT:
		if (direction == Direction::OUT) [[likely]] {
			return EvalConnectionPoint(node->getId(), 0);
		}
		break;
	default:
		break;
	}

	std::optional<connection_port_id_t> portId;
	if (direction == Direction::IN) {
		const std::optional<connection_port_id_t> port = block->getInputConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	} else {
		const std::optional<connection_port_id_t> port = block->getOutputConnectionId(portPosition);
		if (port) [[likely]] {
			portId = port.value();
		}
	}

	if (!portId.has_value()) [[unlikely]] {
		return std::nullopt;
	}

	if (!node->isIC()) [[likely]] {
		return EvalConnectionPoint(node->getId(), portId.value());
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
	return getConnectionPoint(node->getId(), *internalPosition, direction, circuitPortDependencies, circuitNodeDependencies, true);
}

void Evaluator::edit_moveBlock(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, DiffCache& diffCache, Position curPosition, Orientation curOrientation, Position newPosition, Orientation newOrientation) {
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
	evalCircuit->moveNode(curPosition, newPosition);
	checkToCreateExternalConnections(pauseGuard, evalCircuitId, newPosition);
	dirtyBlockAt(newPosition, evalCircuitId);
}

const EvalAddressTree Evaluator::buildAddressTree() const {
	return buildAddressTree(0);
}

const EvalAddressTree Evaluator::buildAddressTree(eval_circuit_id_t evalCircuitId) const {
	std::shared_lock lk(simMutex);
	EvalCircuit* evalCircuit = evalCircuitContainer.getCircuit(evalCircuitId);
	if (!evalCircuit) {
		logError("EvalCircuit with id {} not found", "Evaluator::buildAddressTree", evalCircuitId);
		return EvalAddressTree(0);
	}
	EvalAddressTree root = EvalAddressTree(evalCircuit->getCircuitId());
	evalCircuit->forEachNode([this, &root](Position pos, const CircuitNode& node) {
		if (node.isIC()) {
			root.addBranch(pos, buildAddressTree(node.getId()));
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
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) {
		logError("BlockContainer not found", "Evaluator::getMiddleId");
		return std::nullopt;
	}
	Position blockPosition = address.getPosition(address.size() - 1);
	const Block* block = blockContainer->getBlock(blockPosition);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::getMiddleId", blockPosition.toString());
		return std::nullopt;
	}
	std::optional<CircuitNode> node = evalCircuitContainer.getNode(block->getPosition(), evalCircuitId);
	if (!node.has_value()) {
		logError("Node not found for address {}", "Evaluator::getMiddleId", address.toString());
		return std::nullopt;
	}
	return node->getId();
}

std::optional<middle_id_t> Evaluator::getMiddleId(const eval_circuit_id_t startingPoint, const Address& address, const BlockContainer* blockContainer) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(startingPoint, address);
	Position blockPosition = address.getPosition(address.size() - 1);
	const Block* block = blockContainer->getBlock(blockPosition);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::getMiddleId", blockPosition.toString());
		return std::nullopt;
	}
	std::optional<CircuitNode> node = evalCircuitContainer.getNode(block->getPosition(), evalCircuitId);
	if (!node.has_value()) {
		logError("Node not found for address {}", "Evaluator::getMiddleId", address.toString());
		return std::nullopt;
	}
	return node->getId();
}

std::optional<middle_id_t> Evaluator::getMiddleId(const Address& address) const {
	return getMiddleId(0, address);
}

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
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) {
		logError("BlockContainer not found", "Evaluator::getState");
		return logic_state_t::UNDEFINED;
	}

	std::optional<EvalConnectionPoint> connectionPointOpt = getConnectionPoint(evalCircuitId, blockContainer, address.getPosition(address.size() - 1), Direction::OUT);
	if (!connectionPointOpt.has_value()) {
		logError("Connection point not found for address {}", "Evaluator::getState", address.toString());
		return logic_state_t::UNDEFINED;
	}
	return evalSimulator.getState(connectionPointOpt.value());
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
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) {
		logError("BlockContainer not found", "Evaluator::setState");
		return;
	}

	std::optional<EvalConnectionPoint> connectionPointOpt = getConnectionPoint(evalCircuitId, blockContainer, address.getPosition(address.size() - 1), Direction::OUT);
	if (connectionPointOpt.has_value()) {
		evalSimulator.setState(connectionPointOpt.value(), state);
		return;
	}
	std::optional<EvalConnectionPoint> connectionPointOptIn = getConnectionPoint(evalCircuitId, blockContainer, address.getPosition(address.size() - 1), Direction::IN);
	if (connectionPointOptIn.has_value()) {
		evalSimulator.setState(connectionPointOptIn.value(), state);
		return;
	}
	std::optional<middle_id_t> middleIdOpt = getMiddleId(evalCircuitId, address, blockContainer);
	if (middleIdOpt.has_value()) {
		EvalConnectionPoint connectionPoint(middleIdOpt.value(), 0);
		evalSimulator.setState(connectionPoint, state);
		return;
	}
	logError("Failed to get connection point for address {}", "Evaluator::setState", address.toString());
}

void Evaluator::checkToCreateExternalConnections(SimPauseGuard& pauseGuard, eval_circuit_id_t evalCircuitId, Position position) {
	// logInfo("Checking to create external connections for evalCircuitId {} at position {}", "Evaluator::checkToCreateExternalConnections", evalCircuitId, position.toString());
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
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) {
		logError("BlockContainer not found", "Evaluator::makeEditInPlace");
		return;
	}
	const Block* block = blockContainer->getBlock(position);
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
	std::vector<ConnectionData> connectionDataList;
	if (blockData->isDefaultData()) {
		// logInfo("Block type {} is default data", "Evaluator::checkToCreateExternalConnections", static_cast<int>(block->type()));
		connectionDataList.push_back({ position, Direction::IN });
		connectionDataList.push_back({ position, Direction::OUT });
	} else {
		const auto& connections = blockData->getConnections();
		// logInfo("Found {} connections for block type {}", "Evaluator::checkToCreateExternalConnections", connections.size(), static_cast<int>(block->type()));
		for (const auto& [connectionId, connectionOffset] : connections) {
			Vector portOffset = connectionOffset.first;
			Position portPosition = block->getPosition() + portOffset;
			// Determine direction (input or output)
			Direction direction = block->isConnectionInput(connectionId) ? Direction::IN : Direction::OUT;
			// logInfo("Connection {} at position {} with direction {}", "Evaluator::checkToCreateExternalConnections", connectionId, portPosition.toString(), (direction == Direction::IN ? "IN" : "OUT"));
			connectionDataList.push_back({ portPosition, direction });
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

		// logInfo("Checking connection at position {} with direction {}", "Evaluator::checkToCreateExternalConnections", portPosition.toString(), (direction == Direction::IN ? "IN" : "OUT"));

		std::set<CircuitPortDependency> circuitPortDependencies;
		std::set<CircuitNode> circuitNodeDependencies;
		circuitNodeDependencies.insert(node.value());
		std::optional<EvalConnectionPoint> connectionPoint = getConnectionPoint(
			evalCircuitId, portPosition, direction, circuitPortDependencies, circuitNodeDependencies, false);

		if (connectionPoint.has_value()) {
			traceOutwardsIC(
				pauseGuard,
				evalCircuitId,
				portPosition,
				direction,
				connectionPoint.value(),
				circuitPortDependencies,
				circuitNodeDependencies
			);
		} else {
			logError("Connection point not found for position {}", "Evaluator::checkToCreateExternalConnections", portPosition.toString());
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
	std::set<CircuitNode>& circuitNodeDependencies
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
	const BlockContainer* innerBlockContainer = innerCircuit->getBlockContainer();
	if (!innerBlockContainer) {
		logError("BlockContainer for inner circuit ID {} not found", "Evaluator::traceOutwardsIC", evalCircuit->getCircuitId());
		return;
	}
	const Block* block = innerBlockContainer->getBlock(position);
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
	const BlockContainer* parentCircuitBlockContainer = parentCircuit->getBlockContainer();
	if (!parentCircuitBlockContainer) {
		logError("BlockContainer not found", "Evaluator::traceOutwardsIC");
		return;
	}
	std::optional<Position> parentCircuitPositionOpt = parentEvalCircuit->getPosition(CircuitNode::fromIC(evalCircuitId));
	if (!parentCircuitPositionOpt.has_value()) {
		logError("Parent circuit position not found for evalCircuitId {}", "Evaluator::traceOutwardsIC", evalCircuitId);
		return;
	}
	const Block* parentCircuitBlock = parentCircuitBlockContainer->getBlock(parentCircuitPositionOpt.value());
	if (!parentCircuitBlock) {
		logError("Block not found at position {}", "Evaluator::traceOutwardsIC", parentCircuitPositionOpt.value().toString());
		return;
	}

	circuitNodeDependencies.insert(node.value());

	BidirectionalMultiSecondKeyMap<connection_end_id_t, Position>::constIteratorPairT2 connectionIds = circuitBlockData->getConnectionPositionToId(position);

	for (auto iter = connectionIds.first; iter != connectionIds.second; ++iter) {
		connection_end_id_t connectionEndId = iter->second;
		if (parentCircuitBlock->isConnectionInput(connectionEndId) != (direction == Direction::IN)) {
			continue;
		}
		std::optional<Position> connectionPosOpt = parentCircuitBlock->getConnectionPosition(connectionEndId);
		if (!connectionPosOpt) {
			logError("Connection position not found at connectionEndId {}", "Evaluator::traceOutwardsIC", connectionEndId);
			continue;
		}
		Position connectionPos = connectionPosOpt.value();
		const phmap::flat_hash_set<ConnectionEnd>* connectionEnds = (direction == Direction::IN) ?
			parentCircuitBlock->getInputConnections(connectionPos) : parentCircuitBlock->getOutputConnections(connectionPos);
		if (!connectionEnds) {
			// logError("Connection ends not found at position {}", "Evaluator::traceOutwardsIC", connectionPos.toString());
			continue;
		}

		circuitPortDependencies.insert({ circuitId, connectionEndId });

		for (const ConnectionEnd& connectionEnd : *connectionEnds) {
			const Block* targetBlock = parentCircuitBlockContainer->getBlock(connectionEnd.getBlockId());
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
			std::optional<EvalConnectionPoint> connectionPoint = getConnectionPoint(
				parentEvalCircuitId, targetConnectionPointPosition, !direction, circuitPortDependenciesCopy, circuitNodeDependenciesCopy, false);
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
			evalSimulator.makeConnection(pauseGuard, evalConnection);
			interCircuitConnections.push_back({
				evalConnection,
				circuitPortDependenciesCopy,
				circuitNodeDependenciesCopy
			});

			traceOutwardsIC(
				pauseGuard,
				parentEvalCircuitId,
				targetConnectionPointPosition,
				direction,
				targetConnectionPoint,
				circuitPortDependencies,
				circuitNodeDependencies
			);
		}

		circuitPortDependencies.erase({ circuitId, connectionEndId });
	}

	circuitNodeDependencies.erase(node.value());
}

void Evaluator::processDirtyNodes() {
	for (const simulator_id_t id : dirtySimulatorIds) {
		{
			auto it = portSimulatorIdToEvalPositionMap.equal_range(id);
			for (auto iter = it.first; iter != it.second; ++iter) {
				const auto& [simulatorId, evalPosition] = *iter;
				dirtyNodes.insert(evalPosition);
			}
			portSimulatorIdToEvalPositionMap.erase(id);
		}
		{
			auto it = pinSimulatorIdToEvalPositionMap.equal_range(id);
			for (auto iter = it.first; iter != it.second; ++iter) {
				const auto& [simulatorId, evalPosition] = *iter;
				dirtyNodes.insert(evalPosition);
			}
			pinSimulatorIdToEvalPositionMap.erase(id);
		}
	}
	dirtySimulatorIds.clear();

	std::vector<EvalPosition> dirtyNodesToProcess;
	std::vector<EvalConnectionPoint> connectionPointsToRequest;

	for (const EvalPosition& evalPosition : dirtyNodes) {
		std::optional<EvalConnectionPoint> connectionPoint = getConnectionPoint(evalPosition.evalCircuitId, evalPosition.position, Direction::OUT);
		if (!connectionPoint.has_value()) {
			continue;
		}
		dirtyNodesToProcess.push_back(evalPosition);
		connectionPointsToRequest.push_back(connectionPoint.value());
	}
	dirtyNodes.clear();

	std::vector<SimulatorStateAndPinSimId> simulatorIdPairs = evalSimulator.getSimulatorIds(connectionPointsToRequest);

	std::unordered_map<eval_circuit_id_t, std::vector<SimulatorMappingUpdate>> simulatorMappingUpdates;

	for (size_t i = 0; i < dirtyNodesToProcess.size(); ++i) {
		const EvalPosition& evalPosition = dirtyNodesToProcess.at(i);
		const SimulatorStateAndPinSimId& simulatorIdPair = simulatorIdPairs.at(i);
		simulator_id_t portSimId = simulatorIdPair.portSimId;
		simulator_id_t pinSimId = simulatorIdPair.pinSimId;
		portSimulatorIdToEvalPositionMap.insert({ portSimId, evalPosition });
		pinSimulatorIdToEvalPositionMap.insert({ pinSimId, evalPosition });
		simulatorMappingUpdates[evalPosition.evalCircuitId].push_back({
			evalPosition.position,
			portSimId,
			SimulatorMappingUpdateType::BLOCK
		});
		simulatorMappingUpdates[evalPosition.evalCircuitId].push_back({
			evalPosition.position,
			pinSimId,
			SimulatorMappingUpdateType::PIN
		});
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
	const BlockContainer* blockContainer = circuit->getBlockContainer();
	if (!blockContainer) {
		logError("BlockContainer not found", "Evaluator::dirtyBlockAt");
		return;
	}
	const Block* block = blockContainer->getBlock(position);
	if (!block) {
		logError("Block not found at position {}", "Evaluator::dirtyBlockAt", position.toString());
		return;
	}
	if (block->type() == BlockType::LIGHT) {
		dirtyNodes.insert({position, evalCircuitId});
		return;
	}
	const BlockData* blockData = blockDataManager.getBlockData(block->type());
	if (!blockData) {
		logError("BlockData not found for block type {}", "Evaluator::dirtyBlockAt", static_cast<int>(block->type()));
		return;
	}
	for (connection_end_id_t i = 0; i < blockData->getConnectionCount(); ++i) {
		if (block->isConnectionOutput(i)) {
			std::optional<Position> portPositionOpt = block->getConnectionPosition(i);
			if (!portPositionOpt) {
				logError("Port position not found for connection ID {}", "Evaluator::dirtyBlockAt", i);
				continue;
			}
			dirtyNodes.insert({portPositionOpt.value(), evalCircuitId});
		}
	}
}

std::vector<simulator_id_t> Evaluator::getBlockSimulatorIds(const Address& addressOrigin, const std::vector<Position>& positions) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(addressOrigin);
	std::vector<std::optional<EvalConnectionPoint>> connectionPoints;
	connectionPoints.reserve(positions.size());
	for (const Position& portPosition : positions) {
		connectionPoints.push_back(getConnectionPoint(evalCircuitId, portPosition, Direction::OUT));
	}
	return evalSimulator.getBlockSimulatorIds(connectionPoints);
}

std::vector<simulator_id_t> Evaluator::getPinSimulatorIds(const Address& addressOrigin, const std::vector<Position>& positions) const {
	eval_circuit_id_t evalCircuitId = evalCircuitContainer.traverseToTopLevelIC(addressOrigin);
	std::vector<std::optional<EvalConnectionPoint>> connectionPoints;
	connectionPoints.reserve(positions.size());
	for (const Position& portPosition : positions) {
		connectionPoints.push_back(getConnectionPoint(evalCircuitId, portPosition, Direction::OUT));
	}
	return evalSimulator.getPinSimulatorIds(connectionPoints);
}

std::vector<logic_state_t> Evaluator::getStatesFromSimulatorIds(const std::vector<simulator_id_t>& simulatorIds) const {
	return evalSimulator.getStatesFromSimulatorIds(simulatorIds);
}

void Evaluator::connectListener(
	void* object,
	const Address& address,
	SimulatorMappingUpdateListenerFunction func
) {
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
	evalCircuit->forEachNode([this, evalCircuitId](Position pos, const CircuitNode& node) {
		this->dirtyBlockAt(pos, evalCircuitId.value());
	});
	processDirtyNodes();
}

void Evaluator::waitForSprintComplete() {
	while (evalConfig.getSprintCount() > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
