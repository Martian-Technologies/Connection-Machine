#include "tutorial.h"
#include "backend/position/position.h"
#include "circuitView.h"

#include "environment/environment.h"
#include "events/customEvents.h"
#include "gui/mainWindow/widgets/circuitViewWidget.h"

TutorialManager::TutorialManager(Environment& environment, CircuitView& circuitView) :
	circuitView(circuitView), elementCreator(circuitView.getViewportId()), viewManager(circuitView.getViewManager()), environment(environment),
	dataUpdateEventReciever(environment.getBackend().getDataUpdateEventManager()), tutorialRunning(false), tutorialState(0) {
	this->circuitView.getEventRegister().registerFunction("CircuitStateSet", [this](const Event* event) -> bool {
		if (!tutorialRunning) return false;
		const StateSetEvent* stateSetEvent = event->cast<StateSetEvent>();
		if (!stateSetEvent) return false;
		this->checkTutorialState(stateSetEvent->getPosition(), stateSetEvent->getState());
		return false;
	});
	dataUpdateEventReciever.linkFunction("circuitViewChangeCircuit", [this](const DataUpdateEventManager::EventData* event) -> bool {
		elementCreator.clear();
		if (this->circuitView.getCircuit() && circuitId == this->circuitView.getCircuit()->getCircuitId()) {
			runCurrentStep();
		}
		return false;
	});
}

void TutorialManager::StartTutorial() {
	if (tutorialRunning) return;
	if (tutorialSteps.tutorialSteps.empty()) return;
	tutorialRunning = true;
	tutorialState = 0;
	circuitId = circuitView.getBackend().getCircuitManager().createNewCircuit(false);
	std::optional<simulator_id_t> simulatorId = circuitView.getBackend().createSimulator(circuitId);
	if (!simulatorId) {
		logError("Failed to create simulator.", "Tutorial::StartTutorial");
		return;
	}
	circuitView.setSimulator(simulatorId.value());
	simulator = circuitView.getBackend().getSimulator(simulatorId.value());
	Circuit* currentCircuit = circuitView.getBackend().getCircuitManager().getCircuit(circuitId);
	currentCircuit->connectListener(this, std::bind(&TutorialManager::checkTutorial, this, std::placeholders::_1, std::placeholders::_2));
	runCurrentStep();
}

void TutorialManager::stop() {
	if (!tutorialRunning) return;
	Circuit* currentCircuit = circuitView.getBackend().getCircuitManager().getCircuit(circuitId);
	if (currentCircuit) {
		currentCircuit->disconnectListener(this);
	}
	elementCreator.clear();
	tutorialRunning = false;
}
void TutorialManager::checkTutorial(DifferenceSharedPtr, circuit_id_t) { advanceTutorial(); }
void TutorialManager::checkTutorialState(Position pos, logic_state_t state) { advanceTutorial(); }

void TutorialManager::setTutorial(const Tutorial& tutorial) { tutorialSteps = tutorial; }

void TutorialManager::midStepChecks() {
	if (!tutorialRunning) return;
	if (tutorialState >= tutorialSteps.tutorialSteps.size()) return;
	const TutorialStep& currentStep = tutorialSteps.tutorialSteps[tutorialState];
	Circuit* currentCircuit = circuitView.getBackend().getCircuitManager().getCircuit(circuitId);
	if (!currentCircuit) return;
}

void TutorialManager::advanceTutorial() {
	if (!tutorialRunning) return;
	if (tutorialState >= tutorialSteps.tutorialSteps.size()) return;
	if (isCurrentStepComplete()) {
		tutorialState++;
		elementCreator.clear();
		if (tutorialState == tutorialSteps.tutorialSteps.size()) {
			stop();
			return;
		}
		runCurrentStep();
	}
}

bool TutorialManager::isCurrentStepComplete() const {
	if (tutorialState >= tutorialSteps.tutorialSteps.size()) return false;
	const TutorialStep& currentStep = tutorialSteps.tutorialSteps[tutorialState];
	Circuit* currentCircuit = circuitView.getBackend().getCircuitManager().getCircuit(circuitId);
	if (!currentCircuit) return false;
	const BlockContainer& blockContainer = currentCircuit->getBlockContainer();

	for (const TutorialCondition::BlockRequirement& blockCondition : currentStep.condition.blocks) {
		const Block* currentBlock = blockContainer.getBlock(blockCondition.pos);
		if (currentBlock == nullptr) {
			return false;
		}
		if (currentBlock->type() != blockCondition.type) {
			return false;
		}
		if (currentBlock->getOrientation() != blockCondition.orientation) {
			return false;
		}
	}
	for (const TutorialCondition::ConnectionRequirement& connectionCondition : currentStep.condition.connections) {
		if (!blockContainer.connectionExists(connectionCondition.pos1, connectionCondition.pos2)) {
			return false;
		}
	}
	for (const TutorialCondition::LogicStateRequirement stateCondition : currentStep.condition.logicStates) {
		simulator->tickStep(stateCondition.numSteps);
		logic_state_t b = simulator->getState(Address(stateCondition.pos));
		if (simulator->getState(Address(stateCondition.pos)) != stateCondition.state) {
			logic_state_t a = simulator->getState(Address(stateCondition.pos));
			return false;
		}
	}
	return true;
}

