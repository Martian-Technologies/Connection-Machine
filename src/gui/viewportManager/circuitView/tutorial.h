#ifndef tutorialManager_h
#define tutorialManager_h

#include "./viewManager/viewManager.h"
#include "backend/container/difference.h"
#include "renderer/elementCreator.h"
#include <string>
#include <unordered_map>

class CircuitView;
class Environment;

struct TutorialCondition {
	struct BlockRequirement {
		Position pos;
		BlockType type;
		Orientation orientation;
	};
	struct ConnectionRequirement {
		Position pos1;
		Position pos2;
	};
	struct LogicStateRequirement {
		Position pos;
		logic_state_t state;
		int numSteps;
	};
	std::vector<BlockRequirement> blocks;
	std::vector<ConnectionRequirement> connections;
	std::vector<LogicStateRequirement> logicStates;
};

struct TutorialAction {
	struct BlockInfo {
		Position pos;
		BlockType type;
		Orientation orientation;
	};
	struct ConnectionPreviewInfo {
		Position pos1;
		Position pos2;
	};
	struct Message {
		FPosition pos;
		float scale;
		std::string message;
	};
	struct View {
		std::optional<FPosition> viewCenter;
		std::optional<float> zoom;
	};
	View viewData;
	std::vector<Message> messages;
	std::vector<BlockInfo> blockPreviews;
	std::vector<BlockInfo> blocks;
	std::vector<ConnectionPreviewInfo> connectionPreviews;
};

struct TutorialStep {
	TutorialCondition condition;
	TutorialAction action;
};

struct Tutorial {
    std::vector<TutorialStep> tutorialSteps;
    std::unordered_map<std::string, std::string> info;
};

class TutorialManager {
public:
	TutorialManager(Environment& environment, CircuitView& circuitView);
	void StartTutorial();
	void stop();
	void setTutorial(const Tutorial& tutorial);
	void forceCompleteStep();

private:
	void checkTutorial(DifferenceSharedPtr diff, circuit_id_t circuitId);
	void checkTutorialState(Position pos, logic_state_t state);

	void runCurrentStep();
	bool isCurrentStepComplete() const;
    void midStepChecks();
	void advanceTutorial();

	CircuitView& circuitView;
	ElementCreator elementCreator;
	Environment& environment;
	EvalLogicSimulator* simulator;
	circuit_id_t circuitId;
	ViewManager& viewManager;
	DataUpdateEventManager::DataUpdateEventReceiver dataUpdateEventReciever;

	bool tutorialRunning;
	int tutorialState;
    Tutorial tutorialSteps;
};

#endif /* tutorialManager_h */
