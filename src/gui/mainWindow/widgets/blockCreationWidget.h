#ifndef blockCreationWidget_h
#define blockCreationWidget_h

#include "gui/mainWindow/widget.h"
#include "backend/blockData/blockData.h"
#include "backend/circuit/circuitDefs.h"
#include "backend/circuit/circuitBlockData.h"
#include "backend/dataUpdateEventManager.h"
#include "gui/viewportManager/circuitView/renderer/elementCreator.h"

class CircuitView;

class BlockCreationWidget : public Widget {
public:
	BlockCreationWidget(WidgetId widgetId, MainWindow& mainWindow, circuit_id_t circuitId = 0);
	~BlockCreationWidget();

	void processEvent(SDL_Event& event) override final;

private:
	void render() override final;
	void update() override final;
	void renderViewport(circuit_id_t circuitId);
	void renderSideBar(circuit_id_t circuitId);

	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;

	BlockType blockType = BlockType::NONE;
	ElementCreator elementCreator;

	circuit_id_t renderingCircuitId = 0;
	std::unique_ptr<CircuitView> circuitView;
	std::mutex circuitsMux;
	std::map<circuit_id_t, std::string> circuits;
	std::mutex blockDataCopyMux;
	std::optional<BlockData::BlockDataCopy> blockDataCopy;
	std::mutex circuitBlockDataCopyMux;
	std::optional<CircuitBlockData::CircuitBlockDataCopy> circuitBlockDataCopy;
};

#endif /* blockCreationWidget_h */
