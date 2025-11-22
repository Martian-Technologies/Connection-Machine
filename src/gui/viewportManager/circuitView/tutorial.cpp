#include "tutorial.h"

#include "circuitView.h"
#include "environment/environment.h"
#include "events/customEvents.h"

std::vector<TutorialStep> basicTutorialInitialize();

Tutorial::Tutorial(Environment& environment, CircuitView& circuitView) :
	circuitView(&circuitView), elementCreator(circuitView.getViewportId()), environment(environment), tutorialRunning(false), tutorialState(0) { }

void Tutorial::StartTutorial() {
	if (tutorialRunning) return;
	tutorialRunning = true;
	tutorialState = 0;
	circuit_id_t circuitId = circuitView->getBackend().getCircuitManager().createNewCircuit(false);
	std::optional<evaluator_id_t> evaluatorId = circuitView->getBackend().createEvaluator(circuitId);
	if (!evaluatorId) return;
	circuitView->setEvaluator(evaluatorId.value());

	evaluator = circuitView->getBackend().getEvaluator(evaluatorId.value());
	SharedCircuit circuit = circuitView->getBackend().getCircuitManager().getCircuit(circuitId);
	curentCircuit = circuitView->getBackend().getCircuitManager().getCircuit(circuitId);
	curentCircuit->connectListener(this, std::bind(&Tutorial::checkTutorial, this, std::placeholders::_1, std::placeholders::_2));
	circuitView->getEventRegister().registerFunction("CircuitStateSet", [this](const Event* event) -> bool {
		const StateSetEvent* stateSetEvent = event->cast<StateSetEvent>();
		if (!stateSetEvent) return false;
		this->checkTutorialState(stateSetEvent->getPosition(), stateSetEvent->getState());
		return false;
	});
	std::string a("BasicTutorial.tir");
	parseTutorialFile(a);
	tutorialSteps = basicTutorialInitialize();
	runCurrentStep();
}

void Tutorial::Stop() {
	if (!tutorialRunning) return;
	if (curentCircuit) {
		curentCircuit->disconnectListener(this);
	}
	elementCreator.clear();
	tutorialRunning = false;
}
void Tutorial::checkTutorial(DifferenceSharedPtr, circuit_id_t) { advanceTutorial(); }
void Tutorial::checkTutorialState(Position pos, logic_state_t state) { advanceTutorial(); }

void Tutorial::setTutorial(const std::vector<TutorialStep>& steps) { tutorialSteps = steps; }

void Tutorial::advanceTutorial() {
	if (!tutorialRunning) return;
	if (tutorialState >= tutorialSteps.size()) return;
	if (isCurrentStepComplete()) {
		tutorialState++;
		elementCreator.clear();
		if (tutorialState == tutorialSteps.size()) {
			Stop();
			return;
		}
		runCurrentStep();
	}
}

bool Tutorial::isCurrentStepComplete() const {
	if (tutorialState >= tutorialSteps.size()) return false;
	TutorialStep currentStep = tutorialSteps[tutorialState];
	BlockContainer blockContainer = curentCircuit->getBlockContainer();

	for (std::vector<TutorialCondition::BlockRequirement>::iterator it = currentStep.condition.blocks.begin(); it != currentStep.condition.blocks.end(); it++) {
		const Block* currentBlock = blockContainer.getBlock(it->pos);
		if (currentBlock == nullptr) {
			return false;
		}
		if (currentBlock->type() != it->type) {
			return false;
		}
		if (currentBlock->getOrientation() != it->orientation) {
			return false;
		}
	}
	for (std::vector<TutorialCondition::ConnectionRequirement>::iterator it = currentStep.condition.connections.begin(); it != currentStep.condition.connections.end();
		 it++) {
		if (!blockContainer.connectionExists(it->pos1, it->pos2)) {
			return false;
		}
	}
	for (std::vector<TutorialCondition::LogicStateRequirement>::iterator it = currentStep.condition.logicStates.begin(); it != currentStep.condition.logicStates.end();
		 it++) {
		evaluator->tickStep(it->numSteps);
		if (evaluator->getState(Address(it->pos)) != it->state) {
			return false;
		}
	}
	return true;
}