void TutorialManager::runCurrentStep() {
	if (tutorialState >= tutorialSteps.tutorialSteps.size()) return;
	const TutorialStep& currentStep = tutorialSteps.tutorialSteps[tutorialState];
	Circuit* currentCircuit = circuitView.getBackend().getCircuitManager().getCircuit(circuitId);
	if (!currentCircuit) return;
	const BlockContainer& blockContainer = currentCircuit->getBlockContainer();
	// change this later to real popups or something
	// yippee i changed it later to real pop ups or something
	for (const TutorialAction::Message& message : currentStep.action.messages) {
		elementCreator.addText(message.message, message.pos, message.scale);
	}
	for (const TutorialAction::BlockInfo& block : currentStep.action.blocks) {
		currentCircuit->tryInsertBlock(block.pos, block.orientation, block.type);
	}
	for (const TutorialAction::BlockInfo& blockPreview : currentStep.action.blockPreviews) {
		elementCreator.addBlockPreview(
			BlockPreview(environment.getBlockRenderDataFeeder().getBlockRenderDataId(blockPreview.type), blockPreview.pos, blockPreview.orientation)
		);
	}
	for (const TutorialAction::ConnectionPreviewInfo& connectionPreview : currentStep.action.connectionPreviews) {
		std::optional<FVector> optionalPos1Offset;
		const Block* block1 = blockContainer.getBlock(connectionPreview.pos1);
		if (block1 != nullptr) {
			optionalPos1Offset = blockContainer.getBlockDataManager()
									 .getBlockData(block1->type())
									 ->getConnectionPortOffset(blockContainer.getOutputConnectionEnd(connectionPreview.pos1).value().getConnectionId());
		} else {
			for (const TutorialAction::BlockInfo& blockPreview : currentStep.action.blockPreviews) {
				if (connectionPreview.pos1.withinArea(
						blockPreview.pos,
						blockPreview.pos +
							(blockPreview.orientation * blockContainer.getBlockDataManager().getBlockData(blockPreview.type)->getSize()).getLargestVectorInArea()
					)) {
					optionalPos1Offset = blockContainer.getBlockDataManager().getBlockData(blockPreview.type)->getConnectionPortOffset((connection_end_id_t)0);
				}
			}
		}
		std::optional<FVector> optionalPos2Offset;
		const Block* block2 = blockContainer.getBlock(connectionPreview.pos2);
		if (block2 != nullptr) {
			optionalPos2Offset = blockContainer.getBlockDataManager()
									 .getBlockData(block2->type())
									 ->getConnectionPortOffset(blockContainer.getInputConnectionEnd(connectionPreview.pos2).value().getConnectionId());
		} else {
			for (const TutorialAction::BlockInfo& blockPreview : currentStep.action.blockPreviews) {
				if (connectionPreview.pos2.withinArea(
						blockPreview.pos,
						blockPreview.pos +
							(blockPreview.orientation * blockContainer.getBlockDataManager().getBlockData(blockPreview.type)->getSize()).getLargestVectorInArea()
					)) {
					optionalPos2Offset = blockContainer.getBlockDataManager().getBlockData(blockPreview.type)->getConnectionPortOffset((connection_end_id_t)0);
				}
			}
		}
		if (!optionalPos1Offset.has_value()) {
			optionalPos1Offset = FVector(0.5, 0.5);
		}
		if (!optionalPos2Offset.has_value()) {
			optionalPos2Offset = FVector(0.5, 0.5);
		}
		elementCreator.addConnectionPreview(ConnectionPreview(
			FPosition(connectionPreview.pos1.x, connectionPreview.pos1.y) + optionalPos1Offset.value(),
			FPosition(connectionPreview.pos2.x, connectionPreview.pos2.y) + optionalPos2Offset.value()
		));
	}
	if (currentStep.action.viewData.viewCenter.has_value()) {
		viewManager.setViewCenter(currentStep.action.viewData.viewCenter.value());
		if (currentStep.action.viewData.zoom.has_value()) {
			viewManager.setViewScale(currentStep.action.viewData.zoom.value());
		}
	}
}

void TutorialManager::forceCompleteStep() {
	if (!tutorialRunning) return;
	if (tutorialState >= tutorialSteps.tutorialSteps.size()) return;

	const TutorialStep& currentStep = tutorialSteps.tutorialSteps[tutorialState];
	Circuit* currentCircuit = circuitView.getBackend().getCircuitManager().getCircuit(circuitId);
	if (!currentCircuit) return;
	const BlockContainer& blockContainer = currentCircuit->getBlockContainer();

	for (const TutorialCondition::BlockRequirement& block : currentStep.condition.blocks) {
		const Block* currentBlock = blockContainer.getBlock(block.pos);
		if (currentBlock != nullptr) {
			currentCircuit->tryRemoveBlock(block.pos);
		}
		currentCircuit->tryInsertBlock(block.pos, block.orientation, block.type);
	}

	for (const TutorialCondition::ConnectionRequirement& connection : currentStep.condition.connections) {
		const Block* currentBlock1 = blockContainer.getBlock(connection.pos1);
		const Block* currentBlock2 = blockContainer.getBlock(connection.pos2);
		if (currentBlock1 == nullptr) {
			currentCircuit->tryInsertBlock(connection.pos1, Rotation::ZERO, BlockType::JUNCTION);
		}
		if (currentBlock2 == nullptr) {
			currentCircuit->tryInsertBlock(connection.pos2, Rotation::ZERO, BlockType::JUNCTION);
		}
		currentCircuit->tryCreateConnection(connection.pos1, connection.pos2);
	}

	for (const TutorialCondition::LogicStateRequirement stateCondition : currentStep.condition.logicStates) {
		const Block* currentBlock = blockContainer.getBlock(stateCondition.pos);
		if (currentBlock != nullptr) {
			simulator->setState(Address(stateCondition.pos), stateCondition.state);
		}
	}

	advanceTutorial();
	simulator->tickStep(1);
}