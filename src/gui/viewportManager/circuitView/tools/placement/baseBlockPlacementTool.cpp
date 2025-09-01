#include "baseBlockPlacementTool.h"

void BaseBlockPlacementTool::activate() {
	CircuitTool::activate();
	registerFunction("Tool Rotate Block CW", std::bind(&BaseBlockPlacementTool::rotateBlockCW, this, std::placeholders::_1));
	registerFunction("Tool Rotate Block CCW", std::bind(&BaseBlockPlacementTool::rotateBlockCCW, this, std::placeholders::_1));
}

bool BaseBlockPlacementTool::rotateBlockCW(const Event* event) {
	orientation.nextOrientation();
	updateElements();
	return true;
}

bool BaseBlockPlacementTool::rotateBlockCCW(const Event* event) {
	orientation.lastOrientation();
	updateElements();
	return true;
}