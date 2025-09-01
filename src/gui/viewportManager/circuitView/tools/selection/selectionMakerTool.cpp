#include "selectionMakerTool.h"

#include "../selectionHelpers/tensorCreationTool.h"
#include "../selectionHelpers/areaCreationTool.h"
#include "backend/container/copiedBlocks.h"
#include "backend/backend.h"
#include "../../circuitView.h"

void SelectionMakerTool::reset() {
	CircuitTool::reset();
	if (!activeSelectionHelper) {
		mode = "Area";
		activeSelectionHelper = std::make_shared<AreaCreationTool>();
	}
	activeSelectionHelper->restart();
	updateElements();
}

void SelectionMakerTool::activate() {
	CircuitTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&SelectionMakerTool::click, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&SelectionMakerTool::unclick, this, std::placeholders::_1));
	registerFunction("Copy", std::bind(&SelectionMakerTool::copy, this, std::placeholders::_1));
	if (!activeSelectionHelper->isFinished()) {
		toolStackInterface->pushTool(activeSelectionHelper);
	} else {
		updateElements();
	}
}

void SelectionMakerTool::setMode(std::string toolMode) {
	if (mode != toolMode) {
		if (toolMode == "Area") {
			activeSelectionHelper = std::make_shared<AreaCreationTool>();
			mode = toolMode;
		} else if (toolMode == "Tensor") {
			activeSelectionHelper = std::make_shared<TensorCreationTool>();
			mode = toolMode;
		} else {
			logError("Tool mode \"{}\" could not be found", "", toolMode);
		}
		toolStackInterface->popAbove(this);
	}
}

bool SelectionMakerTool::copy(const Event* event) {
	if (!activeSelectionHelper->isFinished() || !circuit) return false;
	circuitView->getBackend()->setClipboard(std::make_shared<CopiedBlocks>(circuit->getBlockContainer(), activeSelectionHelper->getSelection()));
	return true;
}

bool SelectionMakerTool::click(const Event* event) {
	if (!activeSelectionHelper->isFinished()) return false;
	elementCreator.clear();
	toolStackInterface->pushTool(activeSelectionHelper);
	activeSelectionHelper->sendEvent(event);
	return true;
}

bool SelectionMakerTool::unclick(const Event* event) {
	if (!activeSelectionHelper->isFinished()) return false;
	elementCreator.clear();
	toolStackInterface->pushTool(activeSelectionHelper, false);
	return true;
}

void SelectionMakerTool::updateElements() {
	if (!isActivate || !elementCreator.isSetup()) return;
	elementCreator.clear();
	if (!activeSelectionHelper->isFinished()) return;
	setStatusBar("Left click to select the origin. Right click to cancel. Ctrl-C to copy.");
	elementCreator.addSelectionElement(SelectionObjectElement(activeSelectionHelper->getSelection(), SelectionObjectElement::RenderMode::SELECTION));
}
