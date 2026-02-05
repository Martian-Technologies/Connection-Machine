#ifndef logicSimulator_h
#define logicSimulator_h

#include "logicGroupRunner.h"

#include "logicState.h"
#include "../evalDefs.h"

class gate_group_id_t;
class gate_index_in_group_t;
class CompiledGateGroup;

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
	void addConnection(const EvalConnection& evalConnection, int weight);
	void removeConnection(const EvalConnection& evalConnection, int weight);
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

	using SimulatorGateConnectionContainer = std::unordered_map<connection_end_id_t, std::unordered_map<EvalConnectionPoint, unsigned int>>;

	struct SimulatorGate {
		BlockType type;
		SimulatorGateConnectionContainer connections;
		std::optional<gate_group_id_t> inputGroupId;
		std::optional<gate_group_id_t> outputGroupId;
		gate_index_in_group_t indexInOutputGroup;

		std::unordered_map<EvalConnectionPoint, unsigned int>& getConnectionsFromPort(connection_end_id_t connectionEndId) {
			return connections[connectionEndId]; // yes, we want to create an empty map if it doesn't exist
		}
	};

	std::unordered_map<eval_gate_id, SimulatorGate> gates;

	class SimulatorGateGroup {
	public:
		SimulatorGateGroup() {}
		~SimulatorGateGroup() = default;
		bool operator==(const SimulatorGateGroup& o) const = default;
	private:
		std::vector<eval_gate_id> gateStatesToCollect;
		std::vector<eval_gate_id> gatesToCompute;
	};

	IdProvider<gate_group_id_t> gateGroupIdProvider { 0 };
	std::unordered_map<gate_group_id_t, SimulatorGateGroup> groups;
	std::unordered_set<gate_group_id_t> dirtyGroups;
	std::unordered_set<eval_gate_id> ungroupedGates;

	LogicGroupRunner logicGroupRunner;

	enum class PortDirection {
		INPUT,
		OUTPUT,
		BIDIRECTIONAL
	};
	struct PortInfo {
		PortDirection direction;
		bool limitedToOneConnection;
	};

	void removeAllGateConnections(eval_gate_id gateId);

	logic_state_t getRunnerState_noMux(simulator_state_index_t simulatorStateIndex) const;

	std::unordered_map<simulator_state_index_t, simulator_state_index_t> runGrouping();

	static PortInfo getPortInfo(BlockType blockType, connection_end_id_t connectionEndId);
	static PortDirection getPortDirection(BlockType blockType, connection_end_id_t connectionEndId) { return getPortInfo(blockType, connectionEndId).direction; }
	static bool isPortLimitedToOneConnection(BlockType blockType, connection_end_id_t connectionEndId) { return getPortInfo(blockType, connectionEndId).limitedToOneConnection; }

	gate_group_id_t makeGroup();
	void deleteGroup(gate_group_id_t groupId);
	void mergeGroups(gate_group_id_t groupId1, gate_group_id_t groupId2);
	void dirtyGroup(gate_group_id_t groupId);
	std::unordered_map<gate_group_id_t, CompiledGateGroup> compileGroups() const;
};

#endif /* logicSimulator_h */
