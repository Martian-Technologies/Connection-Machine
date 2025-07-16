#ifndef toolStack_h
#define toolStack_h

#include "backend/evaluator/evaluatorStateInterface.h"
#include "../events/eventRegister.h"
#include "toolStackInterface.h"
#include "../renderer/renderer.h"
#include "circuitTool.h"

class CircuitView;
class ToolManager;

class ToolStack {
public:
	inline ToolStack(EventRegister* eventRegister, Renderer* renderer, CircuitView* circuitView, ToolManager* toolManager) :
		eventRegister(eventRegister), renderer(renderer), circuitView(circuitView), toolManager(toolManager), toolStackInterface(this) {
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
	void setEvaluatorStateInterface(EvaluatorStateInterface* evaluatorStateInterface);

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

	Renderer* renderer;
	EvaluatorStateInterface* evaluatorStateInterface;

	bool isActive = false;
	std::vector<SharedCircuitTool> toolStack;

	ToolManager* toolManager;
};

#endif /* toolStack_h */
