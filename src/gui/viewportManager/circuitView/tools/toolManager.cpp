#include "toolManager.h"

// tools
#include "placement/blockPlacementTool.h"

void ToolManager::selectBlock(BlockType blockType) {
	SharedBlockPlacementTool blockPlacementTool = std::dynamic_pointer_cast<BlockPlacementTool>(toolStacks[activeToolStack].getCurrentNonHelperTool());
	if (blockPlacementTool) {
		blockPlacementTool->selectBlock(blockType);
	}
}

void ToolManager::selectStack(int stack) {
	if (activeToolStack == stack) return;
	if (activeToolStack != -1) toolStacks[activeToolStack].deactivate();
	activeToolStack = stack;
	toolStacks[activeToolStack].activate();
}

SharedCircuitTool ToolManager::selectTool(SharedCircuitTool tool) {
	if (!tool) return nullptr;
	
	if (activeToolStack != tool->getStackId()) {
		toolStacks[activeToolStack].deactivate();
		activeToolStack = tool->getStackId();
		toolStacks[activeToolStack].activate();
	}
	if (circuit) {
		if (!(circuit->isEditable())) {
			if (tool->canMakeEdits()) {
				toolStacks[activeToolStack].clearTools();
				return nullptr; // Can't select tool that can make edits
			}
		}
	}
	if (!toolStacks[activeToolStack].empty() && toolStacks[activeToolStack].getCurrentNonHelperTool()->getPath() == tool->getPath()) {
		return toolStacks[activeToolStack].getCurrentNonHelperTool();
	}
	toolStacks[activeToolStack].clearTools();
	auto iter = toolInstances.find(tool->getPath());
	if (iter == toolInstances.end()) {
		toolInstances[tool->getPath()] = tool;
		toolStacks[activeToolStack].pushTool(tool);
		return tool;
	} else {
		toolStacks[activeToolStack].pushTool(iter->second);
		return iter->second;
	}
}

void ToolManager::setMode(std::string mode) {
	if (activeToolStack == -1) return;
	toolStacks[activeToolStack].setMode(mode);
}
