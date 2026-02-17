#include "fuzzTestcase.h"
#include "testcaseMinifier.h"
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "backend/container/block/blockDefs.h"

FuzzTestcase TestcaseMinifier::minifyTestcase(const FuzzTestcase& originalTestcase) {
	logInfo("----------------------------------------Starting testcase minification: {} edit actions, {} test actions", "", originalTestcase.getEditActions().size(), originalTestcase.getTestActions().size());
	// std::mt19937_64 gen;
	// gen.seed(std::random_device {}());

	FuzzTestcase currentTestcase = originalTestcase;
	while (true) {
		bool reduced = false;
		for (int i = 0; i < currentTestcase.getTestActions().size() + currentTestcase.getEditActions().size(); ++i) {
			logInfo("Trying to remove action {}/{}...", "", i + 1, currentTestcase.getTestActions().size() + currentTestcase.getEditActions().size());
			std::set<size_t> editActionsToTry;
			std::set<size_t> testActionsToTry;
			size_t numEditActions = currentTestcase.getEditActions().size();
			size_t numTestActions = currentTestcase.getTestActions().size();
			if (i < numTestActions) {
				testActionsToTry.insert(i);
			} else {
				editActionsToTry.insert(i - numTestActions);
			}
			std::unique_ptr<FuzzTestcase> newTestcase = tryRemoveEditActions(currentTestcase, editActionsToTry, testActionsToTry);
			if (newTestcase) {
				currentTestcase = std::move(*newTestcase);
				// gen.seed(std::random_device {}());
				reduced = true;
				break;
			}
		}
		if (!reduced) {
			break;
		}
	}
	currentTestcase.tryRemoveBlockTypesNotUsed();
	logInfo("Finished testcase minification: {} edit actions, {} test actions", "", currentTestcase.getEditActions().size(), currentTestcase.getTestActions().size());
	return currentTestcase;
}