void Tutorial::runCurrentStep() {
	if (tutorialState >= tutorialSteps.size()) return;
	TutorialStep currentStep = tutorialSteps[tutorialState];
	BlockContainer blockContainer = curentCircuit->getBlockContainer();
	// change this later to real popups or something
	for (std::vector<std::string>::iterator it = currentStep.action.messages.begin(); it != currentStep.action.messages.end(); it++) {
		logInfo(*it);
	}
	for (std::vector<TutorialAction::BlockPreviewInfo>::iterator it = currentStep.action.blockPreviews.begin(); it != currentStep.action.blockPreviews.end(); it++) {
		elementCreator.addBlockPreview(BlockPreview(environment.getBlockRenderDataFeeder().getBlockRenderDataId(it->type), it->pos, it->orientation));
	}
	for (std::vector<TutorialAction::ConnectionPreviewInfo>::iterator it = currentStep.action.connectionPreviews.begin();
		 it != currentStep.action.connectionPreviews.end();
		 it++) {
		std::optional<FVector> optionalPos1Offset;
		if (blockContainer.getBlock(it->pos1) != nullptr) {
			optionalPos1Offset = blockContainer.getBlockDataManager()
									 .getBlockData(blockContainer.getBlock(it->pos1)->type())
									 ->getConnectionPortOffset(blockContainer.getOutputConnectionEnd(it->pos1).value().getConnectionId());
		} else {
			for (std::vector<TutorialAction::BlockPreviewInfo>::iterator it2 = currentStep.action.blockPreviews.begin(); it2 != currentStep.action.blockPreviews.end();
				 it2++) {
				if (it2->pos == it->pos1) {
					optionalPos1Offset = blockContainer.getBlockDataManager().getBlockData(it2->type)->getConnectionPortOffset((connection_end_id_t)0);
				}
			}
		}
		std::optional<FVector> optionalPos2Offset;
		if (blockContainer.getBlock(it->pos2) != nullptr) {
			optionalPos2Offset = blockContainer.getBlockDataManager()
									 .getBlockData(blockContainer.getBlock(it->pos2)->type())
									 ->getConnectionPortOffset(blockContainer.getInputConnectionEnd(it->pos2).value().getConnectionId());
		} else {
			for (std::vector<TutorialAction::BlockPreviewInfo>::iterator it2 = currentStep.action.blockPreviews.begin(); it2 != currentStep.action.blockPreviews.end();
				 it2++) {
				if (it2->pos == it->pos2) {
					optionalPos2Offset = blockContainer.getBlockDataManager().getBlockData(it2->type)->getConnectionPortOffset((connection_end_id_t)0);
				}
			}
		}
		if (!optionalPos1Offset.has_value()) {
			optionalPos1Offset = FVector(0.5, 0.5);
		}
		if (!optionalPos2Offset.has_value()) {
			optionalPos1Offset = FVector(0.5, 0.5);
		}
		elementCreator.addConnectionPreview(
			ConnectionPreview(FPosition(it->pos1.x, it->pos1.y) + optionalPos1Offset.value(), FPosition(it->pos2.x, it->pos2.y) + optionalPos2Offset.value())
		);
	}
}

std::vector<TutorialStep> basicTutorialInitialize() {
	std::vector<TutorialStep> steps;
	// step 0
	{
		TutorialStep s;
		s.action.messages = { "Welcome to the Connection Machine tutorial.",
							  "Click the 'Switch' button on the left side menu, and click to place 2 switches where prompted." };
		s.action.blockPreviews = { { Position(0, 0), BlockType::SWITCH, Orientation() }, { Position(0, 2), BlockType::SWITCH, Orientation() } };
		s.condition.blocks = { { Position(0, 0), BlockType::SWITCH, Orientation() }, { Position(0, 2), BlockType::SWITCH, Orientation() } };

		steps.push_back(s);
	}

	// step 1
	{
		TutorialStep s;
		s.action.messages = { "Click the 'AND' button on the left side menu, and place an AND block where prompted.",
							  "The AND block outputs ON only when all inputs are ON." };
		s.action.blockPreviews = { { Position(2, 1), BlockType::AND, Orientation() } };
		s.condition.blocks = { { Position(2, 1), BlockType::AND } };

		steps.push_back(s);
	}

	// step 2
	{
		TutorialStep s;
		s.action.messages = { "Click the 'Connection' tool and connect both switches to the AND block." };
		s.action.connectionPreviews = { { Position(0, 0), Position(2, 1) }, { Position(0, 2), Position(2, 1) } };
		s.condition.connections = { { Position(0, 0), Position(2, 1) }, { Position(0, 2), Position(2, 1) } };

		steps.push_back(s);
	}

	// step 3
	{
		TutorialStep s;
		s.condition.blocks = { { Position(4, 1), BlockType::LIGHT } };
		s.condition.connections = { { Position(2, 1), Position(4, 1) } };
		s.action.messages = { "Place a LIGHT and connect the AND output to the LIGHT." };
		s.action.blockPreviews = { { Position(4, 1), BlockType::LIGHT, Orientation() } };
		s.action.connectionPreviews = { { Position(2, 1), Position(4, 1) } };
		steps.push_back(s);
	}

	// step 4
	{
		TutorialStep s;
		s.condition.logicStates = { { Position(2, 1), logic_state_t::HIGH, 2 } };

		s.action.messages = { "Switch to the state changer and flip the switches so the AND output turns ON." };
		steps.push_back(s);
	}

	// // step 5
	// {
	// 	TutorialStep s;
	// 	s.condition.blocks = { { Position(2, 0), BlockType::LIGHT } };

	// 	s.action.messages = { "Place an extra LIGHT at (2, 0) to finish." };
	// 	s.action.blockPreviews = { { Position(2, 0), BlockType::LIGHT, Orientation() } };
	// 	steps.push_back(s);
	// }

	// step 6
	{
		TutorialStep s;
		s.action.messages = { "Tutorial complete." };
		steps.push_back(s);
	}
	return steps;
}

