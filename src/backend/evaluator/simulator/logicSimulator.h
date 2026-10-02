#ifndef logicSimulator_h
#define logicSimulator_h

#include "backend/container/block/blockDefs.h"
#include "backend/container/block/connectionEnd.h"
#include "backend/blockData/blockData.h"
#include "logicState.h"
#include "simulatorConfig.h"
#include "rustSimApi.h"

// C++ owns controls and notifications. Rust owns topology, execution, live state
// and replay. All FFI calls use engineMutex, including reads and edit transactions.
class LogicSimulator {
	friend class SimPauseGuard;
public:
	LogicSimulator(simulator_id_t simulatorId, std::vector<simulator_gate_id_t>& dirtySimulatorIds, DataUpdateEventManager& dataUpdateEventManager);
	~LogicSimulator();
	LogicSimulator(const LogicSimulator&) = delete;
	LogicSimulator& operator=(const LogicSimulator&) = delete;

	double getAverageTickrate() const;
	void setState(simulator_gate_id_t id, logic_state_t state);
	void resetStates();
	logic_state_t getState(simulator_gate_id_t id) const;
	std::vector<logic_state_t> getStates(const std::vector<simulator_gate_id_t>& ids) const;
	std::optional<simulator_gate_id_t> getOutputPortId(simulator_gate_id_t id, connection_end_id_t portId) const;
	BlockData::ConnectionData::PortType getPortType(simulator_gate_id_t id, connection_end_id_t portId) const;

	simulator_gate_id_t addGate(BlockType blockType);
	void removeGate(simulator_gate_id_t id);
	void makeConnection(simulator_gate_id_t sourceId, connection_end_id_t sourcePort, simulator_gate_id_t destinationId, connection_end_id_t destinationPort);
	void removeConnection(simulator_gate_id_t sourceId, connection_end_id_t sourcePort, simulator_gate_id_t destinationId, connection_end_id_t destinationPort);
	void endEdit();
	void waitForSprintComplete();

	bool stepBack();
	bool stepForward();
	bool skipBack();
	bool skipForward();
	bool isViewingReplay() const;
	SimulatorConfig& getSimulatorConfig() { return simulatorConfig; }
	const SimulatorConfig& getSimulatorConfig() const { return simulatorConfig; }
	nlohmann::json dumpState() const;

private:
	SimulatorConfig simulatorConfig;
	CmSimulator* engine;
	std::vector<simulator_gate_id_t>& dirtySimulatorIds;
	std::unordered_map<simulator_gate_id_t, BlockType> gateTypes;
	mutable std::recursive_mutex engineMutex;
	std::mutex controlMutex;
	std::condition_variable controlCv;
	std::thread simulationThread;
	std::atomic<bool> threadRunning { true };
	std::atomic<double> averageTickrate { 0.0 };
	std::atomic<std::uint64_t> controlRevision { 0 };
	void changeConnection(simulator_gate_id_t sourceId, connection_end_id_t sourcePort, simulator_gate_id_t destinationId, connection_end_id_t destinationPort, int delta);
	void simulationLoop();
};

class SimPauseGuard {
public:
	explicit SimPauseGuard(LogicSimulator& sim) : lock(sim.engineMutex) {}
private:
	std::unique_lock<std::recursive_mutex> lock;
};
#endif /* logicSimulator_h */
