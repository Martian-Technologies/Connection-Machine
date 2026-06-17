#include "evaluator.h"

#include "backend/circuit/circuitManager.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "evaluatorInternal.h"
#include "simulatorManager.h"

Evaluator::~Evaluator() = default;

Evaluator::Evaluator(
	CircuitManager& circuitManager,
	const Circuit& circuit,
	DataUpdateEventManager& dataUpdateEventManager
) :
	circuitManager(circuitManager),
	dataUpdateEventManager(dataUpdateEventManager),
	receiver(dataUpdateEventManager),
	circuit(circuit),
	evaluatorInternal(std::make_unique<EvaluatorInternal>(circuit, *this, circuitManager,  receiver))
{
	logInfo("Creating Evaluator for Circuit ID {}", "Evaluator", circuit.getCircuitId());
	const BlockContainer& blockContainer = circuit.getBlockContainer();
	const Difference difference = blockContainer.getCreationDifference();

	makeEdit(std::make_shared<Difference>(difference));
}

void Evaluator::makeEdit(DifferenceSharedPtr difference) {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	startEdit();
	for (const Difference::Modification& modification : difference->getModifications()) {
		const auto& [modificationType, modificationData] = modification;
		switch (modificationType) {
		case Difference::ModificationType::REMOVED_BLOCK: {
			const auto& [position, orientation, blockType] = std::get<Difference::block_modification_t>(modificationData);
			evaluatorInternal->removeBlock(position, orientation, blockType);
			break;
		}
		case Difference::ModificationType::PLACE_BLOCK: {
			const auto& [position, orientation, blockType] = std::get<Difference::block_modification_t>(modificationData);
			evaluatorInternal->addBlock(position, orientation, blockType);
			break;
		}
		case Difference::ModificationType::MOVE_BLOCK: {
			const auto& [curPosition, curOrientation, newPosition, newOrientation, finalMove] = std::get<Difference::move_modification_t>(modificationData);
			evaluatorInternal->moveBlock(curPosition, curOrientation, newPosition, newOrientation);
			break;
		}
		case Difference::ModificationType::REMOVED_CONNECTION: {
			const auto& [outputBlockPosition, outputPosition, inputBlockPosition, inputPosition] = std::get<Difference::connection_modification_t>(modificationData);
			evaluatorInternal->removeConnection(outputBlockPosition, outputPosition, inputBlockPosition, inputPosition);
			break;
		}
		case Difference::ModificationType::CREATED_CONNECTION: {
			const auto& [outputBlockPosition, outputPosition, inputBlockPosition, inputPosition] = std::get<Difference::connection_modification_t>(modificationData);
			evaluatorInternal->createConnection(outputBlockPosition, outputPosition, inputBlockPosition, inputPosition);
			break;
		}
		}
	}
	endEdit();
}

circuit_id_t Evaluator::getCircuitId() const { return circuit.getCircuitId(); }

std::vector<std::pair<Position, circuit_id_t>> Evaluator::getSubcircuits() const { return evaluatorInternal->getSubcircuits(); }

nlohmann::json Evaluator::dumpState() const /* GCOVR_EXCL_FUNCTION */ {
	nlohmann::json stateJson;
	stateJson["circuitId"] = circuit.getCircuitId();
	stateJson["interCircuitConnections"] = nlohmann::json::array();
	stateJson["dirtySimulatorIds"] = nlohmann::json::array();
	stateJson["dirtyMiddleIds"] = nlohmann::json::array();
	stateJson["dirtyNodes"] = nlohmann::json::array();
	stateJson["pinSimulatorIdToEvalPositionMap"] = nlohmann::json::object();
	stateJson["circuitNodeToBlockTypeMap"] = nlohmann::json::object();

	return stateJson;
}

void Evaluator::startEdit() {
	evaluatorInternal->startEdit();
}

void Evaluator::endEdit() {
	#ifdef TRACY_PROFILER
	ZoneScoped;
	#endif
	evaluatorInternal->endEdit();
	for (EvalLogicSimulator* simulator : simulatorsUsingThisEvaluator) simulator->processEdits();
	dataUpdateEventManager.sendEvent("evalUpdate");
}
