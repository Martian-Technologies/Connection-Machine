#include "connectionTool.h"

#include "tensorConnectTool.h"
#include "singleConnectTool.h"

ConnectionTool::ConnectionTool(const Environment& environment) : CircuitTool(environment) {
	activeConnectionTool = std::make_shared<SingleConnectTool>(environment);
}

void ConnectionTool::activate() {
	if (activeConnectionTool) {
		toolStackInterface->pushTool(activeConnectionTool);
	}
}

void ConnectionTool::setMode(const std::string& mode) {
	if (this->mode != mode) {
		SharedCircuitTool newActiveConnectionTool;
		if (mode == "Single") {
			newActiveConnectionTool = std::make_shared<SingleConnectTool>(environment);
			this->mode = mode;
		} else if (mode == "Tensor") {
			newActiveConnectionTool = std::make_shared<TensorConnectTool>(environment);
			this->mode = mode;
		} else {
			logError("Tool mode \"{}\" could not be found", "", mode);
			return;
		}
		activeConnectionTool = newActiveConnectionTool;
		toolStackInterface->popAbove(this);
	}
}
