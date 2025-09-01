#include "tensorConnectTool.h"

#include "../selectionHelpers/tensorCreationTool.h"

void TensorConnectTool::reset() {
	CircuitTool::reset();
	placingOutout = true;
	if (!activeOutputSelectionHelper) activeOutputSelectionHelper = std::make_shared<TensorCreationTool>();
	activeOutputSelectionHelper->restart();
	if (!activeInputSelectionHelper) activeInputSelectionHelper = std::make_shared<TensorCreationTool>();
	activeInputSelectionHelper->restart();
	updateElements();
}

void TensorConnectTool::activate() {
	CircuitTool::activate();
	// registerFunction("Tool Primary Activate", std::bind(&TensorConnectTool::click, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&TensorConnectTool::unclick, this, std::placeholders::_1));
	registerFunction("Tool Invert Mode", std::bind(&TensorConnectTool::invertMode, this, std::placeholders::_1));
	registerFunction("Confirm", std::bind(&TensorConnectTool::confirm, this, std::placeholders::_1));
	if (placingOutout && activeOutputSelectionHelper->isFinished()) {
		placingOutout = false;
		updateElements();
		activeInputSelectionHelper->setSelectionToFollow(activeOutputSelectionHelper->getSelection());
		toolStackInterface->pushTool(activeInputSelectionHelper, false);
	} else if (!activeOutputSelectionHelper->isFinished()) {
		updateElements();
		toolStackInterface->pushTool(activeOutputSelectionHelper, false);
	} else if (activeInputSelectionHelper->isFinished()) {
		updateElements();
	} else {
		placingOutout = true;
		activeOutputSelectionHelper->undoFinished();
		updateElements();
		toolStackInterface->pushTool(activeOutputSelectionHelper, false);
	}
}

bool TensorConnectTool::unclick(const Event* event) {
	if (activeInputSelectionHelper->isFinished()) {
		activeInputSelectionHelper->undoFinished();
		updateElements();
		toolStackInterface->pushTool(activeInputSelectionHelper, false);
	} else if (activeOutputSelectionHelper->isFinished()) {
		activeOutputSelectionHelper->undoFinished();
		updateElements();
		toolStackInterface->pushTool(activeOutputSelectionHelper, false);
	} else {
		updateElements();
		toolStackInterface->pushTool(activeOutputSelectionHelper, false);
	}
	return true;
}

bool TensorConnectTool::confirm(const Event* event) {
	if (!circuit) return false;
	if (!sameSelectionShape(activeOutputSelectionHelper->getSelection(), activeInputSelectionHelper->getSelection())) return false;
	if (doingDisconnect) circuit->tryRemoveConnection(activeOutputSelectionHelper->getSelection(), activeInputSelectionHelper->getSelection());
	else circuit->tryCreateConnection(activeOutputSelectionHelper->getSelection(), activeInputSelectionHelper->getSelection());
	reset();
	toolStackInterface->pushTool(activeOutputSelectionHelper, false);
	return true;
}

bool TensorConnectTool::invertMode(const Event* event) {
	if (!circuit) return false;
	doingDisconnect = !doingDisconnect;
	updateElements();
	return true;
}

void TensorConnectTool::updateElements() {
	if (!isActivate || !elementCreator.isSetup()) return;
	elementCreator.clear();
	if (!activeOutputSelectionHelper->isFinished()) return;
	elementCreator.addSelectionElement(SelectionObjectElement(activeOutputSelectionHelper->getSelection(), SelectionObjectElement::RenderMode::ARROWS));
	if (!activeInputSelectionHelper->isFinished()) return;
	if (doingDisconnect) setStatusBar("Press E to confirm disconnection.");
	else setStatusBar("Press E to confirm connection.");
	elementCreator.addSelectionElement(SelectionObjectElement(activeInputSelectionHelper->getSelection(), SelectionObjectElement::RenderMode::ARROWS));
}
