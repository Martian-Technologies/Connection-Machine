#include "fuzzTestcase.h"
#include "testcaseMinifier.h"
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
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
	// std::unique_ptr<FuzzTestcase> outputTestcase = std::make_unique<FuzzTestcase>(testcase.getBlockTypesUsed());
	// Environment environment { false };
	// circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	// SharedCircuit circuit = environment.getBackend().getCircuit(circuitId);
	// evaluator_id_t evalId = environment.getBackend().createEvaluator(circuitId).value();
	// SharedEvaluator tEval = environment.getBackend().getEvaluator(evalId); // test evaluator
	// SharedEvaluator rEval = nullptr; // reference evaluator

	// std::vector<BlockType> blockTypesUsed = makeBlockTypesUsableVector(testcase.getBlockTypesUsed(), environment);

	// std::vector<block_id_t> blockIds;

	// for (size_t i = 0; i < testcase.getEditActions().size(); ++i) {
	// 	if (editActions.contains(i)) continue;
	// 	FuzzEditAction action = testcase.getEditActions()[i];
	// 	std::visit([&](auto&& arg) {
	// 		using T = std::decay_t<decltype(arg)>;
	// 		if constexpr (std::is_same_v<T, PlaceBlockAction>) {
	// 			int fuzzBlockTypeIndex = arg.fuzzBlockTypeIndex;
	// 			BlockType blockType = blockTypesUsed[fuzzBlockTypeIndex];
	// 			bool success = circuit->tryInsertBlock(arg.position, arg.orientation, blockType);
	// 			if (success) {
	// 				outputTestcase->addEditAction(arg);
	// 				const Block* block = circuit->getBlockContainer().getBlock(arg.position);
	// 				blockIds.push_back(block->id());
	// 			}
	// 		} else if constexpr (std::is_same_v<T, RemoveBlockAction>) {
	// 			const Block* block = circuit->getBlockContainer().getBlock(arg.position);
	// 			if (block != nullptr) {
	// 				block_id_t blockId = block->id();
	// 				bool success = circuit->tryRemoveBlock(arg.position);
	// 				if (success) {
	// 					outputTestcase->addEditAction(arg);
	// 					auto it = std::find(blockIds.begin(), blockIds.end(), blockId);
	// 					if (it != blockIds.end()) {
	// 						std::swap(*it, blockIds.back());
	// 						blockIds.pop_back();
	// 					}
	// 				}
	// 			}
	// 		} else if constexpr (std::is_same_v<T, CreateConnectionAction>) {
	// 			bool success = circuit->tryCreateConnection(arg.source, arg.destination);
	// 			if (success) outputTestcase->addEditAction(arg);
	// 		} else if constexpr (std::is_same_v<T, RemoveConnectionAction>) {
	// 			bool success = circuit->tryRemoveConnection(arg.source, arg.destination);
	// 			if (success) outputTestcase->addEditAction(arg);
	// 		}
	// 	}, action);
	// }

	// evaluator_id_t refEvalId = environment.getBackend().createEvaluator(circuitId).value();
	// rEval = environment.getBackend().getEvaluator(refEvalId);

	// tEval->setRealistic(testcase.getRunRealistic());
	// rEval->setRealistic(testcase.getRunRealistic());
	// outputTestcase->setRealistic(testcase.getRunRealistic());

	// tEval->resetStates();
	// rEval->resetStates();

	// std::vector<simulator_id_t> simulatorIdsTest;
	// std::vector<simulator_id_t> simulatorIdsRef;
	// std::unordered_map<block_id_t, Position> blockIdToPosition;

	// BlockDataManager& blockDataManager = environment.getBackend().getBlockDataManager();

	// for (block_id_t blockId : blockIds) {
	// 	const Block* block = circuit->getBlockContainer().getBlock(blockId);
	// 	if (block == nullptr) continue;
	// 	Position pos = block->getPosition();
	// 	blockIdToPosition[blockId] = pos;
	// 	simulatorIdsTest.push_back(tEval->getBlockSimulatorId(pos));
	// 	simulatorIdsRef.push_back(rEval->getBlockSimulatorId(pos));
	// 	const BlockData* blockData = blockDataManager.getBlockData(block->type());
	// 	if (blockData == nullptr) continue;
	// 	if (blockData->isDefaultData()) {
	// 		std::variant<simulator_id_t, std::vector<simulator_id_t>> simIdTest = tEval->getPinSimulatorId(pos);
	// 		std::variant<simulator_id_t, std::vector<simulator_id_t>> simIdRef = rEval->getPinSimulatorId(pos);
	// 		if (std::holds_alternative<simulator_id_t>(simIdTest) && std::holds_alternative<simulator_id_t>(simIdRef)) {
	// 			simulatorIdsTest.push_back(std::get<simulator_id_t>(simIdTest));
	// 			simulatorIdsRef.push_back(std::get<simulator_id_t>(simIdRef));
	// 		} else if (std::holds_alternative<std::vector<simulator_id_t>>(simIdTest) && std::holds_alternative<std::vector<simulator_id_t>>(simIdRef)) {
	// 			std::vector<simulator_id_t>& vecTest = std::get<std::vector<simulator_id_t>>(simIdTest);
	// 			std::vector<simulator_id_t>& vecRef = std::get<std::vector<simulator_id_t>>(simIdRef);
	// 			simulatorIdsTest.insert(simulatorIdsTest.end(), vecTest.begin(), vecTest.end());
	// 			simulatorIdsRef.insert(simulatorIdsRef.end(), vecRef.begin(), vecRef.end());
	// 		} else {
	// 			return outputTestcase;
	// 		}
	// 	} else {
	// 		const std::unordered_map<connection_end_id_t, BlockData::ConnectionData>& connections = blockData->getConnections();
	// 		for (const auto& [connectionId, connectionData] : connections) {
	// 			if (connectionData.portType == BlockData::ConnectionData::PortType::INPUT) {
	// 				continue;
	// 			}
	// 			std::optional<Position> portPositionOpt = block->getConnectionPosition(connectionId);
	// 			Position portPosition = portPositionOpt.value();
	// 			std::variant<simulator_id_t, std::vector<simulator_id_t>> simIdTest = tEval->getPinSimulatorId(portPosition);
	// 			std::variant<simulator_id_t, std::vector<simulator_id_t>> simIdRef = rEval->getPinSimulatorId(portPosition);
	// 			if (std::holds_alternative<simulator_id_t>(simIdTest) && std::holds_alternative<simulator_id_t>(simIdRef)) {
	// 				simulatorIdsTest.push_back(std::get<simulator_id_t>(simIdTest));
	// 				simulatorIdsRef.push_back(std::get<simulator_id_t>(simIdRef));
	// 			} else if (std::holds_alternative<std::vector<simulator_id_t>>(simIdTest) && std::holds_alternative<std::vector<simulator_id_t>>(simIdRef)) {
	// 				std::vector<simulator_id_t>& vecTest = std::get<std::vector<simulator_id_t>>(simIdTest);
	// 				std::vector<simulator_id_t>& vecRef = std::get<std::vector<simulator_id_t>>(simIdRef);
	// 				if (vecTest.size() != vecRef.size()) {
	// 					return outputTestcase;
	// 				}
	// 				simulatorIdsTest.insert(simulatorIdsTest.end(), vecTest.begin(), vecTest.end());
	// 				simulatorIdsRef.insert(simulatorIdsRef.end(), vecRef.begin(), vecRef.end());

	// 			} else {
	// 				return outputTestcase;
	// 			}
	// 		}
	// 	}
	// }

	// for (int i = 0; i < testcase.getTestActions().size(); ++i) {
	// 	if (testActions.contains(i)) continue;
	// 	FuzzTestAction action = testcase.getTestActions()[i];
	// 	std::visit([&](auto&& arg) {
	// 		using T = std::decay_t<decltype(arg)>;
	// 		if constexpr (std::is_same_v<T, SetBlockStateAction>) {
	// 			tEval->setState(arg.position, arg.state);
	// 			rEval->setState(arg.position, arg.state);
	// 			outputTestcase->addTestAction(arg);
	// 		} else if constexpr (std::is_same_v<T, TickEvalAction>) {
	// 			tEval->tickStep(arg.numTicks);
	// 			rEval->tickStep(arg.numTicks);
	// 			outputTestcase->addTestAction(arg);
	// 		}
	// 	}, action);
	// }

	// std::vector<logic_state_t> statesTest = tEval->getStatesFromSimulatorIds(simulatorIdsTest);
	// std::vector<logic_state_t> statesRef = rEval->getStatesFromSimulatorIds(simulatorIdsRef);
	// for (size_t k = 0; k < statesTest.size(); ++k) {
	// 	if (statesTest[k] != statesRef[k]) {
	// 		return outputTestcase;
	// 	}
	// }
	// return nullptr;
}
