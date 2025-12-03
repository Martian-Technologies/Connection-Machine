#include "fuzzRunner.h"
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"

FuzzRunner::FuzzRunner(Environment& environment) : environment(environment) {
	SharedCircuit circuit = getCircuit(0);
	evaluator_id_t evalId = environment.getBackend().createEvaluator(circuit->getCircuitId()).value();
	testEvaluator = environment.getBackend().getEvaluator(evalId);
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
		bool success = circuit->tryRemoveBlock(act.position);
		if (!success) return false;
		block_id_t blockId = block->id();
		getBlockIdsForCircuit(action.circuitIndex).erase(blockId);
		return true;
	} else if (std::holds_alternative<CreateConnectionAction>(uAction)) {
		const CreateConnectionAction& act = std::get<CreateConnectionAction>(uAction);
		return circuit->tryCreateConnection(act.source, act.destination);
	} else if (std::holds_alternative<RemoveConnectionAction>(uAction)) {
		const RemoveConnectionAction& act = std::get<RemoveConnectionAction>(uAction);
		return circuit->tryRemoveConnection(act.source, act.destination);
	}
	logError("Unkown edit action variant", "FuzzRunner::applyEditAction");
	return false;
}

void FuzzRunner::applyTestAction(const FuzzTestAction& action) {
	ensureReferenceEvaluatorExists();
	auto& uAction = action.action;
	if (std::holds_alternative<SetBlockStateAction>(uAction)) {
		const SetBlockStateAction& act = std::get<SetBlockStateAction>(uAction);
		testEvaluator->setState(act.position, act.state);
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
}

void FuzzRunner::ensureReferenceEvaluatorExists() {
	if (referenceEvaluator == nullptr) {
		SharedCircuit circuit = getCircuit(0);
		evaluator_id_t evalId = environment.getBackend().createEvaluator(circuit->getCircuitId()).value();
		referenceEvaluator = environment.getBackend().getEvaluator(evalId);
	}
}
