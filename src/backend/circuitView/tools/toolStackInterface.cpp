#include "toolStackInterface.h"
#include "toolStack.h"

void ToolStackInterface::pushTool(std::shared_ptr<CircuitTool> newTool) {
	toolStack->pushTool(newTool);
}

void ToolStackInterface::popAbove(CircuitTool* toolNotToPop) {
	toolStack->popAbove(toolNotToPop);
}

void ToolStackInterface::popTool() {
	toolStack->popTool();
}