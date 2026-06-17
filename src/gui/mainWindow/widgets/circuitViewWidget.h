#ifndef circuitViewWidget_h
#define circuitViewWidget_h

#include "../widget.h"
#include "backend/circuit/circuitDefs.h"

class Environment;
class CircuitView;

class CircuitViewWidget : public Widget {
public:
	CircuitViewWidget(WidgetId widgetId, MainWindow& mainWindow);
	~CircuitViewWidget();
	void processEvent(SDL_Event& event) override final;

	circuit_id_t newCircuit();
	CircuitView& getCircuitView() { return *circuitView; }
	const CircuitView& getCircuitView() const { return *circuitView; }

	void save();
	void asSave();
	void load();

private:
	void render() override final;
	void update() override final;
	std::unique_ptr<CircuitView> circuitView;
	int lastToolStack = 0; // used for going back from TreeTraversal
	bool mouseControls;
};

#endif /* circuitViewWidget_h */
