#include "fuzzRunner.h"
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
#include "computerAPI/directoryManager.h"

FuzzRunner::FuzzRunner(
	Environment& environment,
	std::vector<FuzzBlockType> blockTypesUsed
) : environment(environment),
	blockTypesUsed(blockTypesUsed) {
	SharedCircuit circuit = getCircuit(0);
	evaluator_id_t evalId = environment.getBackend().createEvaluator(circuit->getCircuitId()).value();
	testEvaluator = environment.getBackend().getEvaluator(evalId);
	for (const FuzzBlockType& blockType : blockTypesUsed) {
		if (std::holds_alternative<FuzzOtherCircuitType>(blockType.type)) {
			const FuzzOtherCircuitType& otherType = std::get<FuzzOtherCircuitType>(blockType.type);
			circuitIndices.push_back(otherType.circuitIndex);
		}
	}
	populateBlockTypesUsedConverted();
}

SharedCircuit FuzzRunner::getCircuit(unsigned int index) {
	if (index >= circuits.size()) {
		circuits.resize(index + 1);
	}
	if (circuits[index] == nullptr) {
		circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
		SharedCircuit circuit = environment.getBackend().getCircuit(circuitId);
		circuits[index] = circuit;
	}
	return circuits[index];
}

bool FuzzRunner::applyEditAction(const FuzzEditAction& action) {
	resetReferenceEvaluator();
	auto& uAction = action.action;
	SharedCircuit circuit = getCircuit(action.circuitIndex);
	if (std::holds_alternative<PlaceBlockAction>(uAction)) {
		const PlaceBlockAction& act = std::get<PlaceBlockAction>(uAction);
		BlockType blockType = getBlockTypeFromFuzzIndex(act.fuzzBlockTypeIndex);
		bool success = circuit->tryInsertBlock(act.position, act.orientation, blockType);
		if (!success) return false;
		const Block* block = circuit->getBlockContainer().getBlock(act.position);
		getBlockIdsForCircuit(action.circuitIndex).insert(block->id());
		return true;
	} else if (std::holds_alternative<RemoveBlockAction>(uAction)) {
		const RemoveBlockAction& act = std::get<RemoveBlockAction>(uAction);
		const Block* block = circuit->getBlockContainer().getBlock(act.position);
		if (block == nullptr) return false;
		block_id_t blockId = block->id();
		bool success = circuit->tryRemoveBlock(act.position);
		if (!success) return false;
		getBlockIdsForCircuit(action.circuitIndex).erase(blockId);
		return true;
	} else if (std::holds_alternative<CreateConnectionAction>(uAction)) {
		const CreateConnectionAction& act = std::get<CreateConnectionAction>(uAction);
		return circuit->tryCreateConnection(act.source, act.destination);
	} else if (std::holds_alternative<RemoveConnectionAction>(uAction)) {
		const RemoveConnectionAction& act = std::get<RemoveConnectionAction>(uAction);
		return circuit->tryRemoveConnection(act.source, act.destination);
	} else if (std::holds_alternative<MoveBlockAction>(uAction)) {
		const MoveBlockAction& act = std::get<MoveBlockAction>(uAction);
		return circuit->tryMoveBlock(act.oldPosition, act.newPosition, act.orientationOffset);
	}
	logError("Unknown edit action variant", "FuzzRunner::applyEditAction");
	return false;
}

void FuzzRunner::applyTestAction(const FuzzTestAction& action) {
	ensureReferenceEvaluatorExists();
	auto& uAction = action.action;
	if (std::holds_alternative<SetBlockStateAction>(uAction)) {
		const SetBlockStateAction& act = std::get<SetBlockStateAction>(uAction);
		testEvaluator->setState(act.address, act.state);
		return;
	} else if (std::holds_alternative<TickEvalAction>(uAction)) {
		const TickEvalAction& act = std::get<TickEvalAction>(uAction);
		testEvaluator->tickStep(act.numTicks);
		referenceEvaluator->tickStep(act.numTicks);
		return;
	}
	logError("Unkown test action variant", "FuzzRunner::applyTestAction");
}

IndexableUnorderedSet<block_id_t>& FuzzRunner::getBlockIdsForCircuit(unsigned int index) {
	if (index >= blockIdsPerCircuit.size()) {
		blockIdsPerCircuit.resize(index + 1);
	}
	return blockIdsPerCircuit[index];
}

void FuzzRunner::resetReferenceEvaluator() {
	referenceEvaluator = nullptr;
	resetSimIds();
}

void FuzzRunner::ensureReferenceEvaluatorExists() {
	if (referenceEvaluator == nullptr) {
		SharedCircuit circuit = getCircuit(0);
		evaluator_id_t evalId = environment.getBackend().createEvaluator(circuit->getCircuitId()).value();
		referenceEvaluator = environment.getBackend().getEvaluator(evalId);
	}
}

