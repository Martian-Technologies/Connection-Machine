#ifndef circuitTool_h
#define circuitTool_h

#include "../renderer/elementCreator.h"
#include "toolStackInterface.h"
#include "../events/eventRegister.h"
#include "backend/circuit/circuit.h"

class Environment;
class CircuitView;
class ToolStack;

typedef std::function<bool(const Event* event)> EventFunction;
typedef unsigned long long EventRegistrationSignature;

class CircuitTool {
	friend class ToolStack;
public:
	CircuitTool(Environment& environment) : environment(environment) {}
	virtual ~CircuitTool() { unregisterFunctions(); }
	bool isHelper() const { return helper; }
	inline virtual std::vector<std::string> getModes() const { return {}; }
	inline virtual std::string getPath() const { return "NONE"; }
	inline virtual unsigned int getStackId() const { return 0; }
	bool sendEvent(const Event* event);
	inline virtual bool showInMenu() const { return !isHelper(); }
	inline virtual bool canMakeEdits() const { return true; };
	Circuit* getCircuit();
	const Circuit* getCircuit() const;

protected:
	void registerFunction(const std::string& eventName, const EventFunction& function);
	void unregisterFunction(const std::string& eventName);
	void unregisterFunctions();

	void sendEventToCircuitView(const Event& event);

	void setStatusBar(const std::string& text);

	virtual void reset() { elementCreator.clear(); }
	virtual void activate();
	virtual void deactivate(bool clearRendering = false) {
		isActivate = false;
		unregisterFunctions();
		setStatusBar("");
		if (clearRendering) elementCreator.clear();
	}

	virtual void setMode(const std::string& toolMode) { }

	virtual void updateElements() { }

	bool pointerInView = false;
	FPosition lastPointerFPosition;
	Position lastPointerPosition;

	bool helper = false;
	bool isActivate = false;

	CircuitView* circuitView;
	ElementCreator elementCreator;
	ToolStackInterface* toolStackInterface;
	Environment& environment;

private:
	// This will also tell the tool to reset.
	void setup(ViewportId viewportId, EventRegister& eventRegister, ToolStackInterface& toolStackInterface, CircuitView& circuitView);
	void unsetup();
	inline void updatedCircuit() { reset(); }

	bool enterBlockView(const Event* event);
	bool exitBlockView(const Event* event);
	bool pointerMove(const Event* event);

	EventRegister* eventRegister;
	std::vector<std::pair<std::string, EventRegistrationSignature>> registeredEvents;
};

typedef std::shared_ptr<CircuitTool> SharedCircuitTool;

#endif /* circuitTool_h */