std::unique_ptr<FuzzTestcase> TestcaseMinifier::tryRemoveEditActions(const FuzzTestcase& testcase, std::set<size_t> editActions, std::set<size_t> testActions) {
	std::unique_ptr<FuzzTestcase> outputTestcase = std::make_unique<FuzzTestcase>(testcase.getBlockTypesUsed());
	Environment environment { false };
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	SharedCircuit circuit = environment.getBackend().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	EvalLogicSimulator* tSimulator = environment.getBackend().getSimulator(simulatorId); // test evaluator

	std::vector<BlockType> blockTypesUsed = makeBlockTypesUsableVector(testcase.getBlockTypesUsed(), environment);

	std::vector<block_id_t> blockIds;

	for (size_t i = 0; i < testcase.getEditActions().size(); ++i) {
		if (editActions.contains(i)) continue;
		FuzzEditAction action = testcase.getEditActions()[i];
		std::visit([&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, PlaceBlockAction>) {
				int fuzzBlockTypeIndex = arg.fuzzBlockTypeIndex;
				BlockType blockType = blockTypesUsed[fuzzBlockTypeIndex];
				bool success = circuit->tryInsertBlock(arg.position, arg.orientation, blockType);
				if (success) {
					outputTestcase->addEditAction(arg);
					const Block* block = circuit->getBlockContainer().getBlock(arg.position);
					blockIds.push_back(block->id());
				}
			} else if constexpr (std::is_same_v<T, RemoveBlockAction>) {
				const Block* block = circuit->getBlockContainer().getBlock(arg.position);
				if (block != nullptr) {
					block_id_t blockId = block->id();
					bool success = circuit->tryRemoveBlock(arg.position);
					if (success) {
						outputTestcase->addEditAction(arg);
						auto it = std::find(blockIds.begin(), blockIds.end(), blockId);
						if (it != blockIds.end()) {
							std::swap(*it, blockIds.back());
							blockIds.pop_back();
						}
					}
				}
			} else if constexpr (std::is_same_v<T, CreateConnectionAction>) {
				// const Block* sourceBlock = circuit->getBlockContainer().getBlock(arg.source);
				// const Block* destBlock = circuit->getBlockContainer().getBlock(arg.destination);
				// // find connection end IDs at the positions
				// if (sourceBlock == nullptr || destBlock == nullptr) {
				// 	return;
				// }
				// std::optional<connection_end_id_t> sourceConnectionIdOpt = sourceBlock->getOutputOrBidirectionalConnectionId(arg.source);
				// std::optional<connection_end_id_t> destConnectionIdOpt = destBlock->getInputOrBidirectionalConnectionId(arg.destination);
				// if (!sourceConnectionIdOpt.has_value() || !destConnectionIdOpt.has_value()) {
				// 	return;
				// }
				// connection_end_id_t sourceConnectionId = sourceConnectionIdOpt.value();
				// connection_end_id_t destConnectionId = destConnectionIdOpt.value();
				// bool success = circuit->tryCreateConnection({sourceBlock->id(), sourceConnectionId}, {destBlock->id(), destConnectionId});
				bool success = circuit->tryCreateConnection(arg.source, arg.destination);
				if (success) outputTestcase->addEditAction(arg);
			} else if constexpr (std::is_same_v<T, RemoveConnectionAction>) {
				// const Block* sourceBlock = circuit->getBlockContainer().getBlock(arg.source);
				// const Block* destBlock = circuit->getBlockContainer().getBlock(arg.destination);
				// // find connection end IDs at the positions
				// if (sourceBlock == nullptr || destBlock == nullptr) {
				// 	return;
				// }
				// std::optional<connection_end_id_t> sourceConnectionIdOpt = sourceBlock->getOutputOrBidirectionalConnectionId(arg.source);
				// std::optional<connection_end_id_t> destConnectionIdOpt = destBlock->getInputOrBidirectionalConnectionId(arg.destination);
				// if (!sourceConnectionIdOpt.has_value() || !destConnectionIdOpt.has_value()) {
				// 	return;
				// }
				// connection_end_id_t sourceConnectionId = sourceConnectionIdOpt.value();
				// connection_end_id_t destConnectionId = destConnectionIdOpt.value();
				// bool success = circuit->tryRemoveConnection({ sourceBlock->id(), sourceConnectionId }, { destBlock->id(), destConnectionId });
				bool success = circuit->tryRemoveConnection(arg.source, arg.destination);
				if (success) outputTestcase->addEditAction(arg);
			}
		}, action);
	}

	Evaluator rEvaluator(environment.getBackend().getCircuitManager(), *circuit, environment.getBackend().getDataUpdateEventManager());
	simulator_id_t refSimulatorId = simulator_id_t(1000000 + simulatorId.get());
	EvalLogicSimulator rSimulator(refSimulatorId, environment.getBackend().getCircuitManager(), circuitId, environment.getBackend().getDataUpdateEventManager());

	tSimulator->setRealistic(testcase.getRunRealistic());
	rSimulator.setRealistic(testcase.getRunRealistic());
	outputTestcase->setRealistic(testcase.getRunRealistic());

	tSimulator->resetStates();
	rSimulator.resetStates();

	std::vector<simulator_state_reference> simulatorIdsTest;
	std::vector<simulator_state_reference> simulatorIdsRef;
	std::unordered_map<block_id_t, Position> blockIdToPosition;

	BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();

	for (block_id_t blockId : blockIds) {
		const Block* block = circuit->getBlockContainer().getBlock(blockId);
		if (block == nullptr) continue;
		Position pos = block->getPosition();
		blockIdToPosition[blockId] = pos;
		simulatorIdsTest.push_back(std::get<simulator_state_reference>(tSimulator->getVirtualConnectionSimulatorId(Address(pos), 0)));
		simulatorIdsRef.push_back(std::get<simulator_state_reference>(tSimulator->getVirtualConnectionSimulatorId(Address(pos), 0)));
		const BlockData* blockData = blockDataManager.getBlockData(block->type());
		if (blockData == nullptr) continue;
		if (blockData->isDefaultData()) {
			std::unordered_map<connection_end_id_t, BlockData::ConnectionData> connections = blockData->getConnectionsSafe();
			for (const auto& [connectionId, connectionData] : connections) {
				if (connectionData.portType == BlockData::ConnectionData::PortType::INPUT) {
					continue;
				}
				std::optional<Position> portPositionOpt = block->getConnectionPosition(connectionId);
				Position portPosition = portPositionOpt.value();
				std::variant<simulator_state_reference, std::vector<simulator_state_reference>> simIdTest = tSimulator->getPinSimulatorId(portPosition);
				std::variant<simulator_state_reference, std::vector<simulator_state_reference>> simIdRef = rSimulator.getPinSimulatorId(portPosition);
				if (std::holds_alternative<simulator_state_reference>(simIdTest) && std::holds_alternative<simulator_state_reference>(simIdRef)) {
					simulatorIdsTest.push_back(std::get<simulator_state_reference>(simIdTest));
					simulatorIdsRef.push_back(std::get<simulator_state_reference>(simIdRef));
				} else if (std::holds_alternative<std::vector<simulator_state_reference>>(simIdTest) && std::holds_alternative<std::vector<simulator_state_reference>>(simIdRef)) {
					std::vector<simulator_state_reference>& vecTest = std::get<std::vector<simulator_state_reference>>(simIdTest);
					std::vector<simulator_state_reference>& vecRef = std::get<std::vector<simulator_state_reference>>(simIdRef);
					if (vecTest.size() != vecRef.size()) {
						return outputTestcase;
					}
					simulatorIdsTest.insert(simulatorIdsTest.end(), vecTest.begin(), vecTest.end());
					simulatorIdsRef.insert(simulatorIdsRef.end(), vecRef.begin(), vecRef.end());

				} else {
					return outputTestcase;
				}
			}
		}
	}

	for (int i = 0; i < testcase.getTestActions().size(); ++i) {
		if (testActions.contains(i)) continue;
		FuzzTestAction action = testcase.getTestActions()[i];
		std::visit([&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, SetBlockStateAction>) {
				tSimulator->setState(arg.position, arg.state);
				rSimulator.setState(arg.position, arg.state);
				outputTestcase->addTestAction(arg);
			} else if constexpr (std::is_same_v<T, TickEvalAction>) {
				tSimulator->tickStep(arg.numTicks);
				rSimulator.tickStep(arg.numTicks);
				outputTestcase->addTestAction(arg);
			}
		}, action);
	}

	std::vector<logic_state_t> statesTest = tSimulator->getStates(simulatorIdsTest);
	std::vector<logic_state_t> statesRef = rSimulator.getStates(simulatorIdsRef);
	for (size_t k = 0; k < statesTest.size(); ++k) {
		if (statesTest[k] != statesRef[k]) {
			return outputTestcase;
		}
	}
	return nullptr;
}