namespace {

std::optional<connection_end_id_t> getRandomConnectionEnd(std::mt19937_64& gen, const BlockData* blockData, bool wantInput) {
	if (blockData->isDefaultData()) {
		if (wantInput) {
			return connection_end_id_t(0);
		} else {
			return connection_end_id_t(1);
		}
	}
	int numConnections = blockData->getBidirectionalConnectionCount().get();
	if (wantInput) numConnections += blockData->getInputConnectionCount().get();
	else numConnections += blockData->getOutputConnectionCount().get();
	if (numConnections == 0) return std::nullopt;
	std::uniform_int_distribution<int> dist(0, numConnections - 1);
	int index = dist(gen);
	const std::unordered_map<connection_end_id_t, BlockData::ConnectionData>& connections = blockData->getConnections();
	int i = 0;
	for (auto& pair : connections) {
		if (wantInput) {
			if (pair.second.portType == BlockData::ConnectionData::PortType::INPUT || pair.second.portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
				if (i == index) return pair.first;
				i++;
			}
		} else {
			if (pair.second.portType == BlockData::ConnectionData::PortType::OUTPUT || pair.second.portType == BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
				if (i == index) return pair.first;
				i++;
			}
		}
	}
	return std::nullopt;
}

} // namespace

FuzzEditAction FuzzRunner::createRandomEditAction(std::mt19937_64& gen) {
	unsigned int circuitIndex = circuitIndices[gen() % circuitIndices.size()];
	SharedCircuit circuit = getCircuit(circuitIndex);
	int operation = gen() % 12; // 0-11
	if (operation <= 1) { // place block
		Orientation orientation(Rotation(gen() % 4), gen() % 2 == 0);
		int fuzzBlockTypeIndex = gen() % blockTypesUsed.size();
		BlockType blockType = getBlockTypeFromFuzzIndex(fuzzBlockTypeIndex);
		Position pos(gen() % 41 - 20, gen() % 41 - 20);
		return FuzzEditAction { PlaceBlockAction { pos, fuzzBlockTypeIndex, orientation }, circuitIndex };
	} else if (operation <= 2) { // remove block
		IndexableUnorderedSet<block_id_t>& blockIds = getBlockIdsForCircuit(circuitIndex);
		if (blockIds.size() == 0) {
			return createRandomEditAction(gen);
		}
		block_id_t blockId = blockIds.at(gen() % blockIds.size());
		const Block* block = circuit->getBlockContainer().getBlock(blockId);
		return FuzzEditAction { RemoveBlockAction { block->getPosition() }, circuitIndex };
	} else if (operation <= 7) { // connect / disconnect
		IndexableUnorderedSet<block_id_t>& blockIds = getBlockIdsForCircuit(circuitIndex);
		if (blockIds.empty()) {
			return createRandomEditAction(gen);
		}
		block_id_t blockIdA = blockIds.at(gen() % blockIds.size());
		block_id_t blockIdB = blockIds.at(gen() % blockIds.size());
		const Block* blockA = circuit->getBlockContainer().getBlock(blockIdA);
		const Block* blockB = circuit->getBlockContainer().getBlock(blockIdB);
		if (blockA == nullptr || blockB == nullptr) {
			return createRandomEditAction(gen);
		}
		BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
		const BlockData* blockDataA = blockDataManager.getBlockData(blockA->type());
		const BlockData* blockDataB = blockDataManager.getBlockData(blockB->type());
		if (blockDataA == nullptr || blockDataB == nullptr) {
			return createRandomEditAction(gen);
		}
		std::optional<connection_end_id_t> connA = getRandomConnectionEnd(gen, blockDataA, false);
		std::optional<connection_end_id_t> connB = getRandomConnectionEnd(gen, blockDataB, true);
		if (!connA.has_value() || !connB.has_value()) {
			return createRandomEditAction(gen);
		}
		Position posA = blockA->getConnectionPosition(*connA).value();
		Position posB = blockB->getConnectionPosition(*connB).value();
		if (operation <= 5) { // connect
			return FuzzEditAction { CreateConnectionAction { posA, posB }, circuitIndex };
		} else { // disconnect
			return FuzzEditAction { RemoveConnectionAction { posA, posB }, circuitIndex };
		}
	} else if (operation <= 8) { // add circuit IO port
		return createRandomEditAction(gen); // TODO
		bool isInput = gen() % 2 == 0;
		// find a port index that is not used
		BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
		BlockType circuitBlockType = circuit->getBlockType();
		BlockData* blockData = blockDataManager.getBlockData(circuitBlockType);
		int portIndex = 0;
		while (true) {
			if (!blockData->connectionExists(connection_end_id_t(portIndex))) {
				break;
			}
		}
		Size circuitSize = blockData->getSize();
		Vector portOffset(gen() % circuitSize.w, gen() % circuitSize.h);
		// check that no other port uses this offset
		std::optional<connection_end_id_t> existingPort = isInput ? blockData->getInputConnectionId(portOffset) : blockData->getOutputConnectionId(portOffset);
		if (existingPort.has_value()) {
			return createRandomEditAction(gen);
		}
		unsigned int bitWidth = gen() % 8 + 1;
		return FuzzEditAction { UpdateCircuitIOAction { isInput, portOffset, bitWidth, connection_end_id_t(portIndex) }, circuitIndex };
	} else if (operation <= 9) { // remove circuit IO port
		return createRandomEditAction(gen); // TODO
		BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
		BlockType circuitBlockType = circuit->getBlockType();
		BlockData* blockData = blockDataManager.getBlockData(circuitBlockType);
		// find a port to remove
		std::vector<connection_end_id_t> possiblePorts;
		for (const auto& pair : blockData->getConnections()) {
			if (pair.second.portType == BlockData::ConnectionData::PortType::INPUT || pair.second.portType == BlockData::ConnectionData::PortType::OUTPUT) {
				possiblePorts.push_back(pair.first);
			}
		}
		if (possiblePorts.size() == 0) {
			return createRandomEditAction(gen);
		}
		connection_end_id_t portId = possiblePorts[gen() % possiblePorts.size()];
		return FuzzEditAction { RemoveCircuitIOAction { portId }, circuitIndex };
	} else if (operation <= 10) { // modify circuit IO port
		return createRandomEditAction(gen); // TODO
	} else if (operation <= 12) { // move block
		IndexableUnorderedSet<block_id_t>& blockIds = getBlockIdsForCircuit(circuitIndex);
		if (blockIds.size() == 0) {
			return createRandomEditAction(gen);
		}
		block_id_t blockId = blockIds.at(gen() % blockIds.size());
		const Block* block = circuit->getBlockContainer().getBlock(blockId);
		Position newPos(gen() % 41 - 20, gen() % 41 - 20);
		Orientation orientationOffset(Rotation(gen() % 4), gen() % 2 == 0);
		return FuzzEditAction { MoveBlockAction { block->getPosition(), newPos, orientationOffset }, circuitIndex };
	}
}

