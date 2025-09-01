#include "toolStackInterface.h"
#include "toolStack.h"

void ToolStackInterface::pushTool(std::shared_ptr<CircuitTool> newTool, bool resetTool) {
	toolStack->pushTool(newTool, resetTool);
}

void ToolStackInterface::popAbove(CircuitTool* toolNotToPop) {
	toolStack->popAbove(toolNotToPop);
}

void ToolStackInterface::popTool() {
	toolStack->popTool();
}

void ToolStackInterface::switchToStack(int stack) {
	toolStack->switchToStack(stack);
}
