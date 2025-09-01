#ifndef toolStack_h
#define toolStack_h

#include "../events/eventRegister.h"
#include "toolStackInterface.h"
#include "circuitTool.h"

#include "gpu/mainRendererDefs.h"

class CircuitView;
class ToolManager;

class ToolStack {
public:
	inline ToolStack(Environment* environment, EventRegister* eventRegister, ViewportId viewportId, CircuitView* circuitView, ToolManager* toolManager) :
		environment(environment), eventRegister(eventRegister), viewportId(viewportId), circuitView(circuitView), toolManager(toolManager), toolStackInterface(this) {
		eventRegister->registerFunction("pointer enter view", std::bind(&ToolStack::enterBlockView, this, std::placeholders::_1));
		eventRegister->registerFunction("pointer exit view", std::bind(&ToolStack::exitBlockView, this, std::placeholders::_1));
		eventRegister->registerFunction("Pointer Move", std::bind(&ToolStack::pointerMove, this, std::placeholders::_1));
	}
	inline ToolStack(const ToolStack& other) = delete;
	inline ToolStack& operator=(const ToolStack& other) = delete;

	void activate();
	void deactivate() { isActive = false; if (!toolStack.empty()) { toolStack.back()->deactivate(); toolStack.back()->elementCreator.clear(); } }

	void reset();
	void pushTool(SharedCircuitTool newTool, bool resetTool = true);
	void popTool();
	void clearTools();
	void popAbove(CircuitTool* toolNotToPop);
	bool empty() const { return toolStack.empty(); }
	void switchToStack(int stack);

	SharedCircuitTool getCurrentNonHelperTool() const;
	SharedCircuitTool getCurrentTool() const;

	void setMode(std::string mode);

	void setCircuit(Circuit* circuit);

private:
	SharedCircuitTool getCurrentNonHelperTool_();
	SharedCircuitTool getCurrentTool_();

	// mouse data for tool when setup
	bool enterBlockView(const Event* event);
	bool exitBlockView(const Event* event);
	bool pointerMove(const Event* event);

	void verifyNoEdits();

	bool pointerInView = false;
	FPosition lastPointerFPosition;
	Position lastPointerPosition;

	// current block container
	Circuit* circuit = nullptr;
	CircuitView* circuitView;

	// tool function event linking
	ToolStackInterface toolStackInterface;
	EventRegister* eventRegister;

	ViewportId viewportId;

	bool isActive = false;
	std::vector<SharedCircuitTool> toolStack;

	ToolManager* toolManager;
	Environment* environment;
};

#endif /* toolStack_h */
