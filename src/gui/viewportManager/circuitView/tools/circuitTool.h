#ifndef circuitTool_h
#define circuitTool_h

#include "../renderer/elementCreator.h"
#include "backend/circuit/circuit.h"
#include "toolStackInterface.h"
#include "../events/eventRegister.h"

class Environment;
class CircuitView;
class ToolStack;

typedef std::function<bool(const Event* event)> EventFunction;
typedef unsigned long long EventRegistrationSignature;

class CircuitTool {
	friend class ToolStack;
public:
	void setEnvironment(const Environment* environment) { this->environment = environment; }
	virtual ~CircuitTool() { unregisterFunctions(); }
	bool isHelper() const { return helper; }
	inline virtual std::vector<std::string> getModes() const { return {}; }
	inline virtual std::string getPath() const { return "NONE"; }
	inline virtual unsigned int getStackId() const { return 0; }
	bool sendEvent(const Event* event);
	inline virtual bool showInMenu() const { return !isHelper(); }
	inline virtual bool canMakeEdits() const { return true; };

protected:
	void registerFunction(std::string eventName, EventFunction function);
	void unregisterFunction(std::string eventName);
	void unregisterFunctions();

	void setStatusBar(const std::string& text);

	virtual void reset() { elementCreator.clear(); }
	virtual void activate();
	virtual void deactivate() { isActivate = false; unregisterFunctions(); }

	virtual void setMode(std::string toolMode) { }

	virtual void updateElements() { }

	bool pointerInView = false;
	FPosition lastPointerFPosition;
	Position lastPointerPosition;

	Circuit* circuit = nullptr;
	bool helper = false;
	bool isActivate = false;

	CircuitView* circuitView;
	ElementCreator elementCreator;
	ToolStackInterface* toolStackInterface;
	const Environment* environment;

private:
	// This will also tell the tool to reset.
	void setup(ViewportId viewportId, EventRegister* eventRegister, ToolStackInterface* toolStackInterface, CircuitView* circuitView, Circuit* circuit);
	void unsetup();
	inline void setCircuit(Circuit* circuit) { this->circuit = circuit; reset(); }

	bool enterBlockView(const Event* event);
	bool exitBlockView(const Event* event);
	bool pointerMove(const Event* event);

	EventRegister* eventRegister;
	std::vector<std::pair<std::string, EventRegistrationSignature>> registeredEvents;
};

typedef std::shared_ptr<CircuitTool> SharedCircuitTool;

#endif /* circuitTool_h */
