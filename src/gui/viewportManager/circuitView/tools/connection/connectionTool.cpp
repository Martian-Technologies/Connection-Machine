#include "connectionTool.h"

#include "tensorConnectTool.h"
#include "singleConnectTool.h"

ConnectionTool::ConnectionTool() {
	activeConnectionTool = std::make_shared<SingleConnectTool>();
}

void ConnectionTool::activate() {
	if (activeConnectionTool) {
		toolStackInterface->pushTool(activeConnectionTool);
	}
}

void ConnectionTool::setMode(std::string toolMode) {
	if (mode != toolMode) {
		SharedCircuitTool newActiveConnectionTool;
		if (toolMode == "Single") {
			newActiveConnectionTool = std::make_shared<SingleConnectTool>();
		} else if (toolMode == "Tensor") {
			newActiveConnectionTool = std::make_shared<TensorConnectTool>();
		} else {
			logError("Tool mode \"{}\" could not be found", "", toolMode);
			return;
		}
		activeConnectionTool = newActiveConnectionTool;
		toolStackInterface->popAbove(this);
	}
}
