#ifndef logicSimulator_h
#define logicSimulator_h

#include "groupLinker.h"
#include "logicGroupRunner.h"

#include "simBlockData.h"
#include "simulatorDefs.h"
#include "logicState.h"
#include "../evalDefs.h"

class CompiledGateGroup;

class LogicSimulator {
public:
	LogicSimulator(
		simulator_id_t simulatorId,
		std::vector<simulator_state_reference>& dirtySimulatorIds,
		DataUpdateEventManager& dataUpdateEventManager
	) : simulatorId(simulatorId),
		dirtySimulatorIds(dirtySimulatorIds),
		dataUpdateEventManager(dataUpdateEventManager) {};

	// circuit editing

	void addGate(eval_gate_id gateId, BlockType blockType);
	void removeGate(eval_gate_id gateId);
	void addConnection(const EvalConnection& evalConnection, int weight);
	void removeConnection(const EvalConnection& evalConnection, int weight);
	void endEdit();

	// state access

	void resetStates();
	void setState(simulator_state_reference simulatorStateIndex, logic_state_t state);
	logic_state_t getState(simulator_state_reference simulatorStateIndex) const;
	std::vector<logic_state_t> getStates(const std::vector<simulator_state_reference>& simulatorStateIndices) const;
	std::optional<simulator_state_reference> getSimulatorStateIndex(EvalConnectionPoint evalConnectionPoint) const;

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
	std::vector<simulator_state_reference>& dirtySimulatorIds;
	DataUpdateEventManager& dataUpdateEventManager;

	std::unordered_map<eval_gate_id, SimulatorGate> gates;

	GroupLinker groupLinker;
	LogicGroupRunner logicGroupRunner;

	enum class ConnectionDirection {
		AtoB,
		BtoA,
	};

	void removeAllGateConnections(eval_gate_id gateId);

	logic_state_t getRunnerState_noMux(simulator_state_reference simulatorStateIndex) const;

	std::unordered_map<simulator_state_reference, simulator_state_reference> runGrouping();

	BlockType getBlockType(eval_gate_id gateId) const;
	SimBlockData::PortDirection getConnectionPointDirection(const EvalConnectionPoint& evalConnectionPoint) const;
	ConnectionDirection getConnectionDirection(const EvalConnection& evalConnection) const;

	std::unordered_map<gate_group_id_t, CompiledGateGroup> compileGroups() const;

	bool isJunction(eval_gate_id gateId) const { return isJunction(getBlockType(gateId)); }

	static bool isJunction(BlockType blockType) {
		return (
			blockType == BlockType::JUNCTION ||
			blockType == BlockType::JUNCTION_X ||
			blockType == BlockType::JUNCTION_H ||
			blockType == BlockType::JUNCTION_L
		);
	}
};

#endif /* logicSimulator_h */
