#ifndef circuitView_h
#define circuitView_h

#include "events/eventRegister.h"
#include "renderer/circuitRenderManager.h"
#include "tools/toolManager.h"
#include "tutorial.h"
#include "viewManager/viewManager.h"
#include "backend/address.h"

#include "gpu/mainRendererDefs.h"

class EvalLogicSimulator;
class Circuit;
class Backend;

class CircuitView {
	friend class Backend;
public:
	CircuitView(Environment& environment, ViewportId viewportId);

	// --------------- Gettters ---------------

	Circuit* getCircuit();
	const Circuit* getCircuit() const;

	EvalLogicSimulator* getSimulator();
	const EvalLogicSimulator* getSimulator() const;

	EventRegister& getEventRegister() { return eventRegister; }
	const EventRegister& getEventRegister() const { return eventRegister; }

	ToolManager& getToolManager() { return toolManager; }
	const ToolManager& getToolManager() const { return toolManager; }

	TutorialManager& getTutorialManager() { return tutorialManager; }
	const TutorialManager& getTutorialManager() const { return tutorialManager; }

	ViewManager& getViewManager() { return viewManager; }
	const ViewManager& getViewManager() const { return viewManager; }

	ViewportId getViewportId() const { return viewportId; }

	Backend& getBackend() { return backend; }
	const Backend& getBackend() const { return backend; }

	const Address& getAddress() const { return address; }

	// void setBackend(Backend* backend);
	void setSimulator(simulator_id_t simulatorId, const Address& address = Address());
	void setSimulator(const EvalLogicSimulator* simulator, const Address& address = Address());
	void setCircuit(circuit_id_t circuitId);

	void viewChanged();

private:
	Environment& environment;
	Backend& backend;

	const ViewportId viewportId;
	circuit_id_t circuitId;
	simulator_id_t simulatorId;
	Address address;

	DataUpdateEventManager& dataUpdateEventManager;

	std::optional<CircuitRenderManager> circuitRenderManager;
	EventRegister eventRegister;
	ViewManager viewManager;
	ToolManager toolManager;
	TutorialManager tutorialManager;
};

#endif /* circuitView_h */
