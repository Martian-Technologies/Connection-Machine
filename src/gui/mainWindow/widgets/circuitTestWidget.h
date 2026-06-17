#ifndef circuitTestWidget_h
#define circuitTestWidget_h

#include "backend/container/block/blockDefs.h"
#include "gui/mainWindow/widget.h"
#include "backend/circuit/circuitDefs.h"
#include "backend/dataUpdateEventManager.h"
#include "backend/circuitTests/circuitTestGroup.h"
#include "backend/circuitTests/circuitTestGroupRunner.h"
#include <optional>


class CircuitView;

class CircuitTestWidget : public Widget {
public:
	CircuitTestWidget(WidgetId widgetId, MainWindow& mainWindow);
	~CircuitTestWidget();

	void processEvent(SDL_Event& event) override final;

private:
	void render() override final;
	void update() override final;
	void renderViewport(BlockType blockType, const std::string& testGroupName);
	void renderSideBar(BlockType blockType, const std::string& testGroupName);

	int lastToolStack = 0; // used for going back from TreeTraversal
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReceiver;

	BlockType blockType = BlockType::NONE;

	std::mutex renderingCircuitMux;
	circuit_id_t renderingCircuitId = 0;
	std::unique_ptr<CircuitView> circuitView;
	std::mutex blockTypesMux;
	std::map<BlockType, std::string> blockTypes;
	std::mutex testGroupsMux;
	std::vector<std::string> testGroups;
	std::mutex testGroupCopyMux;
	CircuitTestGroup::CircuitTestGroupCopy testGroupCopy;
	std::optional<CircuitTestGroupRunner> testGroupRunner;
	std::mutex testResultsMutex;
	std::vector<CircuitTestGroupRunner::TestRunData> testRunData;
};

#endif /* circuitTestWidget_h */
