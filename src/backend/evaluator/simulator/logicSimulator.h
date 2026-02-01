#ifndef logicSimulator_h
#define logicSimulator_h

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
	enum class PortDirection {
		INPUT,
		OUTPUT,
		BIDDIR
	};
	struct PortInfo {
		PortDirection direction;
		bool limitedToOneConnection;
	};

	using SimulatorGateConnectionContainer = std::unordered_map<connection_end_id_t, std::unordered_map<EvalConnectionPoint, unsigned int>>;

	struct SimulatorGate {
		BlockType type;
		SimulatorGateConnectionContainer connections;

		std::unordered_map<EvalConnectionPoint, unsigned int>& getConnectionsFromPort(connection_end_id_t connectionEndId) {
			return connections[connectionEndId]; // yes, we want to create an empty map if it doesn't exist
		}
	};

	simulator_id_t simulatorId;
	std::vector<simulator_state_index_t>& dirtySimulatorIds;
	DataUpdateEventManager& dataUpdateEventManager;

	std::unordered_map<eval_gate_id, SimulatorGate> gates;

	void removeAllGateConnections(eval_gate_id gateId);

	static PortInfo getPortInfo(BlockType blockType, connection_end_id_t connectionEndId);
	static PortDirection getPortDirection(BlockType blockType, connection_end_id_t connectionEndId) { return getPortInfo(blockType, connectionEndId).direction; }
	static bool isPortLimitedToOneConnection(BlockType blockType, connection_end_id_t connectionEndId) { return getPortInfo(blockType, connectionEndId).limitedToOneConnection; }
};

class SimPauseGuard {
public:
	SimPauseGuard(LogicSimulator& logicSimulator) : logicSimulator(logicSimulator) {}
	~SimPauseGuard() {}

private:
	LogicSimulator& logicSimulator;
};

#endif /* logicSimulator_h */
