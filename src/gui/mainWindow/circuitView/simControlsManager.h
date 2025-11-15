#ifndef simControlsManager_h
#define simControlsManager_h

#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include "backend/dataUpdateEventManager.h"

class CircuitViewWidget;

class SimControlsManager {
public:
	SimControlsManager(Rml::ElementDocument* document, std::shared_ptr<CircuitViewWidget> circuitViewWidget, DataUpdateEventManager& dataUpdateEventManager);
	void update();

private:
	void toggleSimulation();
	void setRealistic();
	void limitSpeed();
	void setTPS();

	bool doSetTPS = false;

	Rml::Element* toggleSimElement;
	Rml::Element* realisticElement;
	Rml::Element* limitSpeedElement;
	Rml::Element* tpsInputElement;
	std::shared_ptr<CircuitViewWidget> circuitViewWidget;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;
};

#endif /* simControlsManager_h */
