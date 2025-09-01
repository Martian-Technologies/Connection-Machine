#include "modeChangerTool.h"

#include "../selectionHelpers/tensorCreationTool.h"
#include "../selectionHelpers/areaCreationTool.h"

const std::vector<std::pair<BlockType, std::string>> types = {
	{BlockType::AND, "And"},
	{BlockType::OR, "Or"},
	{BlockType::XOR, "Xor"},
	{BlockType::NAND, "Nand"},
	{BlockType::NOR, "Nor"},
	{BlockType::XNOR, "Xnor"},
	{BlockType::JUNCTION, "Junction"}
};

void ModeChangerTool::reset() {
	CircuitTool::reset();
	if (!activeSelectionHelper) {
		mode = "Area";
		activeSelectionHelper = std::make_shared<AreaCreationTool>();
	}
	activeSelectionHelper->restart();
	updateElements();
}

void ModeChangerTool::activate() {
	CircuitTool::activate();
	registerFunction("Tool Primary Activate", std::bind(&ModeChangerTool::click, this, std::placeholders::_1));
	registerFunction("Tool Secondary Activate", std::bind(&ModeChangerTool::unclick, this, std::placeholders::_1));
	registerFunction("Tool Rotate Block CW", std::bind(&ModeChangerTool::changeModeUp, this, std::placeholders::_1));
	registerFunction("Tool Rotate Block CCW", std::bind(&ModeChangerTool::changeModeDown, this, std::placeholders::_1));
	if (!activeSelectionHelper->isFinished()) {
		toolStackInterface->pushTool(activeSelectionHelper);
	} else {
		updateElements();
	}
}

void ModeChangerTool::setMode(std::string toolMode) {
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

bool ModeChangerTool::click(const Event* event) {
	if (!activeSelectionHelper->isFinished() || !circuit) return false;
	circuit->setType(activeSelectionHelper->getSelection(), types[type].first);
	reset();
	toolStackInterface->pushTool(activeSelectionHelper);
	return true;
}

bool ModeChangerTool::unclick(const Event* event) {
	if (!activeSelectionHelper->isFinished()) return false;
	elementCreator.clear();
	toolStackInterface->pushTool(activeSelectionHelper, false);
	return true;
}

bool ModeChangerTool::changeModeUp(const Event* event) {
	if (type+1 == types.size()) type = 0;
	else type++;
	updateElements();
	return true;
}

bool ModeChangerTool::changeModeDown(const Event* event) {
	if (type == 0) type = types.size()-1;
	else type--;
	updateElements();
	return true;
}

void ModeChangerTool::updateElements() {
	if (!elementCreator.isSetup()) return;
	elementCreator.clear();
	if (!activeSelectionHelper->isFinished()) return;
	setStatusBar("Left click to set the mode to " + types[type].second+ ". Change mode Q/E.");
	elementCreator.addSelectionElement(SelectionObjectElement(activeSelectionHelper->getSelection(), SelectionObjectElement::RenderMode::SELECTION));
}