Address FuzzRunner::pickRandomAddress(std::mt19937_64& gen, Address root, unsigned int circuitIndex) {
	SharedCircuit circuit = getCircuit(circuitIndex);
	const BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	const CircuitBlockDataManager& circuitBlockDataManager = environment.getBackend().getCircuitManager().getCircuitBlockDataManager();
	IndexableUnorderedSet<block_id_t>& blockIds = getBlockIdsForCircuit(circuitIndex);
	block_id_t blockId = blockIds.at(gen() % blockIds.size());
	const Block* block = circuit->getBlockContainer().getBlock(blockId);
	BlockType blockType = block->type();
	circuit_id_t circuitId = circuit->getCircuitId();
	root.addBlockId(block->getPosition());
	if (circuitId == 0) {
		return root;
	}

	for (unsigned int i = 0; i < circuits.size(); ++i) {
		SharedCircuit circuit = circuits[i];
		if (circuit->getCircuitId() == circuitId) {
			return pickRandomAddress(gen, root, i);
		}
	}
}

SetBlockStateAction FuzzRunner::createRandomSetStateAction(std::mt19937_64& gen) {
	Address address = pickRandomAddress(gen, Address(), 0);
	logic_state_t state = static_cast<logic_state_t>(gen() % 3);
	return SetBlockStateAction { address, state };
}

BlockType FuzzRunner::getBlockTypeFromFuzzIndex(int fuzzBlockTypeIndex) {
	return blockTypesUsedConverted[fuzzBlockTypeIndex];
}

void FuzzRunner::populateBlockTypesUsedConverted() {
	blockTypesUsedConverted.clear();
	for (const FuzzBlockType& fuzzBlockType : blockTypesUsed) {
		blockTypesUsedConverted.push_back(getBlockTypeFromFuzzBlockType(fuzzBlockType));
	}
}


