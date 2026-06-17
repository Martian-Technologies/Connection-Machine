#include "baseBlockPlacementTool.h"

void BaseBlockPlacementTool::activate() {
	CircuitTool::activate();
	registerFunction("Tool Rotate Block CW", std::bind(&BaseBlockPlacementTool::rotateBlockCW, this, std::placeholders::_1));
	registerFunction("Tool Rotate Block CCW", std::bind(&BaseBlockPlacementTool::rotateBlockCCW, this, std::placeholders::_1));
	registerFunction("Tool Flip Block", std::bind(&BaseBlockPlacementTool::flipBlock, this, std::placeholders::_1));
}

bool BaseBlockPlacementTool::rotateBlockCW(const Event* event) {
	orientation.nextRotation();
	updateElements();
	return true;
}

bool BaseBlockPlacementTool::rotateBlockCCW(const Event* event) {
	orientation.lastRotation();
	updateElements();
	return true;
}

bool BaseBlockPlacementTool::flipBlock(const Event* event) {
	orientation.flip();
	updateElements();
	return true;
}
