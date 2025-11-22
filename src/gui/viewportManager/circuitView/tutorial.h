#ifndef tutorialManager_h
#define tutorialManager_h

class CircuitView;
class Environment;

#include "renderer/elementCreator.h"
#include "./viewManager/viewManager.h"
#include "computerAPI/tutorialLoader.h"

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
	struct BlockPreviewInfo {
		Position pos;
		BlockType type;
		Orientation orientation;
	};
	struct ConnectionPreviewInfo {
		Position pos1;
		Position pos2;
	};
	std::vector<std::string> messages;
	std::vector<BlockPreviewInfo> blockPreviews;
	std::vector<ConnectionPreviewInfo> connectionPreviews;
};

struct TutorialStep {
	TutorialCondition condition;
	TutorialAction action;
};

class Tutorial {
public:
	Tutorial(Environment& environment, CircuitView& circuitView);
	void StartTutorial();
	void Stop();
	void setTutorial(const std::vector<TutorialStep>& steps);

private:
	void checkTutorial(DifferenceSharedPtr diff, circuit_id_t circuitId);
	void checkTutorialState(Position pos, logic_state_t state);

	void runCurrentStep();
    bool isCurrentStepComplete() const;
    void advanceTutorial();

	CircuitView* circuitView;
	ElementCreator elementCreator;
	Environment& environment;
	SharedEvaluator evaluator;
	SharedCircuit curentCircuit;
	ViewManager viewManager;

	bool tutorialRunning;
	int tutorialState;
	std::vector<TutorialStep> tutorialSteps;
};

#endif /*tutorialManager_h*/