BlockType FuzzRunner::getBlockTypeFromFuzzBlockType(const FuzzBlockType& fuzzBlockType) {
	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	auto& type = fuzzBlockType.type;
	if (std::holds_alternative<FuzzPrimitiveType>(type)) {
		return std::get<FuzzPrimitiveType>(type).blockType;
	} else if (std::holds_alternative<FuzzBusType>(type)) {
		return blockDataManager.getBusBlock(
			std::get<FuzzBusType>(type).numInputs,
			std::get<FuzzBusType>(type).numOutputs,
			std::get<FuzzBusType>(type).inputLaneWidth,
			std::get<FuzzBusType>(type).outputLaneWidth
		);
	} else if (std::holds_alternative<FuzzCustomCircuitType>(type)) {
		CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
		circuit_id_t circuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / std::get<FuzzCustomCircuitType>(type).path).string()).at(0);
		SharedCircuit circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
		return circuit->getBlockType();
	} else if (std::holds_alternative<FuzzOtherCircuitType>(type)) {
		const FuzzOtherCircuitType& otherType = std::get<FuzzOtherCircuitType>(type);
		SharedCircuit circuit = getCircuit(otherType.circuitIndex);
		return circuit->getBlockType();
	}
	logError("Unknown FuzzBlockType {}", "FuzzRunner::getBlockTypeFromFuzzBlockType", fuzzBlockType.toJson().dump());
	return BlockType::NONE;
}

void FuzzRunner::resetSimIds() {
	simIdsInitialized = false;
}

void FuzzRunner::ensureSimIdsInitialized() {
	if (!simIdsInitialized) {
		testEvaluatorBlockSimIds.clear();
		referenceEvaluatorBlockSimIds.clear();
		addSimulatorIds(testEvaluatorBlockSimIds, Address(), testEvaluator, 0);
		ensureReferenceEvaluatorExists();
		addSimulatorIds(referenceEvaluatorBlockSimIds, Address(), referenceEvaluator, 0);
		simIdsInitialized = true;
	}
}

void FuzzRunner::addSimulatorIds(std::vector<simulator_id_t>& destination, Address rootAddress, SharedEvaluator evaluator, unsigned int circuitIndex) {
	SharedCircuit circuit = getCircuit(circuitIndex);
	const BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();
	const CircuitBlockDataManager& circuitBlockDataManager = environment.getBackend().getCircuitManager().getCircuitBlockDataManager();
	IndexableUnorderedSet<block_id_t>& blockIds = getBlockIdsForCircuit(circuitIndex);
	for (size_t i = 0; i < blockIds.size(); ++i) {
		block_id_t blockId = blockIds.at(i);
		const Block* block = circuit->getBlockContainer().getBlock(blockId);
		BlockType blockType = block->type();
		circuit_id_t circuitId = circuit->getCircuitId();
		Address blockAddress = rootAddress;
		blockAddress.addBlockId(block->getPosition());
		if (circuitId != 0) { // this is a subcircuit
			for (unsigned int i = 0; i < circuits.size(); ++i) {
				SharedCircuit circuit = circuits[i];
				if (circuit->getCircuitId() == circuitId) {
					addSimulatorIds(destination, blockAddress, evaluator, i);
					break;
				}
			}
		} else { // just a normal block
			simulator_id_t simId = evaluator->getBlockSimulatorId(blockAddress);
			destination.push_back(simId);
		}
		// go through pins
		const BlockData* blockData = blockDataManager.getBlockData(blockType);
		for (const auto& pair : blockData->getConnections()) {
			const BlockData::ConnectionData& connData = pair.second;
			if (connData.portType != BlockData::ConnectionData::PortType::OUTPUT && connData.portType != BlockData::ConnectionData::PortType::BIDIRECTIONAL) {
				continue;
			}

			connection_end_id_t connEndId = pair.first;
			Position connPos = block->getConnectionPosition(connEndId).value();
			Address pinAddress = blockAddress;
			pinAddress.addBlockId(connPos);
			std::variant<simulator_id_t, std::vector<simulator_id_t>> pinSimId = evaluator->getPinSimulatorId(pinAddress);
			if (std::holds_alternative<simulator_id_t>(pinSimId)) {
				destination.push_back(std::get<simulator_id_t>(pinSimId));
			} else {
				const std::vector<simulator_id_t>& simIds = std::get<std::vector<simulator_id_t>>(pinSimId);
				destination.insert(destination.end(), simIds.begin(), simIds.end());
			}
		}
	}
}

bool FuzzRunner::checkEvaluatorsMatch() {
	ensureSimIdsInitialized();
	if (testEvaluatorBlockSimIds.size() != referenceEvaluatorBlockSimIds.size()) {
		return false;
	}
	std::vector<logic_state_t> testStates = testEvaluator->getStatesFromSimulatorIds(testEvaluatorBlockSimIds);
	std::vector<logic_state_t> referenceStates = referenceEvaluator->getStatesFromSimulatorIds(referenceEvaluatorBlockSimIds);
	return testStates == referenceStates;
}
