#include "logicState.h"
#include "../evalDefs.h"

class LogicSimulator {
public:
	LogicSimulator(
		simulator_id_t simulatorId,
		std::vector<simulator_state_index_t>& dirtySimulatorIds,
		DataUpdateEventManager& dataUpdateEventManager
	) : simulatorId(simulatorId),
		dirtySimulatorIds(dirtySimulatorIds),
		dataUpdateEventManager(dataUpdateEventManager) {};

	// circuit editing

	void addGate(eval_gate_id gateId, BlockType blockType);
	void removeGate(eval_gate_id gateId);
	void addConnection(const EvalConnection& evalConnection, unsigned int weight);
	void removeConnection(const EvalConnection& evalConnection, unsigned int weight);
	void endEdit();

	// state access

	void resetStates();
	void setState(simulator_state_index_t simulatorStateIndex, logic_state_t state);
	logic_state_t getState(simulator_state_index_t simulatorStateIndex) const;
	std::vector<logic_state_t> getStates(const std::vector<simulator_state_index_t>& simulatorStateIndices) const;
	std::optional<simulator_state_index_t> getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;

	// controls

	void setRunning(bool running);
	bool isRunning() const;

	void setRealistic(bool realistic);
	bool isRealistic() const;

	void setUseTickrateLimiter(bool useTickrate);
	bool getUseTickrateLimiter() const;
	void setTargetTickrate(double tickrate);
	double getTargetTickrate() const;

	double getAverageTickrate() const;

	void addSprint(unsigned int nTicks);
	unsigned int getSprintCount() const;
	void waitForSprintComplete();

	bool stepBack();
	bool stepForward();
	bool skipBack();
	bool skipForward();
	bool isViewingReplay() const;

	// debug

	nlohmann::json dumpState() const;

private:
	simulator_id_t simulatorId;
	std::vector<simulator_state_index_t>& dirtySimulatorIds;
	DataUpdateEventManager& dataUpdateEventManager;

};

class SimPauseGuard {
public:
	SimPauseGuard(LogicSimulator& logicSimulator) : logicSimulator(logicSimulator) {}

	~SimPauseGuard() {}
private:
	LogicSimulator& logicSimulator;
};
