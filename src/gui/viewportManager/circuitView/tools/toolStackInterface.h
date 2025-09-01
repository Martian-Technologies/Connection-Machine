#ifndef toolStackInterface_h
#define toolStackInterface_h

class CircuitTool;
class ToolStack;

class ToolStackInterface {
public:
	inline ToolStackInterface(ToolStack* toolStack = nullptr) : toolStack(toolStack) { }

	void pushTool(std::shared_ptr<CircuitTool> newTool, bool resetTool = true);
	void popAbove(CircuitTool* toolNotToPop);
	void popTool();
	void switchToStack(int stack);

private:
	ToolStack* toolStack;
};

#endif /* toolStackInterface_h */
